/* 명령줄에서 UTF-8 문자열을 번역하는 실행 프로그램입니다. */
#include "cpapago.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void)
{
    fprintf(stderr,
        "Usage:\n"
        "  cpapago detect TEXT\n"
        "  cpapago translate TARGET TEXT [--source CODE] [--honorific] [--verbose]\n"
        "  cpapago batch TARGET TEXT... [--source CODE] [--honorific] [--verbose]\n"
        "Options: --base-url URL, --timeout MS, --parallel N, -- (end options)\n");
}

/* 다중 일반 번역도 유효한 JSON 문자열 배열로 출력합니다. */
static void print_json_string(const char *text)
{
    const unsigned char *c = (const unsigned char *)text;
    putchar('"');
    for (; *c; ++c) {
        if (*c == '"' || *c == '\\') { putchar('\\'); putchar(*c); }
        else if (*c < 0x20) printf("\\u%04x", (unsigned)*c);
        else putchar(*c);
    }
    putchar('"');
}

int main(int argc, char **argv)
{
    cpapago_options options = {0};
    cpapago_error error = {0};
    const char *source = NULL;
    const char **arguments;
    int count = 0, literal = 0, i;
    cpapago_status status;
    char *output = NULL;
    if (argc < 3) { usage(); return 2; }
    arguments = calloc((size_t)argc, sizeof(*arguments));
    if (!arguments) return 1;
    for (i = 2; i < argc; ++i) {
        if (!literal && strcmp(argv[i], "--") == 0) { literal = 1; continue; }
        if (!literal && strcmp(argv[i], "--verbose") == 0) options.verbose = true;
        else if (!literal && strcmp(argv[i], "--honorific") == 0) options.honorific = true;
        else if (!literal && (strcmp(argv[i], "--source") == 0 || strcmp(argv[i], "--base-url") == 0 ||
                              strcmp(argv[i], "--timeout") == 0 || strcmp(argv[i], "--parallel") == 0)) {
            const char *flag = argv[i++];
            if (i == argc) goto invalid;
            if (strcmp(flag, "--source") == 0) source = argv[i];
            else if (strcmp(flag, "--base-url") == 0) options.base_url = argv[i];
            else {
                char *end;
                long value;
                errno = 0; value = strtol(argv[i], &end, 10);
                if (errno || !*argv[i] || *end || value <= 0) goto invalid;
                if (strcmp(flag, "--timeout") == 0) options.timeout_ms = value;
                else {
                    if (value > 16) goto invalid;
                    options.parallelism = (size_t)value;
                }
            }
        } else if (!literal && strncmp(argv[i], "--", 2) == 0) goto invalid;
        else arguments[count++] = argv[i];
    }
    if (strcmp(argv[1], "detect") == 0 ? count != 1 : count < 2) goto invalid;
    if (strcmp(argv[1], "detect") && strcmp(argv[1], "translate") && strcmp(argv[1], "batch")) goto invalid;
    if (strcmp(argv[1], "translate") == 0 && count != 2) goto invalid;
    if (cpapago_init() != CPAPAGO_OK) { free(arguments); return 1; }
    if (strcmp(argv[1], "detect") == 0) {
        status = cpapago_detect(arguments[0], &options, &output, &error);
    } else if (strcmp(argv[1], "translate") == 0) {
        cpapago_parameter parameter = {source, arguments[0], arguments[1]};
        status = cpapago_translate(&parameter, &options, &output, &error);
    } else {
        size_t n = (size_t)count - 1, j;
        char **outputs = NULL;
        cpapago_parameter *parameters = calloc(n, sizeof(*parameters));
        if (!parameters) { cpapago_cleanup(); free(arguments); return 1; }
        for (j = 0; j < n; ++j) {
            parameters[j].source = source;
            parameters[j].target = arguments[0];
            parameters[j].text = arguments[j + 1];
        }
        status = cpapago_translate_many(parameters, n, &options, &outputs, &error);
        if (status == CPAPAGO_OK) {
            puts("[");
            for (j = 0; j < n; ++j) {
                if (j) puts(",");
                if (options.verbose) fputs(outputs[j], stdout);
                else print_json_string(outputs[j]);
            }
            puts("\n]");
        }
        cpapago_free_many(outputs, n); free(parameters);
    }
    if (status == CPAPAGO_OK && output) puts(output);
    else if (status != CPAPAGO_OK) fprintf(stderr, "cpapago: %s (status=%d, HTTP=%ld)\n", error.message, status, error.http_status);
    free(output); free(arguments); cpapago_cleanup();
    return status == CPAPAGO_OK ? 0 : 1;
invalid:
    usage(); free(arguments); return 2;
}
