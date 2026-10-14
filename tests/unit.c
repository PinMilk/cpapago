#include "cpapago.h"
#include "json.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void valid(const char *json, const char *field, const char *expected)
{
    char *output = NULL;
    assert(cpapago_json_string(json, strlen(json), field, &output) == 1);
    assert(strcmp(output, expected) == 0);
    free(output);
}

static void invalid(const char *json)
{
    char *output = NULL;
    assert(cpapago_json_string(json, strlen(json), "translatedText", &output) == 0);
    assert(output == NULL);
}

int main(void)
{
    char *output = NULL;
    char **outputs = NULL;
    cpapago_error error;
    cpapago_parameter parameter = {"en", "ko", "hello"};
    cpapago_options options = {0};
    size_t i;
    const char *bad[] = {
        "", "null", "[]", "{}", "<html></html>",
        "{\"translatedText\":123}", "{\"translatedText\":null}",
        "{\"nested\":{\"translatedText\":\"wrong\"}}",
        "{\"translatedText\":\"ok\",}", "{\"translatedText\":\"ok\"}garbage",
        "{\"translatedText\":\"\\uD800\"}", "{\"translatedText\":\"\\uDC00\"}",
        "{\"translatedText\":\"\\u0000\"}", "{\"translatedText\":\"\\xFF\"}",
        "{\"translatedText\":\"ok\",\"n\":01}",
        "{\"translatedText\":\"ok\",\"n\":1.}",
        "{\"translatedText\":\"ok\",\"n\":1e}",
        "{\"translatedText\":\"ok\",\"arr\":[true,]}",
        "{\"translatedText\":\"\xC0\xAF\"}",
        "{\"translatedText\":\"ok\",\"translatedText\":false}"
    };
    valid("{\"translatedText\":\"안녕하세요\"}", "translatedText", "안녕하세요");
    valid("{\"translatedText\":\"\\uC548\\uB155\\uD83D\\uDE00\"}", "translatedText", "안녕😀");
    valid("{\"translatedText\":\"a\\n\\t\\r\\b\\f\\\\\\\"\\/b\"}", "translatedText", "a\n\t\r\b\f\\\"/b");
    valid("{\"translatedText\":\"\"}", "translatedText", "");
    valid("{\"translatedText\":\"first\",\"translatedText\":\"last\"}", "translatedText", "last");
    valid("{\"translatedText\":\"ok\",\"arr\":[null,true,false,-2.5e+3,{},[]]}", "translatedText", "ok");
    valid("{\"translatedText\":\"ok\",\"unknown\":\"\\u0000\"}", "translatedText", "ok");
    valid("{\"langCode\":\"zh-CN\"}", "langCode", "zh-CN");
    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) invalid(bad[i]);
    /* 모든 접두 잘림과 과도한 중첩을 검증합니다. */
    {
        const char *json = "{\"translatedText\":\"\\uD83D\\uDE00\"}";
        for (i = 0; i < strlen(json); ++i) {
            assert(cpapago_json_string(json, i, "translatedText", &output) == 0);
            assert(output == NULL);
        }
        char deep[512];
        size_t offset = (size_t)snprintf(deep, sizeof(deep), "{\"translatedText\":\"ok\",\"a\":");
        for (i = 0; i < 100; ++i) deep[offset++] = '[';
        deep[offset++] = '0';
        for (i = 0; i < 100; ++i) deep[offset++] = ']';
        deep[offset++] = '}'; deep[offset] = 0;
        invalid(deep);
    }
    /* 짧은 무작위 바이트 입력에서도 범위 밖 읽기나 오류 시 누수가 없는지 검사합니다. */
    {
        uint32_t state = 12345;
        unsigned char bytes[64];
        for (i = 0; i < 10000; ++i) {
            size_t j, length = i % sizeof(bytes);
            for (j = 0; j < length; ++j) {
                state = state * 1664525u + 1013904223u;
                bytes[j] = (unsigned char)(state >> 24);
            }
            int parsed = cpapago_json_string((const char *)bytes, length, "translatedText", &output);
            assert(parsed == 0 || parsed == 1);
            free(output); output = NULL;
        }
    }
    assert(cpapago_init() == CPAPAGO_OK);
    assert(cpapago_detect(NULL, NULL, &output, &error) == CPAPAGO_INVALID_ARGUMENT);
    assert(output == NULL && error.code == CPAPAGO_INVALID_ARGUMENT);
    assert(cpapago_translate(NULL, NULL, &output, NULL) == CPAPAGO_INVALID_ARGUMENT);
    assert(cpapago_translate(&parameter, NULL, NULL, &error) == CPAPAGO_INVALID_ARGUMENT);
    options.timeout_ms = -1;
    assert(cpapago_translate(&parameter, &options, &output, &error) == CPAPAGO_INVALID_ARGUMENT);
    options.timeout_ms = 0; options.base_url = "file:///tmp/invalid";
    assert(cpapago_translate(&parameter, &options, &output, &error) == CPAPAGO_INVALID_ARGUMENT);
    options.base_url = NULL; options.parallelism = 17;
    assert(cpapago_translate_many(&parameter, 1, &options, &outputs, &error) == CPAPAGO_INVALID_ARGUMENT);
    assert(cpapago_translate_many(NULL, 0, NULL, &outputs, &error) == CPAPAGO_OK && outputs == NULL);
    assert(cpapago_translate_many(&parameter, SIZE_MAX, NULL, &outputs, &error) == CPAPAGO_OUT_OF_MEMORY);
    cpapago_free_many(NULL, 0);
    cpapago_cleanup();
    puts("JSON parsing and public argument tests passed");
    return 0;
}
