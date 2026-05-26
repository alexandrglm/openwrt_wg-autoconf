/**
 * wg-setup.c, from `setup_wireguard()` / `remove_wireguard()`
 * Autonomous usage:
 *   wg-setup setup <conf_name> [manual]
 *   wg-setup remove <wg_iface> [silent]
 *
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define VERSION "1.0.0-r1"
#define MAX_LINE 4096
#define MAX_CMD 4096
#define WG_CONF_DIR "/etc/wireguard"
#define NETWORK_CONF "/etc/config/network"
#define DHCP_CONF "/etc/config/dhcp"
#define FIREWALL_CONF "/etc/config/firewall"
#define STATE_FILE "/usr/libexec/wg-autoconf/states"
#define ATOMIC_PATHS "/usr/libexec/wg-autoconf/atomics"
#define DEFAULT_DNS "1.1.1.1, 1.0.0.1"
#define DEFAULT_PORT "51820"
#define DEFAULT_ALLOW_IPS "0.0.0.0/0"

/* UI SHITTIES */
#define COLOR_RED     "\033[1;31m"
#define COLOR_GREEN   "\033[0;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_BLUE    "\033[0;34m"
#define COLOR_NC      "\033[0m"

/* STRUCT FOR CONFIG BOLLOCKS */
typedef struct {
    char *private_key;
    char *public_key;
    char *address;
    char *endpoint_host;
    char *endpoint_port;
    char *allowed_ips;
    char *dns;
} WireGuardConfig;

static void log_error(const char *fmt, ...) {
    va_list args;
    fprintf(stderr, COLOR_RED "[ERROR]" COLOR_NC " ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

static void log_warning(const char *fmt, ...) {
    va_list args;
    fprintf(stderr, COLOR_YELLOW "[WARNING]" COLOR_NC " ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

static void log_success(const char *fmt, ...) {
    va_list args;
    fprintf(stderr, COLOR_GREEN "[OK]" COLOR_NC " ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

static void log_info(const char *fmt, ...) {
    va_list args;
    fprintf(stderr, COLOR_BLUE "[wg-setup]" COLOR_NC " ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

/* CMD PARSER */
static int run_cmd(const char *cmd) {
    return system(cmd);
}

/* 1.   GET VALUE FROM .CONF FILE */
static char* get_conf_value(const char *key, const char *filename) {
    FILE *fp;
    char line[MAX_LINE];
    char *value = NULL;
    size_t key_len;

    if (!key || !filename) return NULL;

    fp = fopen(filename, "r");
    if (!fp) return NULL;

    key_len = strlen(key);

    while (fgets(line, sizeof(line), fp)) {
        char *p = line;

        while (isspace((unsigned char)*p)) p++;

        if (strncmp(p, key, key_len) != 0) continue;

        p += key_len;
        if (*p != '=') continue;
        p++;

        while (isspace((unsigned char)*p)) p++;

        char *end = p + strlen(p) - 1;
        while (end > p && isspace((unsigned char)*end)) end--;
        *(end + 1) = '\0';

        value = strdup(p);
        break;
    }

    fclose(fp);
    return value;
}

/* 2.   VALIDATE IPv4 ADDRESS */
static int validate_ipv4(const char *ip) {
    int octet;
    int dots = 0;
    const char *p = ip;

    if (!ip || !*ip) return 0;

    while (*p) {
        if (*p == '.') {
            dots++;
            p++;
            continue;
        }
        if (!isdigit((unsigned char)*p)) return 0;

        octet = 0;
        while (isdigit((unsigned char)*p)) {
            octet = octet * 10 + (*p - '0');
            if (octet > 255) return 0;
            p++;
        }
    }
    return (dots == 3);
}

/* 3.   VALIDATE CIDR NOTATION */
static int validate_cidr(const char *cidr_str, int is_ipv6) {
    char *endptr;
    long cidr;
    int max_cidr = is_ipv6 ? 128 : 32;

    if (!cidr_str || *cidr_str == '\0') return 0;
    cidr = strtol(cidr_str, &endptr, 10);
    if (*endptr != '\0') return 0;
    return (cidr >= 0 && cidr <= max_cidr);
}

/* 4.   VALIDATE INTERFACE NAME */
static int validate_iface_name(const char *iface) {
    int len;
    const char *p;

    if (!iface || strlen(iface) == 0) return 0;
    len = strlen(iface);
    if (len > 15) return 0;
    if (strncmp(iface, "wg", 2) != 0) return 0;
    if (strcmp(iface, "wg") == 0) return 0;

    for (p = iface; *p; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-') return 0;
    }
    return 1;
}

/* 5.   BACKUP CONFIG FILE */
static int backup_config_file(const char *file) {
    char backup_file[MAX_LINE];
    char cmd[MAX_CMD];

    snprintf(backup_file, sizeof(backup_file), "%s.BACKUP_PRE_WIREGUARD", file);

    if (access(backup_file, F_OK) == 0) {
        return 0;
    }

    snprintf(cmd, MAX_CMD, "cp %s %s 2>/dev/null", file, backup_file);
    return run_cmd(cmd);
}

/* 6.   RESTORE ALL BACKUPS */
static int restore_backups(void) {
    char cmd[MAX_CMD];
    int ret = 0;

    snprintf(cmd, MAX_CMD, "[ -f %s.BACKUP_PRE_WIREGUARD ] && cp %s.BACKUP_PRE_WIREGUARD %s 2>/dev/null",
             NETWORK_CONF, NETWORK_CONF, NETWORK_CONF);
    if (run_cmd(cmd) == 0) ret++;

    snprintf(cmd, MAX_CMD, "[ -f %s.BACKUP_PRE_WIREGUARD ] && cp %s.BACKUP_PRE_WIREGUARD %s 2>/dev/null",
             DHCP_CONF, DHCP_CONF, DHCP_CONF);
    if (run_cmd(cmd) == 0) ret++;

    snprintf(cmd, MAX_CMD, "[ -f %s.BACKUP_PRE_WIREGUARD ] && cp %s.BACKUP_PRE_WIREGUARD %s 2>/dev/null",
             FIREWALL_CONF, FIREWALL_CONF, FIREWALL_CONF);
    if (run_cmd(cmd) == 0) ret++;

    return (ret == 3) ? 0 : 1;
}

/* 7.   GET NEXT TAG ID FROM CONFIG FILE */
static int get_next_tag_id(const char *file, const char *tag) {
    char cmd[MAX_CMD];
    char output[256];
    FILE *fp;
    int max_id = 0;

    snprintf(cmd, MAX_CMD, "grep '# wg-autoconf %s start id' %s 2>/dev/null | sed 's/.*id //g' | sort -n | tail -1", tag, file);

    fp = popen(cmd, "r");
    if (fp) {
        if (fgets(output, sizeof(output), fp)) {
            max_id = atoi(output);
        }
        pclose(fp);
    }

    return max_id + 1;
}

/* 8.   SETUP NETWORK UCI CONFIG */
static int setup_network_config(const char *iface_name, WireGuardConfig *config) {
    int tag_id;
    char cmd[MAX_CMD * 2];
    char temp_file[MAX_LINE];
    char allowed_ips_clean[MAX_LINE];
    char *p;

    tag_id = get_next_tag_id(NETWORK_CONF, "network");

    snprintf(temp_file, sizeof(temp_file), "%s/network.atomic.%d", ATOMIC_PATHS, tag_id);

    strncpy(allowed_ips_clean, config->allowed_ips, sizeof(allowed_ips_clean) - 1);
    allowed_ips_clean[sizeof(allowed_ips_clean) - 1] = '\0';
    for (p = allowed_ips_clean; *p; p++) {
        if (*p == ',') *p = ' ';
    }

    snprintf(cmd, sizeof(cmd),
             "cp %s %s 2>/dev/null && "
             "cat >> %s << 'EOF'\n"
             "\n"
             "# wg-autoconf network start id %d\n"
             "config interface '%s'\n"
             "    option proto 'wireguard'\n"
             "    option private_key '%s'\n"
             "    option addresses '%s'\n"
             "    option dns '%s'\n"
             "\n"
             "config wireguard_%s '%s_peer'\n"
             "    option public_key '%s'\n"
             "    option endpoint_host '%s'\n"
             "    option endpoint_port '%s'\n"
             "    option route_allowed_ips '0'\n"
             "    option persistent_keepalive '25'\n"
             "    list allowed_ips '%s'\n"
             "# wg-autoconf network end id %d\n"
             "EOF\n"
             "mv %s %s 2>/dev/null && "
             "uci commit network 2>/dev/null",
             NETWORK_CONF, temp_file,
             temp_file,
             tag_id, iface_name, config->private_key, config->address, config->dns,
             iface_name, iface_name, config->public_key,
             config->endpoint_host, config->endpoint_port, allowed_ips_clean,
             tag_id, temp_file, NETWORK_CONF);

    return run_cmd(cmd);
}

/* 9.   SETUP DHCP UCI CONFIG */
static int setup_dhcp_config(const char *iface_name) {
    int tag_id;
    char cmd[MAX_CMD * 2];
    char temp_file[MAX_LINE];

    tag_id = get_next_tag_id(DHCP_CONF, "dhcp");

    snprintf(temp_file, sizeof(temp_file), "%s/dhcp.atomic.%d", ATOMIC_PATHS, tag_id);

    snprintf(cmd, sizeof(cmd),
             "cp %s %s 2>/dev/null && "
             "cat >> %s << 'EOF'\n"
             "\n"
             "# wg-autoconf dhcp start id %d\n"
             "config dhcp '%s'\n"
             "    option interface '%s'\n"
             "    option ignore '1'\n"
             "# wg-autoconf dhcp end id %d\n"
             "EOF\n"
             "mv %s %s 2>/dev/null && "
             "uci commit dhcp 2>/dev/null",
             DHCP_CONF, temp_file,
             temp_file,
             tag_id, iface_name, iface_name,
             tag_id, temp_file, DHCP_CONF);

    return run_cmd(cmd);
}

/* 10.  SETUP FIREWALL UCI CONFIG */
static int setup_firewall_config(const char *iface_name, const char *endpoint_port) {
    int tag_id;
    char cmd[MAX_CMD * 2];
    char temp_file[MAX_LINE];

    tag_id = get_next_tag_id(FIREWALL_CONF, "firewall");

    snprintf(temp_file, sizeof(temp_file), "%s/firewall.atomic.%d", ATOMIC_PATHS, tag_id);

    snprintf(cmd, sizeof(cmd),
             "cp %s %s 2>/dev/null && "
             "cat >> %s << 'EOF'\n"
             "\n"
             "# wg-autoconf firewall start id %d\n"
             "config zone\n"
             "    option name '%s'\n"
             "    option input 'ACCEPT'\n"
             "    option output 'ACCEPT'\n"
             "    option forward 'ACCEPT'\n"
             "    option masq '1'\n"
             "    option mtu_fix '1'\n"
             "    list network '%s'\n"
             "\n"
             "config rule\n"
             "    option name 'Allow-WireGuard-%s'\n"
             "    option src 'wan'\n"
             "    option dest_port '%s'\n"
             "    option proto 'udp'\n"
             "    option target 'ACCEPT'\n"
             "\n"
             "config forwarding\n"
             "    option src '%s'\n"
             "    option dest 'wan'\n"
             "# wg-autoconf firewall end id %d\n"
             "EOF\n"
             "mv %s %s 2>/dev/null && "
             "uci commit firewall 2>/dev/null && "
             "/etc/init.d/firewall reload 2>/dev/null",
             FIREWALL_CONF, temp_file,
             temp_file,
             tag_id, iface_name, iface_name, iface_name, endpoint_port, iface_name,
             tag_id, temp_file, FIREWALL_CONF);

    return run_cmd(cmd);
}

/* 11.  STATE -> ADD IFACE */
static int state_add_iface(const char *iface_name) {
    char cmd[MAX_CMD * 2];

    snprintf(cmd, sizeof(cmd),
             "mkdir -p %s 2>/dev/null; "
             "if [ ! -f %s ]; then "
             "  echo 'IS_INSTALLED=1' > %s; "
             "  echo 'IS_FIRST_EXEC=1' >> %s; "
             "  echo 'IS_PREV_TO_UPGRADE=0' >> %s; "
             "  echo 'IS_UPGRADED=0' >> %s; "
             "fi; "
             "NEXT_ID=$(grep '^ID_.*_NAME=' %s 2>/dev/null | sed 's/^ID_\\([0-9]*\\)_NAME.*/\\1/' | sort -n | tail -1); "
             "NEXT_ID=$((NEXT_ID + 1)); "
             "echo 'ID_${NEXT_ID}_NAME=%s' >> %s; "
             "echo 'ID_${NEXT_ID}_IS_CREATED=1' >> %s; "
             "echo 'ID_${NEXT_ID}_IS_ACTIVE=0' >> %s; "
             "echo 'ID_${NEXT_ID}_IS_RT_TABLES_IN_USE=0' >> %s",
             ATOMIC_PATHS, STATE_FILE, STATE_FILE, STATE_FILE, STATE_FILE, STATE_FILE,
             STATE_FILE, iface_name, STATE_FILE, STATE_FILE, STATE_FILE, STATE_FILE);

    return run_cmd(cmd);
}

/* 12.  STATE -> REMOVE IFACE */
static int state_remove_iface(const char *iface_name) {
    char cmd[MAX_CMD];

    snprintf(cmd, sizeof(cmd),
             "if [ -f %s ]; then "
             "  ID=$(grep '^ID_.*_NAME=%s$' %s 2>/dev/null | sed 's/^ID_\\([0-9]*\\)_NAME.*/\\1/'); "
             "  if [ -n \"$ID\" ]; then "
             "    sed -i '/^ID_${ID}_/d' %s; "
             "  fi; "
             "fi",
             STATE_FILE, iface_name, STATE_FILE, STATE_FILE);

    return run_cmd(cmd);
}

/* 13.  SETUP WIREGUARD (MAIN) */
static int setup_wireguard(const char *conf_name, const char *mode) {
    char conf_file[MAX_LINE];
    WireGuardConfig config = {0};
    char iface_name[MAX_LINE];
    char check_cmd[MAX_CMD];
    int is_manual = (mode && strcmp(mode, "manual") == 0);

    snprintf(conf_file, sizeof(conf_file), "%s/%s.conf", WG_CONF_DIR, conf_name);

    log_info("Setting up from %s.conf...", conf_name);

    if (!is_manual && access(conf_file, F_OK) != 0) {
        log_error("Configuration file not found: %s", conf_file);
        return 1;
    }

    if (!is_manual) {
        config.private_key = get_conf_value("PrivateKey", conf_file);
        config.public_key = get_conf_value("PublicKey", conf_file);
        config.address = get_conf_value("Address", conf_file);

        char *endpoint = get_conf_value("Endpoint", conf_file);
        if (endpoint) {
            char *colon = strchr(endpoint, ':');
            if (colon) {
                *colon = '\0';
                config.endpoint_host = strdup(endpoint);
                config.endpoint_port = strdup(colon + 1);
            } else {
                config.endpoint_host = strdup(endpoint);
                config.endpoint_port = strdup(DEFAULT_PORT);
            }
            free(endpoint);
        } else {
            config.endpoint_host = strdup("");
            config.endpoint_port = strdup(DEFAULT_PORT);
        }

        config.allowed_ips = get_conf_value("AllowedIPs", conf_file);
        if (!config.allowed_ips) {
            config.allowed_ips = strdup(DEFAULT_ALLOW_IPS);
        }

        config.dns = get_conf_value("DNS", conf_file);
        if (!config.dns) {
            config.dns = strdup(DEFAULT_DNS);
        }

        if (!config.private_key || !config.public_key || !config.address) {
            log_error("Missing required fields in config file");
            return 1;
        }

        snprintf(iface_name, sizeof(iface_name), "wg_%s", conf_name);

    } else {
        log_info("Manual mode - interactive configuration");
        log_warning("Interactive mode not fully implemented in C yet");
        log_warning("Falling back to shell for manual setup");
        return 1;
    }

    if (!validate_iface_name(iface_name)) {
        log_error("Invalid interface name: %s", iface_name);
        return 1;
    }

    snprintf(check_cmd, sizeof(check_cmd), "uci get network.%s >/dev/null 2>&1", iface_name);
    if (run_cmd(check_cmd) == 0) {
        log_error("Interface %s already exists", iface_name);
        return 1;
    }

    backup_config_file(NETWORK_CONF);
    backup_config_file(DHCP_CONF);
    backup_config_file(FIREWALL_CONF);

    if (setup_network_config(iface_name, &config) != 0) {
        log_error("Failed to setup network config");
        return 1;
    }

    if (setup_dhcp_config(iface_name) != 0) {
        log_warning("Failed to setup DHCP config");
    }

    if (setup_firewall_config(iface_name, config.endpoint_port) != 0) {
        log_warning("Failed to setup firewall config");
    }

    run_cmd("uci set dhcp.@dnsmasq[0].filter_aaaa='1' 2>/dev/null");
    run_cmd("uci commit dhcp 2>/dev/null");
    run_cmd("/etc/init.d/dnsmasq restart >/dev/null 2>&1");

    state_add_iface(iface_name);

    log_success("Done!");
    printf("\n");
    printf("%s=== WireGuard CONFIG ===%s\n", COLOR_GREEN, COLOR_NC);
    printf("  %s- Interface:%s          %s%s%s\n", COLOR_BLUE, COLOR_NC, COLOR_YELLOW, iface_name, COLOR_NC);
    printf("  %s- Local IP:%s           %s%s%s\n", COLOR_BLUE, COLOR_NC, COLOR_YELLOW, config.address, COLOR_NC);
    printf("  %s- Endpoint:%s           %s%s:%s%s\n", COLOR_BLUE, COLOR_NC, COLOR_YELLOW,
           config.endpoint_host ? config.endpoint_host : "N/A",
           config.endpoint_port ? config.endpoint_port : "N/A", COLOR_NC);
    printf("  %s- DNS:%s                %s%s%s\n", COLOR_BLUE, COLOR_NC, COLOR_YELLOW, config.dns, COLOR_NC);
    printf("\n");
    printf("%s=== FINISH SETUP ===%s\n", COLOR_GREEN, COLOR_NC);
    printf("%s1.%s %sENABLE WG:%s         %swg-autoconf up %s%s\n", COLOR_GREEN, COLOR_NC, COLOR_BLUE, COLOR_NC, COLOR_YELLOW, iface_name, COLOR_NC);
    printf("%s2.%s %sVERIFY:%s            %swg show %s%s\n", COLOR_GREEN, COLOR_NC, COLOR_BLUE, COLOR_NC, COLOR_YELLOW, iface_name, COLOR_NC);
    printf("%s3.%s %sTEST WG:%s           %sping -I %s 8.8.8.8%s\n", COLOR_GREEN, COLOR_NC, COLOR_BLUE, COLOR_NC, COLOR_YELLOW, iface_name, COLOR_NC);
    printf("%s4.%s %sCHECK WG IP:%s       %sping --interface %s ifconfig.me%s\n", COLOR_GREEN, COLOR_NC, COLOR_BLUE, COLOR_NC, COLOR_YELLOW, iface_name, COLOR_NC);

    return 0;
}

/* 14.  REMOVE WIREGUARD (MAIN) */
static int remove_wireguard(const char *iface, const char *silent) {
    char cmd[MAX_CMD * 2];
    char other_ifaces[256];
    FILE *fp;
    int remaining;
    int is_silent = (silent && strcmp(silent, "silent") == 0);

    if (!is_silent) {
        log_info("Removing %s...", iface);
    }

    snprintf(cmd, sizeof(cmd), "uci get network.%s >/dev/null 2>&1", iface);
    if (run_cmd(cmd) != 0) {
        if (!is_silent) {
            log_success("%s removed (was not in UCI)", iface);
        }
        return 0;
    }

    if (!validate_iface_name(iface)) {
        log_error("Invalid interface name: %s", iface);
        return 1;
    }

    snprintf(cmd, sizeof(cmd), "ifdown %s >/dev/null 2>&1", iface);
    run_cmd(cmd);

    /* REMOVE NETWORK BLOCK */
    snprintf(cmd, sizeof(cmd),
             "LINE=$(grep -n \"config interface '%s'\" %s 2>/dev/null | cut -d: -f1); "
             "if [ -n \"$LINE\" ]; then "
             "  TAG_ID=$(sed -n \"1,${LINE}p\" %s | grep '# wg-autoconf network start id' | tail -1 | sed 's/.*id //g'); "
             "  if [ -n \"$TAG_ID\" ]; then "
             "    START=$(grep -n \"# wg-autoconf network start id $TAG_ID\" %s | cut -d: -f1); "
             "    END=$(grep -n \"# wg-autoconf network end id $TAG_ID\" %s | cut -d: -f1); "
             "    if [ -n \"$START\" ] && [ -n \"$END\" ]; then "
             "      sed -i \"${START},${END}d\" %s; "
             "    fi; "
             "  fi; "
             "fi",
             iface, NETWORK_CONF, NETWORK_CONF, NETWORK_CONF, NETWORK_CONF, NETWORK_CONF);
    run_cmd(cmd);

    /* REMOVE DHCP BLOCK */
    snprintf(cmd, sizeof(cmd),
             "LINE=$(grep -n \"config dhcp '%s'\" %s 2>/dev/null | cut -d: -f1); "
             "if [ -n \"$LINE\" ]; then "
             "  TAG_ID=$(sed -n \"1,${LINE}p\" %s | grep '# wg-autoconf dhcp start id' | tail -1 | sed 's/.*id //g'); "
             "  if [ -n \"$TAG_ID\" ]; then "
             "    START=$(grep -n \"# wg-autoconf dhcp start id $TAG_ID\" %s | cut -d: -f1); "
             "    END=$(grep -n \"# wg-autoconf dhcp end id $TAG_ID\" %s | cut -d: -f1); "
             "    if [ -n \"$START\" ] && [ -n \"$END\" ]; then "
             "      sed -i \"${START},${END}d\" %s; "
             "    fi; "
             "  fi; "
             "fi",
             iface, DHCP_CONF, DHCP_CONF, DHCP_CONF, DHCP_CONF, DHCP_CONF);
    run_cmd(cmd);

    /* REMOVE FIREWALL BLOCK */
    snprintf(cmd, sizeof(cmd),
             "LINE=$(grep -n \"option name '%s'\" %s 2>/dev/null | head -1 | cut -d: -f1); "
             "if [ -n \"$LINE\" ]; then "
             "  TAG_ID=$(sed -n \"1,${LINE}p\" %s | grep '# wg-autoconf firewall start id' | tail -1 | sed 's/.*id //g'); "
             "  if [ -n \"$TAG_ID\" ]; then "
             "    START=$(grep -n \"# wg-autoconf firewall start id $TAG_ID\" %s | cut -d: -f1); "
             "    END=$(grep -n \"# wg-autoconf firewall end id $TAG_ID\" %s | cut -d: -f1); "
             "    if [ -n \"$START\" ] && [ -n \"$END\" ]; then "
             "      sed -i \"${START},${END}d\" %s; "
             "    fi; "
             "  fi; "
             "fi",
             iface, FIREWALL_CONF, FIREWALL_CONF, FIREWALL_CONF, FIREWALL_CONF, FIREWALL_CONF);
    run_cmd(cmd);

    /* CHECK REMAINING INTERFACES */
    fp = popen("uci show network 2>/dev/null | grep -c 'network\\.wg[^=]*=interface' || echo 0", "r");
    remaining = 0;
    if (fp) {
        if (fgets(other_ifaces, sizeof(other_ifaces), fp)) {
            remaining = atoi(other_ifaces);
        }
        pclose(fp);
    }

    if (remaining <= 1) {
        restore_backups();
        if (!is_silent) {
            log_info("Configuration restored from backups");
        }
    } else if (!is_silent) {
        log_info("Backups preserved (as other interfaces exist)");
    }

    run_cmd("uci commit network 2>/dev/null");
    run_cmd("uci commit dhcp 2>/dev/null");
    run_cmd("uci commit firewall 2>/dev/null");
    run_cmd("/etc/init.d/network reload 2>/dev/null");
    run_cmd("/etc/init.d/firewall reload 2>/dev/null");

    state_remove_iface(iface);

    if (!is_silent) {
        log_success("%s removed successfully!", iface);
    }

    return 0;
}

/* main */
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: wg-setup {setup|remove} [args...]\n");
        fprintf(stderr, "\nCommands:\n");
        fprintf(stderr, "  setup <conf_name>              - Setup from .conf file\n");
        fprintf(stderr, "  setup <conf_name> manual       - Manual setup (fallback to shell)\n");
        fprintf(stderr, "  remove <wg_iface>              - Remove WireGuard interface\n");
        fprintf(stderr, "  remove <wg_iface> silent       - Remove silently (for nuke)\n");
        return 1;
    }

    if (strcmp(argv[1], "setup") == 0) {
        if (argc < 3) {
            log_error("Usage: wg-setup setup <conf_name> [manual]");
            return 1;
        }
        const char *mode = (argc >= 4) ? argv[3] : NULL;
        return setup_wireguard(argv[2], mode);

    } else if (strcmp(argv[1], "remove") == 0) {
        if (argc < 3) {
            log_error("Usage: wg-setup remove <wg_iface> [silent]");
            return 1;
        }
        const char *silent = (argc >= 4) ? argv[3] : NULL;
        return remove_wireguard(argv[2], silent);

    } else {
        log_error("Unknown command: %s", argv[1]);
        fprintf(stderr, "Valid commands: setup, remove\n");
        return 1;
    }
}
