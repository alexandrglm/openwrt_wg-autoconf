/**
 * wg-validator.c, fulll .conf / manual setup validator
 *
 * Autonomous usage:
 *   wg-validator validate --type <auto|manual> --field <FIELD> --value <VALUE> [--test-mode]
 *
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>

#define VERSION "1.0.0-r1"
#define MAX_LINE 4096
#define DEFAULT_PORT "51820"

#define MODE_NORMAL 0
#define MODE_TEST   1

/* UI SHITTIES */
#define COLOR_RED     "\033[1;31m"
#define COLOR_GREEN   "\033[0;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_BLUE    "\033[0;34m"
#define COLOR_NC      "\033[0m"

/* FIELD TYPE ENUM */
typedef enum {
    FIELD_DNS,
    FIELD_ALLOWED_IPS,
    FIELD_ENDPOINT,
    FIELD_PRIVATE_KEY,
    FIELD_PUBLIC_KEY,
    FIELD_ADDRESS,
    FIELD_INTERFACE_NAME,
    FIELD_UNKNOWN
} FieldType;

/* ARGS STRUCT */
typedef struct {
    int test_mode;
    FieldType field;
    char *value;
    char *config_file;
    char *config_type;
} ValidatorArgs;

/* 1.   STRCASECMP WANKER */
static int my_strcasecmp(const char *a, const char *b) {
    while (*a && *b) {
        char ca = tolower((unsigned char)*a);
        char cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++;
        b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}
/* 2.   LOG ERROR */
static void log_error(const char *fmt, ...) {
    va_list args;
    fprintf(stderr, COLOR_RED "[ERROR]" COLOR_NC " ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}
/* 3.   LOG WARNING */
static void log_warning(const char *fmt, ...) {
    va_list args;
    fprintf(stderr, COLOR_YELLOW "[WARNING]" COLOR_NC " ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
/* 4.   LOG DEBUG */
static void log_debug(const char *fmt, ...) {
    #ifdef DEBUG
    va_list args;
    fprintf(stderr, COLOR_BLUE "[DEBUG]" COLOR_NC " ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    #endif
}



/* 5.   VALIDATE IPv4 ADDRESS */
static int is_valid_ipv4(const char *ip) {
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

        if (*p != '.' && *p != '\0') return 0;
    }

    return (dots == 3);
}

/* 6.   VALIDATE IPv6 ADDRESS (simplified but works) */
static int is_valid_ipv6(const char *ip) {
    regex_t regex;
    int ret;
    const char *pattern = "^([0-9a-fA-F]{1,4}:){1,7}([0-9a-fA-F]{1,4}|:)$|^::$|^([0-9a-fA-F]{1,4}:){1,6}:([0-9a-fA-F]{1,4})?$";

    if (regcomp(&regex, pattern, REG_EXTENDED | REG_NOSUB) != 0) {
        return 0;
    }

    ret = regexec(&regex, ip, 0, NULL, 0);
    regfree(&regex);

    return ret == 0;
}

/* 7.   VALIDATE HOSTNAME */
static int is_valid_hostname(const char *hostname) {
    const char *p;
    int len = strlen(hostname);
    int has_dot = 0;

    if (len > 253) return 0;
    if (len == 0) return 0;

    for (p = hostname; *p; p++) {
        if (*p == '.') {
            has_dot = 1;
            continue;
        }
        if (!isalnum((unsigned char)*p) && *p != '-' && *p != '_') {
            return 0;
        }
    }

    return (has_dot || strchr(hostname, ':') != NULL);
}

/* 8.   VALIDATE PORT NUMBER */
static int is_valid_port(const char *port_str) {
    char *endptr;
    long port;

    if (!port_str || *port_str == '\0') return 0;

    port = strtol(port_str, &endptr, 10);
    if (*endptr != '\0') return 0;
    if (port < 1 || port > 65535) return 0;

    return 1;
}

/* 9.   VALIDATE WIREGUARD KEY (base64, 43-44 chars) */
static int is_valid_wg_key(const char *key) {
    int len;
    const char *p;
    const char *base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

    if (!key) return 0;

    while (isspace((unsigned char)*key)) key++;
    len = strlen(key);
    while (len > 0 && isspace((unsigned char)key[len-1])) len--;

    if (len < 43 || len > 44) return 0;

    for (p = key; p < key + len; p++) {
        if (strchr(base64_chars, *p) == NULL) return 0;
    }

    return 1;
}

/* 10.  VALIDATE CIDR NOTATION */
static int is_valid_cidr(const char *cidr_str, int is_ipv6) {
    char *endptr;
    long cidr;
    int max_cidr = is_ipv6 ? 128 : 32;

    if (!cidr_str || *cidr_str == '\0') return 0;

    cidr = strtol(cidr_str, &endptr, 10);
    if (*endptr != '\0') return 0;
    if (cidr < 0 || cidr > max_cidr) return 0;

    return 1;
}

/* 11.  VALIDATE IP/CIDR ADDRESS */
static int is_valid_address(const char *address) {
    char ip_part[256];
    char cidr_part[16];
    char *slash;
    int is_ipv6;

    if (!address) return 0;

    slash = strchr(address, '/');
    if (!slash) return 0;

    size_t ip_len = slash - address;
    if (ip_len >= sizeof(ip_part)) return 0;
    strncpy(ip_part, address, ip_len);
    ip_part[ip_len] = '\0';

    strncpy(cidr_part, slash + 1, sizeof(cidr_part) - 1);
    cidr_part[sizeof(cidr_part) - 1] = '\0';

    is_ipv6 = (strchr(ip_part, ':') != NULL);

    if (is_ipv6) {
        return is_valid_ipv6(ip_part) && is_valid_cidr(cidr_part, 1);
    } else {
        return is_valid_ipv4(ip_part) && is_valid_cidr(cidr_part, 0);
    }
}

/* 12.  VALIDATE DNS FIELD */
static int validate_dns(const char *value, int test_mode) {
    char value_copy[MAX_LINE];
    char *token;
    char *saveptr;
    int valid_count = 0;
    int has_error = 0;

    if (!value || strlen(value) == 0) {
        log_debug("DNS empty, will use defaults");
        return 1;
    }

    strncpy(value_copy, value, sizeof(value_copy) - 1);
    value_copy[sizeof(value_copy) - 1] = '\0';

    token = strtok_r(value_copy, ",", &saveptr);
    while (token != NULL) {
        while (isspace((unsigned char)*token)) token++;
        char *end = token + strlen(token) - 1;
        while (end > token && isspace((unsigned char)*end)) end--;
        *(end + 1) = '\0';

        if (strlen(token) == 0) {
            token = strtok_r(NULL, ",", &saveptr);
            continue;
        }

        if (is_valid_ipv4(token) || is_valid_ipv6(token)) {
            valid_count++;
            log_debug("DNS valid: %s", token);
        } else {
            if (test_mode) {
                log_warning("Invalid DNS server: %s", token);
            } else {
                log_error("Invalid DNS server: %s", token);
            }
            has_error = 1;
        }

        token = strtok_r(NULL, ",", &saveptr);
    }

    if (valid_count == 0) {
        if (test_mode) {
            log_warning("No valid DNS servers found");
        } else {
            log_error("No valid DNS servers found");
        }
        return 0;
    }

    log_debug("DNS OK: %d servers", valid_count);
    return !has_error;
}

/* 13.  VALIDATE ALLOWEDIPS FIELD */
static int validate_allowed_ips(const char *value, int test_mode) {
    char value_copy[MAX_LINE];
    char *token;
    char *saveptr;
    int valid_count = 0;
    int has_error = 0;

    if (!value || strlen(value) == 0) {
        if (test_mode) {
            log_warning("AllowedIPs cannot be empty");
        } else {
            log_error("AllowedIPs cannot be empty");
        }
        return 0;
    }

    strncpy(value_copy, value, sizeof(value_copy) - 1);
    value_copy[sizeof(value_copy) - 1] = '\0';

    token = strtok_r(value_copy, ",", &saveptr);
    while (token != NULL) {
        char ip_part[256];
        char cidr_part[16];
        char *slash;
        int is_ipv6;

        while (isspace((unsigned char)*token)) token++;
        char *end = token + strlen(token) - 1;
        while (end > token && isspace((unsigned char)*end)) end--;
        *(end + 1) = '\0';

        if (strlen(token) == 0) {
            token = strtok_r(NULL, ",", &saveptr);
            continue;
        }

        slash = strchr(token, '/');
        if (!slash) {
            if (test_mode) {
                log_warning("AllowedIPs entry missing CIDR: %s", token);
            } else {
                log_error("AllowedIPs entry missing CIDR: %s", token);
            }
            has_error = 1;
            token = strtok_r(NULL, ",", &saveptr);
            continue;
        }

        size_t ip_len = slash - token;
        if (ip_len >= sizeof(ip_part)) {
            has_error = 1;
            token = strtok_r(NULL, ",", &saveptr);
            continue;
        }
        strncpy(ip_part, token, ip_len);
        ip_part[ip_len] = '\0';

        strncpy(cidr_part, slash + 1, sizeof(cidr_part) - 1);
        cidr_part[sizeof(cidr_part) - 1] = '\0';

        char *endptr;
        long cidr_val = strtol(cidr_part, &endptr, 10);
        if (*endptr != '\0') {
            if (test_mode) {
                log_warning("Invalid CIDR in AllowedIPs: %s", cidr_part);
            } else {
                log_error("Invalid CIDR in AllowedIPs: %s", cidr_part);
            }
            has_error = 1;
            token = strtok_r(NULL, ",", &saveptr);
            continue;
        }

        is_ipv6 = (strchr(ip_part, ':') != NULL);

        if (is_ipv6) {
            if (cidr_val < 0 || cidr_val > 128) {
                if (test_mode) {
                    log_warning("Invalid IPv6 CIDR: /%s", cidr_part);
                } else {
                    log_error("Invalid IPv6 CIDR: /%s", cidr_part);
                }
                has_error = 1;
            } else if (is_valid_ipv6(ip_part)) {
                valid_count++;
                log_debug("AllowedIPs valid: %s/%s", ip_part, cidr_part);
            } else {
                if (test_mode) {
                    log_warning("Invalid IPv6 in AllowedIPs: %s", ip_part);
                } else {
                    log_error("Invalid IPv6 in AllowedIPs: %s", ip_part);
                }
                has_error = 1;
            }
        } else {
            if (cidr_val < 0 || cidr_val > 32) {
                if (test_mode) {
                    log_warning("Invalid IPv4 CIDR: /%s", cidr_part);
                } else {
                    log_error("Invalid IPv4 CIDR: /%s", cidr_part);
                }
                has_error = 1;
            } else if (is_valid_ipv4(ip_part)) {
                valid_count++;
                log_debug("AllowedIPs valid: %s/%s", ip_part, cidr_part);
            } else {
                if (test_mode) {
                    log_warning("Invalid IPv4 in AllowedIPs: %s", ip_part);
                } else {
                    log_error("Invalid IPv4 in AllowedIPs: %s", ip_part);
                }
                has_error = 1;
            }
        }

        token = strtok_r(NULL, ",", &saveptr);
    }

    if (valid_count == 0) {
        if (test_mode) {
            log_warning("No valid AllowedIPs found");
        } else {
            log_error("No valid AllowedIPs found");
        }
        return 0;
    }

    log_debug("AllowedIPs OK: %d entries", valid_count);
    return !has_error;
}

/* 14.  VALIDATE ENDPOINT FIELD */
static int validate_endpoint(const char *value, int test_mode) {
    char host_part[256];
    char port_part[16];
    int has_port = 0;

    if (!value || strlen(value) == 0) {
        if (test_mode) {
            log_warning("Endpoint cannot be empty");
        } else {
            log_error("Endpoint cannot be empty");
        }
        return 0;
    }

    memset(host_part, 0, sizeof(host_part));
    strcpy(port_part, DEFAULT_PORT);

    if (value[0] == '[') {
        const char *close_bracket = strchr(value, ']');
        if (!close_bracket) {
            if (test_mode) {
                log_warning("Invalid IPv6 endpoint format (missing closing bracket)");
            } else {
                log_error("Invalid IPv6 endpoint format (missing closing bracket)");
            }
            return 0;
        }

        size_t host_len = close_bracket - value - 1;
        if (host_len >= sizeof(host_part)) {
            if (test_mode) {
                log_warning("IPv6 address too long");
            } else {
                log_error("IPv6 address too long");
            }
            return 0;
        }
        strncpy(host_part, value + 1, host_len);
        host_part[host_len] = '\0';

        if (*(close_bracket + 1) == ':') {
            strncpy(port_part, close_bracket + 2, sizeof(port_part) - 1);
            port_part[sizeof(port_part) - 1] = '\0';
            has_port = 1;
        }
    } else {
        const char *colon = strchr(value, ':');
        if (colon) {
            size_t host_len = colon - value;
            if (host_len >= sizeof(host_part)) {
                if (test_mode) {
                    log_warning("Host part too long");
                } else {
                    log_error("Host part too long");
                }
                return 0;
            }
            strncpy(host_part, value, host_len);
            host_part[host_len] = '\0';

            strncpy(port_part, colon + 1, sizeof(port_part) - 1);
            port_part[sizeof(port_part) - 1] = '\0';
            has_port = 1;
        } else {
            strncpy(host_part, value, sizeof(host_part) - 1);
            host_part[sizeof(host_part) - 1] = '\0';
        }
    }

    if (strlen(host_part) == 0) {
        if (test_mode) {
            log_warning("Endpoint host cannot be empty");
        } else {
            log_error("Endpoint host cannot be empty");
        }
        return 0;
    }

    if (!is_valid_ipv4(host_part) && !is_valid_ipv6(host_part) && !is_valid_hostname(host_part)) {
        if (test_mode) {
            log_warning("Invalid endpoint host: %s", host_part);
        } else {
            log_error("Invalid endpoint host: %s", host_part);
        }
        return 0;
    }

    if (has_port && !is_valid_port(port_part)) {
        if (test_mode) {
            log_warning("Invalid endpoint port: %s", port_part);
        } else {
            log_error("Invalid endpoint port: %s", port_part);
        }
        return 0;
    }

    log_debug("Endpoint OK: %s:%s", host_part, port_part);
    return 1;
}

/* 15.  VALIDATE PRIVATE/PUBLIC KEY FIELD */
static int validate_wg_key_field(const char *field_name, const char *value, int test_mode) {
    if (!value || strlen(value) == 0) {
        if (test_mode) {
            log_warning("%s cannot be empty", field_name);
        } else {
            log_error("%s cannot be empty", field_name);
        }
        return 0;
    }

    if (!is_valid_wg_key(value)) {
        if (test_mode) {
            log_warning("Invalid %s format (not valid base64, length 43-44)", field_name);
        } else {
            log_error("Invalid %s format (not valid base64, length 43-44)", field_name);
        }
        return 0;
    }

    log_debug("%s OK", field_name);
    return 1;
}

/* 16.  VALIDATE ADDRESS FIELD */
static int validate_address_field(const char *value, int test_mode) {
    if (!value || strlen(value) == 0) {
        if (test_mode) {
            log_warning("Address cannot be empty");
        } else {
            log_error("Address cannot be empty");
        }
        return 0;
    }

    if (!is_valid_address(value)) {
        if (test_mode) {
            log_warning("Invalid Address format: %s (must be IP/CIDR)", value);
        } else {
            log_error("Invalid Address format: %s (must be IP/CIDR)", value);
        }
        return 0;
    }

    log_debug("Address OK: %s", value);
    return 1;
}

/* 17.  VALIDATE INTERFACE NAME FIELD */
static int validate_interface_name(const char *value, int test_mode) {
    const char *p;
    int len;

    if (!value || strlen(value) == 0) {
        if (test_mode) {
            log_warning("Interface name cannot be empty");
        } else {
            log_error("Interface name cannot be empty");
        }
        return 0;
    }

    len = strlen(value);
    if (len > 15) {
        if (test_mode) {
            log_warning("Interface name too long: %s (max 15 chars)", value);
        } else {
            log_error("Interface name too long: %s (max 15 chars)", value);
        }
        return 0;
    }

    if (strncmp(value, "wg", 2) != 0) {
        if (test_mode) {
            log_warning("Interface name must start with 'wg': %s", value);
        } else {
            log_error("Interface name must start with 'wg': %s", value);
        }
        return 0;
    }

    if (strcmp(value, "wg") == 0) {
        if (test_mode) {
            log_warning("Interface name cannot be just 'wg'");
        } else {
            log_error("Interface name cannot be just 'wg'");
        }
        return 0;
    }

    for (p = value; *p; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-') {
            if (test_mode) {
                log_warning("Invalid interface name: %s (only letters, numbers, _, -)", value);
            } else {
                log_error("Invalid interface name: %s (only letters, numbers, _, -)", value);
            }
            return 0;
        }
    }

    log_debug("InterfaceName OK: %s", value);
    return 1;
}

/* 18.  PARSE FIELD TYPE FROM STRING */
static FieldType parse_field(const char *field_str) {
    if (my_strcasecmp(field_str, "DNS") == 0) return FIELD_DNS;
    if (my_strcasecmp(field_str, "AllowedIPs") == 0) return FIELD_ALLOWED_IPS;
    if (my_strcasecmp(field_str, "Endpoint") == 0) return FIELD_ENDPOINT;
    if (my_strcasecmp(field_str, "PrivateKey") == 0) return FIELD_PRIVATE_KEY;
    if (my_strcasecmp(field_str, "PublicKey") == 0) return FIELD_PUBLIC_KEY;
    if (my_strcasecmp(field_str, "Address") == 0) return FIELD_ADDRESS;
    if (my_strcasecmp(field_str, "InterfaceName") == 0) return FIELD_INTERFACE_NAME;
    return FIELD_UNKNOWN;
}

/* 19.  PRINT USAGE BULLSHIT */
static void print_usage(const char *progname) {
    fprintf(stderr, "Usage: %s validate --type <auto|manual> --field <FIELD> --value <VALUE> [--test-mode] [--config <file>]\n", progname);
    fprintf(stderr, "\nFields:\n");
    fprintf(stderr, "  DNS, AllowedIPs, Endpoint, PrivateKey, PublicKey, Address, InterfaceName\n");
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  --test-mode    Non-fatal validation (warnings only)\n");
    fprintf(stderr, "  --config       Configuration file (for context, not used in validation)\n");
    fprintf(stderr, "\nExit codes:\n");
    fprintf(stderr, "  0 = valid, 1 = invalid\n");
}

/* 20.  PARSE COMMAND LINE ARGUMENTS */
static int parse_args(int argc, char **argv, ValidatorArgs *args) {
    int i;

    memset(args, 0, sizeof(ValidatorArgs));
    args->field = FIELD_UNKNOWN;
    args->test_mode = MODE_NORMAL;

    if (argc < 2) {
        print_usage(argv[0]);
        return 0;
    }

    if (strcmp(argv[1], "validate") != 0) {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        print_usage(argv[0]);
        return 0;
    }

    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--type") == 0 && i + 1 < argc) {
            args->config_type = argv[++i];
        } else if (strcmp(argv[i], "--field") == 0 && i + 1 < argc) {
            args->field = parse_field(argv[++i]);
        } else if (strcmp(argv[i], "--value") == 0 && i + 1 < argc) {
            args->value = argv[++i];
        } else if (strcmp(argv[i], "--test-mode") == 0) {
            args->test_mode = MODE_TEST;
        } else if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            args->config_file = argv[++i];
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 0;
        }
    }

    if (args->field == FIELD_UNKNOWN) {
        log_error("--field is required");
        return 0;
    }

    if (!args->value) {
        log_error("--value is required");
        return 0;
    }

    return 1;
}

/* main */
int main(int argc, char **argv) {
    ValidatorArgs args;
    int valid = 0;

    if (!parse_args(argc, argv, &args)) {
        return 1;
    }

    switch (args.field) {
        case FIELD_DNS:
            valid = validate_dns(args.value, args.test_mode);
            break;
        case FIELD_ALLOWED_IPS:
            valid = validate_allowed_ips(args.value, args.test_mode);
            break;
        case FIELD_ENDPOINT:
            valid = validate_endpoint(args.value, args.test_mode);
            break;
        case FIELD_PRIVATE_KEY:
            valid = validate_wg_key_field("PrivateKey", args.value, args.test_mode);
            break;
        case FIELD_PUBLIC_KEY:
            valid = validate_wg_key_field("PublicKey", args.value, args.test_mode);
            break;
        case FIELD_ADDRESS:
            valid = validate_address_field(args.value, args.test_mode);
            break;
        case FIELD_INTERFACE_NAME:
            valid = validate_interface_name(args.value, args.test_mode);
            break;
        default:
            log_error("Unknown field type");
            return 1;
    }

    return valid ? 0 : 1;
}
