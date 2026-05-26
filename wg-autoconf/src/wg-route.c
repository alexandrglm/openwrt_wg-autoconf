/**
 * wg-route.c - WireGuard Route Manager
 *
 * Autonomous usage:
 *   wg-route set <wg_iface> <lan_iface>
 *   wg-route unset <wg_iface> <lan_iface>
 *   wg-route show [wg_iface]
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
#define UCI_NETWORK "/etc/config/network"
#define UCI_FIREWALL "/etc/config/firewall"
#define RT_TABLES "/etc/iproute2/rt_tables"
#define ATOMIC_PATHS "/usr/libexec/wg-autoconf/atomics"
#define STATE_FILE "/usr/libexec/wg-autoconf/states"

/* UI SHITTIES */
#define COLOR_RED     "\033[1;31m"
#define COLOR_GREEN   "\033[0;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_BLUE    "\033[0;34m"
#define COLOR_NC      "\033[0m"

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
    fprintf(stderr, COLOR_BLUE "[wg-route]" COLOR_NC " ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

/* CMD PARSER */
static int run_cmd(const char *cmd) {
    return system(cmd);
}

/* 1.   GET LAN IP */
static char* get_lan_ip(const char *lan_iface) {
    static char ip[64];
    char cmd[MAX_CMD];
    FILE *fp;

    snprintf(cmd, MAX_CMD, "uci get network.%s.ipaddr 2>/dev/null", lan_iface);
    fp = popen(cmd, "r");
    if (fp) {
        if (fgets(ip, sizeof(ip), fp)) {
            char *nl = strchr(ip, '\n');
            if (nl) *nl = '\0';
            pclose(fp);
            return ip;
        }
        pclose(fp);
    }

    snprintf(cmd, MAX_CMD, "ip -4 addr show %s 2>/dev/null | grep inet | awk '{print $2}' | cut -d/ -f1 | head -1", lan_iface);
    fp = popen(cmd, "r");
    if (fp) {
        if (fgets(ip, sizeof(ip), fp)) {
            char *nl = strchr(ip, '\n');
            if (nl) *nl = '\0';
            pclose(fp);
            return ip;
        }
        pclose(fp);
    }

    return NULL;
}

/* 2.   GET LAN NETMASK */
static char* get_lan_netmask(const char *lan_iface) {
    static char netmask[64];
    char cmd[MAX_CMD];
    FILE *fp;

    snprintf(cmd, MAX_CMD, "uci get network.%s.netmask 2>/dev/null", lan_iface);
    fp = popen(cmd, "r");
    if (fp) {
        if (fgets(netmask, sizeof(netmask), fp)) {
            char *nl = strchr(netmask, '\n');
            if (nl) *nl = '\0';
            pclose(fp);
            return netmask;
        }
        pclose(fp);
    }

    return "255.255.255.0";
}

/* 3.   GET LAN DEVICE */
static char* get_lan_device(const char *lan_iface) {
    static char device[64];
    char cmd[MAX_CMD];
    FILE *fp;

    snprintf(cmd, MAX_CMD, "uci get network.%s.device 2>/dev/null", lan_iface);
    fp = popen(cmd, "r");
    if (fp) {
        if (fgets(device, sizeof(device), fp)) {
            char *nl = strchr(device, '\n');
            if (nl) *nl = '\0';
            pclose(fp);
            return device;
        }
        pclose(fp);
    }

    snprintf(device, sizeof(device), "%s", lan_iface);
    return device;
}

/* 4.   GET WG DNS */
static char* get_wg_dns(const char *wg_iface) {
    static char dns[64];
    char cmd[MAX_CMD];
    FILE *fp;

    snprintf(cmd, MAX_CMD, "uci get network.%s.dns 2>/dev/null | tr -d '\"'", wg_iface);
    fp = popen(cmd, "r");
    if (fp) {
        if (fgets(dns, sizeof(dns), fp)) {
            char *nl = strchr(dns, '\n');
            if (nl) *nl = '\0';
            pclose(fp);
            if (strlen(dns) > 0) {
                return dns;
            }
        }
        pclose(fp);
    }
    return NULL;
}

/* 5.   NETMASK -> CIDR */
static int netmask_to_cidr(const char *netmask) {
    if (strcmp(netmask, "255.255.255.0") == 0) return 24;
    if (strcmp(netmask, "255.255.254.0") == 0) return 23;
    if (strcmp(netmask, "255.255.252.0") == 0) return 22;
    if (strcmp(netmask, "255.255.0.0") == 0) return 16;
    if (strcmp(netmask, "255.0.0.0") == 0) return 8;
    return 24;
}

/* 6.   CALCULATE SUBNET */
static void calculate_subnet(const char *ip, int cidr, char *subnet, size_t size) {
    char ip_copy[64];
    char *dot;

    strncpy(ip_copy, ip, sizeof(ip_copy) - 1);
    ip_copy[sizeof(ip_copy) - 1] = '\0';

    switch (cidr) {
        case 24:
            dot = strrchr(ip_copy, '.');
            if (dot) *dot = '\0';
            snprintf(subnet, size, "%s.0/%d", ip_copy, cidr);
        break;
        case 16:
            dot = strrchr(ip_copy, '.');
            if (dot) *dot = '\0';
            dot = strrchr(ip_copy, '.');
        if (dot) *dot = '\0';
        snprintf(subnet, size, "%s.0.0/%d", ip_copy, cidr);
        break;
        case 8:
            dot = strchr(ip_copy, '.');
            if (dot) *dot = '\0';
            snprintf(subnet, size, "%s.0.0.0/%d", ip_copy, cidr);
        break;
        default:
            snprintf(subnet, size, "%s/%d", ip, cidr);
    }
}

/* 7.   GET TABLE NAME */
static void get_table_name(const char *wg_iface, const char *lan_iface, char *table_name, size_t size) {
    snprintf(table_name, size, "_vpn_%s_%s", wg_iface, lan_iface);
}

/* 8.   TABLE EXISTS? */
static int table_exists(const char *table_name) {
    char cmd[MAX_CMD];
    snprintf(cmd, MAX_CMD, "grep -q '^[0-9]\\+[[:space:]]\\+%s$' %s 2>/dev/null", table_name, RT_TABLES);
    return (run_cmd(cmd) == 0);
}

/* 9.   GET OR CREATE TABLE ID */
static int get_or_create_table_id(const char *table_name) {
    char cmd[MAX_CMD];
    FILE *fp;
    char output[32];
    long table_id;

    snprintf(cmd, MAX_CMD, "grep '^[0-9]\\+[[:space:]]\\+%s$' %s 2>/dev/null | awk '{print $1}'", table_name, RT_TABLES);
    fp = popen(cmd, "r");
    if (fp) {
        if (fgets(output, sizeof(output), fp)) {
            char *nl = strchr(output, '\n');
            if (nl) *nl = '\0';
            pclose(fp);
            if (strlen(output) > 0) {
                table_id = strtol(output, NULL, 10);
                return (int)table_id;
            }
        }
        pclose(fp);
    }

    for (table_id = 150; table_id < 250; table_id++) {
        snprintf(cmd, MAX_CMD, "grep -q '^%ld[[:space:]]' %s 2>/dev/null", table_id, RT_TABLES);
        if (run_cmd(cmd) != 0) {
            char atomic_file[MAX_CMD];
            snprintf(atomic_file, MAX_CMD, "%s/rt_tables.atomic.%ld", ATOMIC_PATHS, table_id);
            snprintf(cmd, MAX_CMD, "cp %s %s 2>/dev/null && echo '%ld %s' >> %s && mv %s %s 2>/dev/null",
                     RT_TABLES, atomic_file, table_id, table_name, atomic_file, atomic_file, RT_TABLES);
            if (run_cmd(cmd) == 0) {
                return (int)table_id;
            }
        }
    }

    return -1;
}

/* 10.  REMOVE TABLE */
static int remove_table(const char *table_name) {
    char cmd[MAX_CMD];
    char atomic_file[MAX_CMD];

    if (!table_exists(table_name)) {
        return 0;
    }

    snprintf(atomic_file, MAX_CMD, "%s/rt_tables.unset.$$", ATOMIC_PATHS);
    snprintf(cmd, MAX_CMD, "grep -v '^[0-9]\\+[[:space:]]\\+%s$' %s > %s && mv %s %s 2>/dev/null",
             table_name, RT_TABLES, atomic_file, atomic_file, RT_TABLES);

    return run_cmd(cmd);
}

/* 11.  ADD ROUTING RULES */
static int add_routing_rules(const char *subnet, const char *table_name, const char *wg_iface, const char *lan_device) {
    char cmd[MAX_CMD];
    int ret = 0;

    snprintf(cmd, MAX_CMD, "ip rule add from %s lookup %s 2>/dev/null", subnet, table_name);
    if (run_cmd(cmd) != 0) {
        log_warning("Failed to add routing rule for %s", subnet);
        ret = 1;
    }

    snprintf(cmd, MAX_CMD, "ip rule add from %s to %s lookup main 2>/dev/null", subnet, subnet);
    if (run_cmd(cmd) != 0) {
        log_warning("Failed to add local rule");
        ret = 1;
    }

    snprintf(cmd, MAX_CMD, "ip route add %s dev %s table %s 2>/dev/null", subnet, lan_device, table_name);
    if (run_cmd(cmd) != 0) {
        log_warning("Failed to add local route for %s", subnet);
        ret = 1;
    }

    snprintf(cmd, MAX_CMD, "ip link show %s >/dev/null 2>&1", wg_iface);
    if (run_cmd(cmd) == 0) {
        snprintf(cmd, MAX_CMD, "ip route add default dev %s table %s 2>/dev/null", wg_iface, table_name);
        if (run_cmd(cmd) != 0) {
            log_warning("Failed to add default route via %s", wg_iface);
            ret = 1;
        }
    } else {
        log_warning("%s is not active! Routes may not work until interface is up.", wg_iface);
    }

    return ret;
}

/* 12.  REMOVE ROUTING RULES */
static int remove_routing_rules(const char *subnet, const char *table_name) {
    char cmd[MAX_CMD];

    snprintf(cmd, MAX_CMD, "ip rule del from %s lookup %s 2>/dev/null", subnet, table_name);
    run_cmd(cmd);

    snprintf(cmd, MAX_CMD, "ip rule del from %s to %s lookup main 2>/dev/null", subnet, subnet);
    run_cmd(cmd);

    snprintf(cmd, MAX_CMD, "ip route flush table %s 2>/dev/null", table_name);
    run_cmd(cmd);

    return 0;
}

/* 13.  ADD FIREWALL RULES */
static int add_firewall_rules(const char *wg_iface, const char *lan_iface) {
    char cmd[MAX_CMD];

    snprintf(cmd, MAX_CMD, "grep -q 'config forwarding.*src.*%s.*dest.*%s' %s 2>/dev/null", lan_iface, wg_iface, UCI_FIREWALL);
    if (run_cmd(cmd) == 0) {
        return 0;
    }

    snprintf(cmd, MAX_CMD,
             "cat >> %s << 'EOF'\n"
             "\n"
             "config forwarding\n"
             "    option src '%s'\n"
             "    option dest '%s'\n"
             "\n"
             "config forwarding\n"
             "    option src '%s'\n"
             "    option dest '%s'\n"
             "EOF\n"
             "uci commit firewall 2>/dev/null && /etc/init.d/firewall reload 2>/dev/null",
             UCI_FIREWALL, lan_iface, wg_iface, wg_iface, lan_iface);

    if (run_cmd(cmd) != 0) {
        log_warning("Failed to add firewall forwarding rules");
        return 1;
    }

    log_info("Firewall forwarding rules added");
    return 0;
}

/* 14.  REMOVE FIREWALL RULES */
static int remove_firewall_rules(const char *wg_iface, const char *lan_iface) {
    char cmd[MAX_CMD];
    char temp_file[MAX_CMD];

    snprintf(temp_file, MAX_CMD, "%s/firewall.clean.$$", ATOMIC_PATHS);

    snprintf(cmd, MAX_CMD,
             "grep -v 'option src .*%s.*\\n.*option dest .*%s' %s | "
             "grep -v 'option src .*%s.*\\n.*option dest .*%s' > %s && "
             "mv %s %s 2>/dev/null",
             lan_iface, wg_iface, UCI_FIREWALL, wg_iface, lan_iface, temp_file, temp_file, UCI_FIREWALL);

    run_cmd(cmd);

    run_cmd("uci commit firewall 2>/dev/null");
    run_cmd("/etc/init.d/firewall reload 2>/dev/null");

    return 0;
}

/* 15.  ADD DNS REDIRECT */
static int add_dns_redirect(const char *wg_iface, const char *lan_iface) {
    char cmd[MAX_CMD * 2];
    char *tunnel_dns;
    char redirect_name[128];

    tunnel_dns = get_wg_dns(wg_iface);
    if (!tunnel_dns) {
        log_warning("No DNS configured for %s, cannot setup DNS redirect", wg_iface);
        return 1;
    }

    snprintf(redirect_name, sizeof(redirect_name), "Redirect_DNS_%s_to_%s", lan_iface, wg_iface);

    snprintf(cmd, MAX_CMD, "uci show firewall 2>/dev/null | grep -q \"redirect.*name='%s'\"", redirect_name);
    if (run_cmd(cmd) == 0) {
        log_info("DNS redirect already exists for %s", lan_iface);
        return 0;
    }

    snprintf(cmd, MAX_CMD,
             "uci add firewall redirect > /dev/null && "
             "uci set firewall.@redirect[-1].name='%s' && "
             "uci set firewall.@redirect[-1].src='%s' && "
             "uci set firewall.@redirect[-1].proto='tcp udp' && "
             "uci set firewall.@redirect[-1].src_dport='53' && "
             "uci set firewall.@redirect[-1].dest_ip='%s' && "
             "uci set firewall.@redirect[-1].dest_port='53' && "
             "uci set firewall.@redirect[-1].target='DNAT' && "
             "uci commit firewall && "
             "/etc/init.d/firewall reload 2>/dev/null",
             redirect_name, lan_iface, tunnel_dns);

    if (run_cmd(cmd) != 0) {
        log_warning("Failed to add DNS redirect for %s", lan_iface);
        return 1;
    }

    log_info("DNS redirect added: %s -> %s", lan_iface, tunnel_dns);
    return 0;
}

/* 16.  REMOVE DNS REDIRECT */
static int remove_dns_redirect(const char *wg_iface, const char *lan_iface) {
    char cmd[MAX_CMD];
    char redirect_name[128];
    char output[32];
    FILE *fp;

    snprintf(redirect_name, sizeof(redirect_name), "Redirect_DNS_%s_to_%s", lan_iface, wg_iface);

    snprintf(cmd, MAX_CMD, "uci show firewall 2>/dev/null | grep -F '%s' | head -1 | cut -d'[' -f2 | cut -d']' -f1", redirect_name);

    fp = popen(cmd, "r");
    if (fp) {
        if (fgets(output, sizeof(output), fp)) {
            char *nl = strchr(output, '\n');
            if (nl) *nl = '\0';
            if (strlen(output) > 0 && output[0] >= '0' && output[0] <= '9') {
                snprintf(cmd, MAX_CMD, "uci -q delete firewall.@redirect[%s] && uci commit firewall && /etc/init.d/firewall reload 2>/dev/null", output);
                run_cmd(cmd);
                log_info("DNS redirect removed for %s", lan_iface);
                return 0;
            }
        }
        pclose(fp);
    }

    log_warning("DNS redirect not found for %s -> %s", lan_iface, wg_iface);
    return 0;
}

/* 17.  UPDATE STATE */
static void update_state(const char *wg_iface, int is_in_use) {
    char cmd[MAX_CMD];

    snprintf(cmd, MAX_CMD,
             "STATE_FILE=\"%s\"; "
             "if [ -f \"$STATE_FILE\" ]; then "
             "  ID=$(grep \"^ID_.*_NAME=%s$\" \"$STATE_FILE\" 2>/dev/null | sed 's/^ID_\\([0-9]*\\)_NAME.*/\\1/'); "
             "  if [ -n \"$ID\" ]; then "
             "    sed -i \"s/^ID_${ID}_IS_RT_TABLES_IN_USE=.*/ID_${ID}_IS_RT_TABLES_IN_USE=%d/\" \"$STATE_FILE\" 2>/dev/null || "
             "    echo \"ID_${ID}_IS_RT_TABLES_IN_USE=%d\" >> \"$STATE_FILE\"; "
             "  fi; "
             "fi",
             STATE_FILE, wg_iface, is_in_use, is_in_use);

    run_cmd(cmd);
}

/* 18.  SET LAN ROUTES (MAIN) */
static int set_lan_routes(const char *wg_iface, const char *lan_iface) {
    char *lan_ip;
    char *lan_netmask;
    char *lan_device;
    int cidr;
    char subnet[128];
    char table_name[256];
    int table_id;

    if (!wg_iface || !lan_iface) {
        log_error("WireGuard and LAN interface names required");
        return 1;
    }

    log_info("Setting up routes for %s through %s...", lan_iface, wg_iface);

    lan_ip = get_lan_ip(lan_iface);
    if (!lan_ip) {
        log_error("Could not determine IP address for %s", lan_iface);
        return 1;
    }

    lan_netmask = get_lan_netmask(lan_iface);
    cidr = netmask_to_cidr(lan_netmask);
    calculate_subnet(lan_ip, cidr, subnet, sizeof(subnet));

    log_info("LAN subnet: %s", subnet);

    lan_device = get_lan_device(lan_iface);
    log_info("LAN device: %s", lan_device);

    get_table_name(wg_iface, lan_iface, table_name, sizeof(table_name));

    table_id = get_or_create_table_id(table_name);
    if (table_id == -1) {
        log_error("Cannot find available routing table ID");
        return 1;
    }

    log_info("Using routing table: %s (ID: %d)", table_name, table_id);

    if (add_routing_rules(subnet, table_name, wg_iface, lan_device) != 0) {
        log_warning("Some routing rules may not have been added correctly");
    }

    add_firewall_rules(wg_iface, lan_iface);
    add_dns_redirect(wg_iface, lan_iface);
    update_state(wg_iface, 1);

    log_success("Routes configured successfully!");
    return 0;
}

/* 19.  UNSET LAN ROUTES (MAIN) */
static int unset_lan_routes(const char *wg_iface, const char *lan_iface) {
    char *lan_ip;
    char *lan_netmask;
    int cidr;
    char subnet[128];
    char table_name[256];
    char cmd[MAX_CMD];
    char remaining[32];
    int remaining_tables;

    if (!wg_iface || !lan_iface) {
        log_error("WireGuard and LAN interface names required");
        return 1;
    }

    log_info("Removing routes for %s (via %s)...", lan_iface, wg_iface);

    lan_ip = get_lan_ip(lan_iface);
    if (!lan_ip) {
        log_error("Could not determine IP address for %s", lan_iface);
        return 1;
    }

    lan_netmask = get_lan_netmask(lan_iface);
    cidr = netmask_to_cidr(lan_netmask);
    calculate_subnet(lan_ip, cidr, subnet, sizeof(subnet));

    get_table_name(wg_iface, lan_iface, table_name, sizeof(table_name));

    if (!table_exists(table_name)) {
        log_error("No routes configured for %s <-> %s (table '%s' not found!)", wg_iface, lan_iface, table_name);
        return 1;
    }

    log_info("Route table: %s", table_name);
    log_info("Subnet: %s", subnet);

    remove_routing_rules(subnet, table_name);
    remove_firewall_rules(wg_iface, lan_iface);
    remove_dns_redirect(wg_iface, lan_iface);
    remove_table(table_name);

    snprintf(cmd, MAX_CMD, "grep -c '^[0-9]\\+[[:space:]]\\+_vpn_%s_' %s 2>/dev/null || echo 0", wg_iface, RT_TABLES);
    FILE *fp = popen(cmd, "r");
    if (fp) {
        if (fgets(remaining, sizeof(remaining), fp)) {
            remaining_tables = atoi(remaining);
            update_state(wg_iface, (remaining_tables > 0) ? 1 : 0);
        } else {
            update_state(wg_iface, 0);
        }
        pclose(fp);
    } else {
        update_state(wg_iface, 0);
    }

    log_success("%s routes (via %s) successfully removed!", lan_iface, wg_iface);
    return 0;
}

/* 20.  SHOW ROUTES */
static int show_routes(const char *filter_wg_iface) {
    char cmd[MAX_CMD];

    if (filter_wg_iface && strlen(filter_wg_iface) > 0) {
        log_info("Routes for %s:", filter_wg_iface);
        snprintf(cmd, MAX_CMD, "grep '_vpn_%s' %s 2>/dev/null | while read line; do "
        "table_id=$(echo \"$line\" | awk '{print $1}'); "
        "table_name=$(echo \"$line\" | awk '{print $2}'); "
        "lan_iface=$(echo \"$table_name\" | sed 's/_vpn_%s//g'); "
        "echo \"$lan_iface:\"; "
        "echo \"  Table: $table_name (ID: $table_id)\"; "
        "ip rule show | grep \"lookup $table_name\" | sed 's/^/  Rule: /'; "
        "ip route show table $table_name 2>/dev/null | while read route; do "
        "echo \"  Route: $route\"; done; echo \"\"; done",
        filter_wg_iface, RT_TABLES, filter_wg_iface);
    } else {
        log_info("All routes:");
        snprintf(cmd, MAX_CMD, "grep '_vpn_' %s 2>/dev/null | while read line; do "
        "table_id=$(echo \"$line\" | awk '{print $1}'); "
        "table_name=$(echo \"$line\" | awk '{print $2}'); "
        "echo \"$table_name (ID: $table_id):\"; "
        "ip rule show | grep \"lookup $table_name\" | sed 's/^/  Rule: /'; "
        "ip route show table $table_name 2>/dev/null | while read route; do "
        "echo \"  Route: $route\"; done; echo \"\"; done", RT_TABLES);
    }

    run_cmd(cmd);
    return 0;
}

/* main */
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: wg-route {set|unset|show} [args...]\n");
        fprintf(stderr, "  set <wg_iface> <lan_iface>     - Add routing rules\n");
        fprintf(stderr, "  unset <wg_iface> <lan_iface>   - Remove routing rules\n");
        fprintf(stderr, "  show [wg_iface]                - Show routing rules\n");
        return 1;
    }

    if (strcmp(argv[1], "set") == 0) {
        if (argc != 4) {
            log_error("Usage: wg-route set <wg_iface> <lan_iface>");
            return 1;
        }
        return set_lan_routes(argv[2], argv[3]);

    } else if (strcmp(argv[1], "unset") == 0) {
        if (argc != 4) {
            log_error("Usage: wg-route unset <wg_iface> <lan_iface>");
            return 1;
        }
        return unset_lan_routes(argv[2], argv[3]);

    } else if (strcmp(argv[1], "show") == 0) {
        const char *filter = (argc >= 3) ? argv[2] : NULL;
        return show_routes(filter);

    } else {
        log_error("Unknown command: %s", argv[1]);
        return 1;
    }
}
