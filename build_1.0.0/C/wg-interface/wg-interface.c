/**
 * wg-interface.c, from activate_interface() / deactivate_interface()
 *
 * Autonomous usage:
 *   wg-interface up <wg_iface>
 *   wg-interface down <wg_iface>
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
#define MAX_CMD 4096
#define STATE_FILE "/usr/libexec/wg-autoconf/states"
#define ATOMIC_PATHS "/usr/libexec/wg-autoconf/atomics"

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
    fprintf(stderr, COLOR_BLUE "[wg-interface]" COLOR_NC " ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}



/* CMD PARSER */
static int run_cmd(const char *cmd) {
    return system(cmd);
}


/* 1.   UCI IFACE EXISTS */
static int uci_iface_exists(const char *iface) {
    char cmd[MAX_CMD];
    snprintf(cmd, MAX_CMD, "uci get network.%s >/dev/null 2>&1", iface);
    return (run_cmd(cmd) == 0);
}

/* 2.   VALIDATE IFACE */
static int validate_iface_name(const char *iface) {
    const char *p;
    int len;

    if (!iface || strlen(iface) == 0) {
        return 0;
    }

    len = strlen(iface);
    if (len > 15) {
        return 0;
    }

    if (strncmp(iface, "wg", 2) != 0) {
        return 0;
    }

    if (strcmp(iface, "wg") == 0) {
        return 0;
    }

    for (p = iface; *p; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-') {
            return 0;
        }
    }

    return 1;
}

/* 3.   STATE -> GET ID IFACE */
static char* state_get_id(const char *iface_name) {
    static char id[16];
    char cmd[MAX_CMD];
    FILE *fp;

    snprintf(cmd, MAX_CMD, "grep \"^ID_.*_NAME=%s$\" %s 2>/dev/null | sed 's/^ID_\\([0-9]*\\)_NAME.*/\\1/'", iface_name, STATE_FILE);

    fp = popen(cmd, "r");
    if (fp) {
        if (fgets(id, sizeof(id), fp)) {
            char *newline = strchr(id, '\n');
            if (newline) *newline = '\0';
            pclose(fp);
            return id;
        }
        pclose(fp);
    }

    return NULL;
}


/* 4.   STATE -> ADD IFACE */
static int state_add_iface(const char *iface_name) {
    char cmd[MAX_CMD];

    snprintf(cmd, MAX_CMD,
             "STATE_FILE=\"%s\"; "
             "if [ ! -f \"$STATE_FILE\" ]; then "
             "  echo \"IS_INSTALLED=1\" > \"$STATE_FILE\"; "
             "  echo \"IS_FIRST_EXEC=1\" >> \"$STATE_FILE\"; "
             "  echo \"IS_PREV_TO_UPGRADE=0\" >> \"$STATE_FILE\"; "
             "  echo \"IS_UPGRADED=0\" >> \"$STATE_FILE\"; "
             "fi; "
             "NEXT_ID=$(grep \"^ID_.*_NAME=\" \"$STATE_FILE\" 2>/dev/null | sed 's/^ID_\\([0-9]*\\)_NAME.*/\\1/' | sort -n | tail -1); "
             "NEXT_ID=$((NEXT_ID + 1)); "
             "echo \"ID_${NEXT_ID}_NAME=%s\" >> \"$STATE_FILE\"; "
             "echo \"ID_${NEXT_ID}_IS_CREATED=0\" >> \"$STATE_FILE\"; "
             "echo \"ID_${NEXT_ID}_IS_ACTIVE=0\" >> \"$STATE_FILE\"; "
             "echo \"ID_${NEXT_ID}_IS_RT_TABLES_IN_USE=0\" >> \"$STATE_FILE\"",
             STATE_FILE, iface_name);

    return run_cmd(cmd);
}

/* 5.   STATE -> IS_ACTIVE FLAG */
static int state_set_active(const char *iface_name, int active) {
    char cmd[MAX_CMD];
    char *id = state_get_id(iface_name);

    if (!id) {
        if (state_add_iface(iface_name) != 0) {
            return 1;
        }
    }

    snprintf(cmd, MAX_CMD,
             "STATE_FILE=\"%s\"; "
             "ID=$(grep \"^ID_.*_NAME=%s$\" \"$STATE_FILE\" 2>/dev/null | sed 's/^ID_\\([0-9]*\\)_NAME.*/\\1/'); "
             "if [ -n \"$ID\" ]; then "
             "  if grep -q \"^ID_${ID}_IS_ACTIVE=\" \"$STATE_FILE\"; then "
             "    sed -i \"s/^ID_${ID}_IS_ACTIVE=.*/ID_${ID}_IS_ACTIVE=%d/\" \"$STATE_FILE\"; "
             "  else "
             "    echo \"ID_${ID}_IS_ACTIVE=%d\" >> \"$STATE_FILE\"; "
             "  fi; "
             "fi",
             STATE_FILE, iface_name, active, active);

    return run_cmd(cmd);
}



/* 6.   WG UP IFACE */
static int activate_interface(const char *iface) {
    char cmd[MAX_CMD];
    int timeout = 2;
    int count = 0;

    if (!iface || strlen(iface) == 0) {
        log_error("Interface name required!");
        return 1;
    }

    log_info("Activating interface: %s", iface);

    if (!uci_iface_exists(iface)) {
        log_error("%s DOES NOT exist!", iface);
        return 1;
    }

    if (!validate_iface_name(iface)) {
        log_error("Invalid interface name: %s (must start with 'wg')", iface);
        return 1;
    }

    snprintf(cmd, MAX_CMD, "ifup %s >/dev/null 2>&1", iface);

    if (run_cmd(cmd) == 0) {

        while (count < timeout) {

            snprintf(cmd, MAX_CMD, "ip link show %s >/dev/null 2>&1", iface);
            if (run_cmd(cmd) == 0) {
                break;
            }

            sleep(1);
            count++;
        }

        if (count >= timeout) {
            log_warning("Interface %s did not appear after %d attempts", iface, count);
        } else if (count > 0) {
            log_info("Interface appeared after %d attempts", count);
        }

        if (state_set_active(iface, 1) != 0) {
            log_error("Failed to update state for %s. Try: wg-autoconf nuke", iface);
            return 1;
        }

        log_success("Interface '%s' enabled!", iface);

        printf("\n");
        printf("%sNow you can set up routes if needed!%s\n", COLOR_YELLOW, COLOR_NC);

        return 0;

    } else {

        log_error("Errors found enabling %s. Check logread! Try a full cleanup!", iface);

        fprintf(stderr, "\n");
        fprintf(stderr, "Debug information:\n");
        snprintf(cmd, MAX_CMD, "wg show %s 2>/dev/null || echo \"  (WireGuard info not available)\"", iface);
        run_cmd(cmd);

        fprintf(stderr, "\n");
        snprintf(cmd, MAX_CMD, "ip link show | grep wg || echo \"  (no wg interfaces)\"");
        run_cmd(cmd);

        fprintf(stderr, "\n");
        snprintf(cmd, MAX_CMD, "cat %s 2>/dev/null || echo \"  (no state file)\"", STATE_FILE);
        run_cmd(cmd);

        fprintf(stderr, "\n");
        snprintf(cmd, MAX_CMD, "logread | tail -10");
        run_cmd(cmd);

        fprintf(stderr, "\n");

        return 1;
    }
}


/* 7.   WG UP DOWN */
static int deactivate_interface(const char *iface) {
    char cmd[MAX_CMD];

    if (!iface || strlen(iface) == 0) {
        log_error("Interface name required");
        return 1;
    }

    log_info("Deactivating interface: %s", iface);

    if (!uci_iface_exists(iface)) {
        log_error("%s DOES NOT exist", iface);
        return 1;
    }

    if (!validate_iface_name(iface)) {
        log_error("Invalid interface name: %s (must start with 'wg')", iface);
        return 1;
    }

    snprintf(cmd, MAX_CMD, "ifdown %s >/dev/null 2>&1", iface);

    if (run_cmd(cmd) == 0) {
        state_set_active(iface, 0);
        log_success("%s disabled!", iface);
        return 0;

    } else {
        log_error("%s was NOT disabled! Check logs!", iface);

        fprintf(stderr, "\n");

        fprintf(stderr, "Debug information:\n");
        snprintf(cmd, MAX_CMD, "wg show %s 2>/dev/null || echo \"  (no WireGuard info)\"", iface);
        run_cmd(cmd);

        fprintf(stderr, "\n");
        snprintf(cmd, MAX_CMD, "ip link show | grep wg || echo \"  (no wg interfaces)\"");
        run_cmd(cmd);

        fprintf(stderr, "\n");
        snprintf(cmd, MAX_CMD, "cat %s 2>/dev/null || echo \"  (no state file)\"", STATE_FILE);
        run_cmd(cmd);

        fprintf(stderr, "\n");
        snprintf(cmd, MAX_CMD, "logread | tail -10");
        run_cmd(cmd);

        fprintf(stderr, "\n");

        return 1;
    }
}

/* main */
int main(int argc, char **argv) {

    if (argc < 3) {
        fprintf(stderr, "Usage: wg-interface {up|down} <wg_iface>\n");
        fprintf(stderr, "\nCommands:\n");
        fprintf(stderr, "  up <wg_iface>     - Activate WireGuard interface\n");
        fprintf(stderr, "  down <wg_iface>   - Deactivate WireGuard interface\n");
        fprintf(stderr, "\nExamples:\n");
        fprintf(stderr, "  wg-interface up wg0\n");
        fprintf(stderr, "  wg-interface down wg0\n");
        return 1;
    }

    if (strcmp(argv[1], "up") == 0) {
        return activate_interface(argv[2]);

    } else if (strcmp(argv[1], "down") == 0) {
        return deactivate_interface(argv[2]);

    } else {
        log_error("Unknown command: %s", argv[1]);
        fprintf(stderr, "Valid commands: up, down\n");
        return 1;
    }
}
