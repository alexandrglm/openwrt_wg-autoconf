/**
 * wg-get_conf_value.c, from gett_conf_value()
 *
 * Autonomous usagi:
 *   wg-get_conf_value get
 *        --key <KEY> [--file <file>] [--format <raw|uci|conf>] [--value <direct_value>]
 *
 * EXiT CODES:
 *   0 = success (value printed to stdout)
 *   1 = error (no value, error message to stderr)
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>

#define VERSION "1.0.0-r1"
#define MAX_LINE 4096
#define MAX_KEY_LEN 256

/* OUTPUT enums*/
typedef enum {
    FORMAT_RAW,
    FORMAT_UCI,
    FORMAT_CONF
} OutputFormat;

/* get_conf args */
typedef struct {
    char *key;
    char *file;
    char *direct_value;
    OutputFormat format;
} GetConfArgs;

/**
 * UI SHITTIES, un-"refactorised"
 *
 */
static void log_error(const char *fmt, ...) {
    va_list args;
    fprintf(stderr, "[ERROR] ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

/* CLEAN STR WHITESPADE */
static void trim(char *str) {
    char *start = str;
    char *end;

    /* AT THE BEGINNIG */
    if (!str || !*str) return;
    while (isspace((unsigned char)*start)) start++;

    if (*start == '\0') {
        *str = '\0';
        return;
    }

    /* AT THE END */
    end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';

    /* if OFFSET, MOVE */
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

/* VALUE PARSER */
static char* format_value(const char *raw_value, OutputFormat format) {
    static char buffer[MAX_LINE];
    const char *p;
    int first = 1;

    if (!raw_value || strlen(raw_value) == 0) {
        return NULL;
    }

    switch (format) {
        /* 1. RAW ---> TRIM */
        case FORMAT_RAW:
            strncpy(buffer, raw_value, MAX_LINE - 1);
            buffer[MAX_LINE - 1] = '\0';
            trim(buffer);
            return buffer;

            /* 2. UCI --> COMMA-SEPARATED to SPACE-SEP */
            case FORMAT_UCI:
                buffer[0] = '\0';
                p = raw_value;
                first = 1;

                while (*p) {
                    while (isspace((unsigned char)*p)) p++;
                    if (*p == '\0') break;

                    /* if NOT FIRST --> ADD SPACE */
                    if (!first) {
                        strcat(buffer, " ");
                    }
                    first = 0;

                    /* COPY UNTIL COMMA, OTHERWISE END */
                    while (*p && *p != ',') {
                        size_t len = strlen(buffer);
                        buffer[len] = *p;
                        buffer[len + 1] = '\0';
                        p++;
                    }

                    /*  */
                    if (*p == ',') p++;
                }

                /* LAST CLEANUPS */
                trim(buffer);

                return buffer;

                /* 3. .CONF ---> COMMA-SEP plus SPACES */
                case FORMAT_CONF:
                    buffer[0] = '\0';
                    p = raw_value;
                    first = 1;

                    while (*p) {
                        while (isspace((unsigned char)*p)) p++;
                        if (*p == '\0') break;

                        if (!first) {
                            strcat(buffer, ", ");
                        }
                        first = 0;

                        while (*p && *p != ',') {
                            size_t len = strlen(buffer);
                            buffer[len] = *p;
                            buffer[len + 1] = '\0';
                            p++;
                        }

                        if (*p == ',') p++;
                    }

                    trim(buffer);

                    return buffer;

                default:
                    return NULL;
    }
}


/* GET VALUE RETURN RAW */
static char* get_value_from_file(const char *key, const char *filename) {
    FILE *fp;
    char line[MAX_LINE];
    char *value = NULL;
    size_t key_len;

    if (!key || !filename) return NULL;

    fp = fopen(filename, "r");
    if (!fp) {
        log_error("Cannot open file: %s", filename);
        return NULL;
    }

    key_len = strlen(key);

    while (fgets(line, sizeof(line), fp)) {
        char *equal_sign;
        char *line_key;
        char *line_value;

        line_key = line;

        while (isspace((unsigned char)*line_key)) line_key++;

        if (strncmp(line_key, key, key_len) != 0) continue;

        equal_sign = line_key + key_len;
        if (*equal_sign != '=') continue;

        line_value = equal_sign + 1;

        char *end = line_value + strlen(line_value) - 1;
        while (end > line_value && isspace((unsigned char)*end)) {
            *end = '\0';
            end--;
        }

        while (isspace((unsigned char)*line_value)) line_value++;

        /* COPY VALUE */
        value = strdup(line_value);
        if (!value) {
            log_error("Memory allocation failed");
            fclose(fp);
            return NULL;
        }

        break;
    }

    fclose(fp);
    return value;
}


/**
 *
 * aarg PARSER
 *
 */
static OutputFormat parse_format(const char *format_str) {
    if (strcmp(format_str, "uci") == 0) return FORMAT_UCI;
    if (strcmp(format_str, "conf") == 0) return FORMAT_CONF;
    return FORMAT_RAW;  /* default */
}

/* HELP USAGE AUTONOMOUS MODE */
static void print_usage(const char *progname) {
    fprintf(stderr, "Usage: %s get --key <KEY> [--file <file>] [--format <raw|uci|conf>] [--value <direct_value>]\n", progname);
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  --key <KEY>           Configuration key to extract\n");
    fprintf(stderr, "  --file <file>         WireGuard .conf file to read from\n");
    fprintf(stderr, "  --format <format>     Output format: raw (default), uci, conf\n");
    fprintf(stderr, "  --value <value>       Direct value to format (bypass file reading)\n");
    fprintf(stderr, "\nFormats:\n");
    fprintf(stderr, "  raw   - Value as-is (trimmed)\n");
    fprintf(stderr, "  uci   - Comma-separated -> space-separated (for UCI)\n");
    fprintf(stderr, "  conf  - Comma-separated -> ', ' (for .conf files)\n");
    fprintf(stderr, "\nExit codes:\n");
    fprintf(stderr, "  0 = success (value printed to stdout)\n");
    fprintf(stderr, "  1 = error (no value)\n");
    fprintf(stderr, "\nExamples:\n");
    fprintf(stderr, "  %s get --key PrivateKey --file /etc/wireguard/wg0.conf\n", progname);
    fprintf(stderr, "  %s get --key DNS --file wg0.conf --format uci\n", progname);
    fprintf(stderr, "  %s get --key AllowedIPs --format conf --value \"0.0.0.0/0, ::/0\"\n", progname);
}

static int parse_args(int argc, char **argv, GetConfArgs *args) {
    int i;

    memset(args, 0, sizeof(GetConfArgs));
    args->format = FORMAT_RAW;

    if (argc < 2) {
        print_usage(argv[0]);
        return 0;
    }

    if (strcmp(argv[1], "get") != 0) {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        print_usage(argv[0]);
        return 0;
    }

    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--key") == 0 && i + 1 < argc) {
            args->key = argv[++i];
        } else if (strcmp(argv[i], "--file") == 0 && i + 1 < argc) {
            args->file = argv[++i];
        } else if (strcmp(argv[i], "--format") == 0 && i + 1 < argc) {
            args->format = parse_format(argv[++i]);
        } else if (strcmp(argv[i], "--value") == 0 && i + 1 < argc) {
            args->direct_value = argv[++i];
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 0;
        }
    }

    if (!args->key) {
        log_error("--key is required");
        return 0;
    }

    if (!args->direct_value && !args->file) {
        log_error("Either --file or --value is required");
        return 0;
    }

    return 1;
}



/* main */
int main(int argc, char **argv) {
    GetConfArgs args;
    char *raw_value = NULL;
    char *formatted_value;
    int ret = 1;

    if (!parse_args(argc, argv, &args)) {
        return 1;
    }

    if (args.direct_value) {

        raw_value = strdup(args.direct_value);

        if (!raw_value) {
            log_error("Memory allocation failed! this sould NOT happen! Check debug log! Did you tampered bins?");
            return 1;
        }

    } else if (args.file) {

        raw_value = get_value_from_file(args.key, args.file);

        if (!raw_value) {

            /* log error HERE*/
            return 1;
        }
    }


    if (raw_value && strlen(raw_value) > 0) {

        formatted_value = format_value(raw_value, args.format);

        if (formatted_value && strlen(formatted_value) > 0) {
            printf("%s\n", formatted_value);
            ret = 0;
        }

    }

    if (raw_value) free(raw_value);

    return ret;
}
