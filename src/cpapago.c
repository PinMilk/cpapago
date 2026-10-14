#include "cpapago.h"
#include "json.h"
#include <curl/curl.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RESPONSE_LIMIT (8u * 1024u * 1024u)
#define DEFAULT_BASE "https://papago.naver.com"
#define USER_AGENT "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/27.0.1 Safari/605.1.15"

static cpapago_status fail(cpapago_error *error, cpapago_status code, long http, const char *message)
{
    if (error) {
        error->code = code;
        error->http_status = http;
        snprintf(error->message, sizeof(error->message), "%s", message);
    }
    return code;
}

static void clear_error(cpapago_error *error)
{
    if (error) memset(error, 0, sizeof(*error));
}

cpapago_status cpapago_init(void)
{
    return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK ? CPAPAGO_OK : CPAPAGO_INTERNAL_ERROR;
}

void cpapago_cleanup(void) { curl_global_cleanup(); }

static int valid_options(const cpapago_options *options)
{
    if (!options) return 1;
    if (options->timeout_ms < 0 || options->parallelism > 16) return 0;
    return !options->base_url || strncmp(options->base_url, "https://", 8) == 0 ||
           strncmp(options->base_url, "http://", 7) == 0;
}

/* 바이트 기준 폼 인코딩입니다. 입력의 &, +, %, 줄바꿈과 한글을 손상시키지 않습니다. */
static char *make_form(const char *const keys[], const char *const values[], size_t count)
{
    char *parts[8] = {0};
    char *body = NULL;
    size_t length = 0, i, offset = 0;
    if (count > sizeof(parts) / sizeof(parts[0])) return NULL;
    for (i = 0; i < count; ++i) {
        size_t value_length = strlen(values[i]);
        size_t part_length;
        if (value_length > INT_MAX) goto done;
        parts[i] = curl_easy_escape(NULL, values[i], (int)value_length);
        if (!parts[i]) goto done;
        part_length = strlen(keys[i]) + strlen(parts[i]) + 2;
        if (length > SIZE_MAX - part_length) goto done;
        length += part_length;
    }
    if (length == SIZE_MAX) goto done;
    body = malloc(length + 1);
    if (!body) goto done;
    for (i = 0; i < count; ++i) {
        if (i) body[offset++] = '&';
        memcpy(body + offset, keys[i], strlen(keys[i])); offset += strlen(keys[i]);
        body[offset++] = '=';
        memcpy(body + offset, parts[i], strlen(parts[i])); offset += strlen(parts[i]);
    }
    body[offset] = '\0';
done:
    for (i = 0; i < count; ++i) curl_free(parts[i]);
    return body;
}

typedef struct { char *data; size_t size; int memory_error, too_large, invalid; } response;

/* libcurl의 조각 응답을 누적합니다. 크기/곱셈 오버플로와 과도한 응답을 방지합니다. */
static size_t receive_body(char *data, size_t size, size_t count, void *context)
{
    response *r = context;
    size_t bytes;
    char *next;
    if (size && count > SIZE_MAX / size) { r->too_large = 1; return 0; }
    bytes = size * count;
    if (memchr(data, 0, bytes)) { r->invalid = 1; return 0; }
    if (bytes > RESPONSE_LIMIT - r->size) { r->too_large = 1; return 0; }
    next = realloc(r->data, r->size + bytes + 1);
    if (!next) { r->memory_error = 1; return 0; }
    r->data = next;
    memcpy(r->data + r->size, data, bytes);
    r->size += bytes;
    r->data[r->size] = '\0';
    return bytes;
}

static cpapago_status post(const char *path, const char *body, const cpapago_options *options,
                            char **output, cpapago_error *error)
{
    CURL *curl = NULL;
    struct curl_slist *headers = NULL, *next;
    const char *base = options && options->base_url ? options->base_url : DEFAULT_BASE;
    size_t base_length = strlen(base), path_length = strlen(path);
    char *url = NULL;
    char details[CURL_ERROR_SIZE] = {0};
    response r = {0};
    CURLcode code;
    long http = 0;
    cpapago_status status = CPAPAGO_OUT_OF_MEMORY;
    while (base_length && base[base_length - 1] == '/') --base_length;
    if (base_length > SIZE_MAX - path_length - 1) goto done;
    url = malloc(base_length + path_length + 1);
    if (!url) goto done;
    memcpy(url, base, base_length); memcpy(url + base_length, path, path_length + 1);
    curl = curl_easy_init();
    if (!curl) goto done;
    headers = curl_slist_append(NULL, "Content-Type: application/x-www-form-urlencoded; charset=UTF-8");
    if (!headers) goto done;
    next = curl_slist_append(headers, "Accept-Language: ko");
    if (!next) goto done;
    headers = next;
    /* 각 요청은 독립 핸들을 사용합니다. TLS 검증은 기본값(활성화)을 유지합니다. */
#define SET(option, value) do { if (curl_easy_setopt(curl, option, value) != CURLE_OK) { status = CPAPAGO_INTERNAL_ERROR; goto done; } } while (0)
    SET(CURLOPT_URL, url);
    SET(CURLOPT_HTTPHEADER, headers);
    SET(CURLOPT_USERAGENT, USER_AGENT);
    SET(CURLOPT_POSTFIELDS, body);
    SET(CURLOPT_TIMEOUT_MS, options && options->timeout_ms ? options->timeout_ms : 30000L);
    SET(CURLOPT_CONNECTTIMEOUT_MS, 10000L);
    SET(CURLOPT_NOSIGNAL, 1L);
    SET(CURLOPT_WRITEFUNCTION, receive_body);
    SET(CURLOPT_WRITEDATA, &r);
    SET(CURLOPT_ERRORBUFFER, details);
#undef SET
    code = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);
    if (r.memory_error) goto done;
    if (r.too_large) { status = fail(error, CPAPAGO_INVALID_RESPONSE, http, "Response exceeds 8 MiB"); goto done; }
    if (r.invalid) { status = fail(error, CPAPAGO_INVALID_RESPONSE, http, "Response contains a raw NUL byte"); goto done; }
    if (code != CURLE_OK) {
        status = fail(error, CPAPAGO_NETWORK_ERROR, http, details[0] ? details : curl_easy_strerror(code));
        goto done;
    }
    if (http < 200 || http >= 300) {
        char message[64];
        snprintf(message, sizeof(message), "Papago returned HTTP %ld", http);
        status = fail(error, CPAPAGO_HTTP_ERROR, http, message);
        goto done;
    }
    if (!r.data) { status = fail(error, CPAPAGO_INVALID_RESPONSE, http, "Empty response"); goto done; }
    *output = r.data; r.data = NULL;
    status = CPAPAGO_OK;
done:
    if (status == CPAPAGO_OUT_OF_MEMORY) fail(error, status, http, "Out of memory");
    if (status == CPAPAGO_INTERNAL_ERROR) fail(error, status, http, "Could not configure HTTP request");
    free(r.data); free(url); curl_slist_free_all(headers);
    if (curl) curl_easy_cleanup(curl);
    return status;
}

static int ascii_equal(const char *left, const char *right)
{
    while (*left && *right) {
        unsigned char c = (unsigned char)*left++;
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (c != (unsigned char)*right++) return 0;
    }
    return *left == *right;
}

static const char *api_code(const char *code)
{
    if (ascii_equal(code, "zh-cn")) return "zh-CN";
    if (ascii_equal(code, "zh-tw")) return "zh-TW";
    return code;
}

static cpapago_status extract(char *document, const char *field, char **output, cpapago_error *error)
{
    int parsed = cpapago_json_string(document, strlen(document), field, output);
    if (parsed == 1) return CPAPAGO_OK;
    return fail(error, parsed == -1 ? CPAPAGO_OUT_OF_MEMORY : CPAPAGO_INVALID_RESPONSE,
                0, parsed == -1 ? "Out of memory" : "Invalid JSON response or missing string field");
}

cpapago_status cpapago_detect(const char *text, const cpapago_options *options,
                              char **output, cpapago_error *error)
{
    const char *keys[] = {"query"};
    const char *values[] = {text};
    char *body, *document = NULL;
    cpapago_status status;
    clear_error(error);
    if (output) *output = NULL;
    if (!text || !output || !valid_options(options)) return fail(error, CPAPAGO_INVALID_ARGUMENT, 0, "Invalid detection arguments");
    body = make_form(keys, values, 1);
    if (!body) return fail(error, CPAPAGO_OUT_OF_MEMORY, 0, "Could not encode form data");
    status = post("/api/langs/dect", body, options, &document, error);
    free(body);
    if (status == CPAPAGO_OK) status = extract(document, "langCode", output, error);
    free(document);
    if (status == CPAPAGO_OK && !**output) {
        free(*output); *output = NULL;
        return fail(error, CPAPAGO_INVALID_RESPONSE, 0, "Empty language code");
    }
    if (status == CPAPAGO_OK) {
        char *c;
        for (c = *output; *c; ++c) if (*c >= 'A' && *c <= 'Z') *c += 'a' - 'A';
    }
    return status;
}

cpapago_status cpapago_translate(const cpapago_parameter *parameter, const cpapago_options *options,
                                 char **output, cpapago_error *error)
{
    const char *keys[] = {"dict", "dictDisplay", "honorific", "useGlossary", "source", "target", "text"};
    const char *values[7];
    const char *source;
    char *detected = NULL, *body = NULL, *document = NULL;
    cpapago_status status;
    clear_error(error);
    if (output) *output = NULL;
    if (!output || !parameter || !parameter->text || !parameter->target || !*parameter->target || !valid_options(options))
        return fail(error, CPAPAGO_INVALID_ARGUMENT, 0, "Invalid translation arguments");
    source = parameter->source;
    if (!source || strcmp(source, "detect") == 0) {
        status = cpapago_detect(parameter->text, options, &detected, error);
        if (status != CPAPAGO_OK) return status;
        source = detected;
    }
    values[0] = "true"; values[1] = "30";
    values[2] = options && options->honorific ? "true" : "false";
    values[3] = "false"; values[4] = api_code(source);
    values[5] = api_code(parameter->target); values[6] = parameter->text;
    body = make_form(keys, values, 7);
    free(detected);
    if (!body) return fail(error, CPAPAGO_OUT_OF_MEMORY, 0, "Could not encode form data");
    status = post("/api/text/translation", body, options, &document, error);
    free(body);
    if (status == CPAPAGO_OK) {
        status = extract(document, "translatedText", output, error);
        if (status == CPAPAGO_OK && options && options->verbose) {
            free(*output); *output = document; document = NULL;
        }
    }
    free(document);
    return status;
}

void cpapago_free_many(char **outputs, size_t count)
{
    size_t i;
    if (!outputs) return;
    for (i = 0; i < count; ++i) free(outputs[i]);
    free(outputs);
}

typedef struct {
    const cpapago_parameter *parameters;
    const cpapago_options *options;
    size_t count, next;
    char **outputs;
    cpapago_error *errors;
    cpapago_status *statuses;
    pthread_mutex_t mutex;
} jobs;

static void *worker(void *context)
{
    jobs *queue = context;
    for (;;) {
        size_t index;
        pthread_mutex_lock(&queue->mutex);
        index = queue->next;
        if (index < queue->count) ++queue->next;
        pthread_mutex_unlock(&queue->mutex);
        if (index >= queue->count) break;
        queue->statuses[index] = cpapago_translate(&queue->parameters[index], queue->options,
                                                  &queue->outputs[index], &queue->errors[index]);
    }
    return NULL;
}

cpapago_status cpapago_translate_many(const cpapago_parameter *parameters, size_t count,
                                      const cpapago_options *options, char ***outputs, cpapago_error *error)
{
    jobs queue = {0};
    pthread_t threads[15];
    size_t concurrency, started = 0, i;
    cpapago_status status = CPAPAGO_OK;
    clear_error(error);
    if (outputs) *outputs = NULL;
    if (!outputs || (count && !parameters) || !valid_options(options))
        return fail(error, CPAPAGO_INVALID_ARGUMENT, 0, "Invalid multi-translation arguments");
    if (!count) return CPAPAGO_OK;
    if (count > SIZE_MAX / sizeof(char *) || count > SIZE_MAX / sizeof(cpapago_error) || count > SIZE_MAX / sizeof(cpapago_status))
        return fail(error, CPAPAGO_OUT_OF_MEMORY, 0, "Too many translation parameters");
    queue.parameters = parameters; queue.options = options; queue.count = count;
    queue.outputs = calloc(count, sizeof(*queue.outputs));
    queue.errors = calloc(count, sizeof(*queue.errors));
    queue.statuses = calloc(count, sizeof(*queue.statuses));
    if (!queue.outputs || !queue.errors || !queue.statuses) {
        status = fail(error, CPAPAGO_OUT_OF_MEMORY, 0, "Out of memory"); goto done;
    }
    if (pthread_mutex_init(&queue.mutex, NULL) != 0) {
        status = fail(error, CPAPAGO_INTERNAL_ERROR, 0, "Could not initialize worker mutex"); goto done;
    }
    concurrency = options && options->parallelism ? options->parallelism : 4;
    if (concurrency > count) concurrency = count;
    /* 호출 스레드도 작업합니다. 추가 스레드 생성 실패 시 나머지는 기존 작업자가 처리합니다. */
    while (started + 1 < concurrency) {
        if (pthread_create(&threads[started], NULL, worker, &queue) != 0) break;
        ++started;
    }
    worker(&queue);
    for (i = 0; i < started; ++i) pthread_join(threads[i], NULL);
    pthread_mutex_destroy(&queue.mutex);
    for (i = 0; i < count; ++i) {
        if (queue.statuses[i] != CPAPAGO_OK) {
            status = queue.statuses[i];
            if (error) *error = queue.errors[i];
            goto done;
        }
    }
    *outputs = queue.outputs; queue.outputs = NULL;
done:
    cpapago_free_many(queue.outputs, count);
    free(queue.errors); free(queue.statuses);
    return status;
}
