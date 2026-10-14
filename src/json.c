/*
 * API 응답의 필드 추출을 위한 JSON 검사기입니다. 외부 JSON 라이브러리는 필요 없습니다.
 * 문자열 escape, UTF-8, UTF-16 surrogate pair, 숫자 문법과 중첩 값을 검증합니다.
 * 정규식으로 JSON을 읽지 않으며, 중첩 깊이를 64로 제한해 스택 고갈을 방지합니다.
 */
#include "json.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const unsigned char *cursor, *end;
    unsigned depth;
    int memory_error;
} parser;

static void whitespace(parser *p)
{
    while (p->cursor < p->end && (*p->cursor == ' ' || *p->cursor == '\n' ||
           *p->cursor == '\r' || *p->cursor == '\t')) ++p->cursor;
}

static int hex4(parser *p, uint32_t *value)
{
    unsigned i;
    *value = 0;
    if ((size_t)(p->end - p->cursor) < 4) return 0;
    for (i = 0; i < 4; ++i) {
        unsigned char c = *p->cursor++;
        unsigned digit;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else return 0;
        *value = (*value << 4) | digit;
    }
    return 1;
}

/* 출력이 NULL이면 검증만 합니다. 필요할 때만 문자열 버퍼를 할당합니다. */
static int string(parser *p, char **output, size_t *length)
{
    char *buffer = NULL;
    size_t used = 0;
    if (p->cursor == p->end || *p->cursor++ != '"') return 0;
    if (output) {
        buffer = malloc((size_t)(p->end - p->cursor) + 1);
        if (!buffer) { p->memory_error = 1; return 0; }
    }
    while (p->cursor < p->end) {
        uint32_t code = *p->cursor++;
        unsigned remaining = 0;
        uint32_t minimum = 0;
        if (code == '"') {
            if (output) { buffer[used] = '\0'; *output = buffer; }
            if (length) *length = used;
            return 1;
        }
        if (code < 0x20) break;
        if (code == '\\') {
            if (p->cursor == p->end) break;
            switch (*p->cursor++) {
                case '"': code = '"'; break;
                case '\\': code = '\\'; break;
                case '/': code = '/'; break;
                case 'b': code = '\b'; break;
                case 'f': code = '\f'; break;
                case 'n': code = '\n'; break;
                case 'r': code = '\r'; break;
                case 't': code = '\t'; break;
                case 'u': {
                    uint32_t low;
                    if (!hex4(p, &code)) goto invalid;
                    if (code >= 0xD800 && code <= 0xDBFF) {
                        if ((size_t)(p->end - p->cursor) < 6 || p->cursor[0] != '\\' || p->cursor[1] != 'u') goto invalid;
                        p->cursor += 2;
                        if (!hex4(p, &low) || low < 0xDC00 || low > 0xDFFF) goto invalid;
                        code = 0x10000 + ((code - 0xD800) << 10) + low - 0xDC00;
                    } else if (code >= 0xDC00 && code <= 0xDFFF) goto invalid;
                    break;
                }
                default: goto invalid;
            }
        } else if (code >= 0x80) {
            if (code >= 0xC2 && code <= 0xDF) { code &= 0x1F; remaining = 1; minimum = 0x80; }
            else if (code >= 0xE0 && code <= 0xEF) { code &= 0x0F; remaining = 2; minimum = 0x800; }
            else if (code >= 0xF0 && code <= 0xF4) { code &= 0x07; remaining = 3; minimum = 0x10000; }
            else goto invalid;
            while (remaining-- > 0) {
                if (p->cursor == p->end || (*p->cursor & 0xC0) != 0x80) goto invalid;
                code = (code << 6) | (*p->cursor++ & 0x3F);
            }
            if (code < minimum || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) goto invalid;
        }
        /* escape로 얻은 유니코드도 UTF-8로 변환합니다. 원래 JSON 길이를 넘지 않습니다. */
        if (code < 0x80) { if (buffer) buffer[used] = (char)code; ++used; }
        else {
            unsigned count = code < 0x800 ? 2 : code < 0x10000 ? 3 : 4;
            unsigned i;
            if (buffer) {
                buffer[used] = (char)((count == 2 ? 0xC0 : count == 3 ? 0xE0 : 0xF0) | (code >> (6 * (count - 1))));
                for (i = 1; i < count; ++i) buffer[used + i] = (char)(0x80 | ((code >> (6 * (count - 1 - i))) & 0x3F));
            }
            used += count;
        }
    }
invalid:
    free(buffer);
    return 0;
}

static int value(parser *p);

static int compound(parser *p, int object)
{
    unsigned char end = object ? '}' : ']';
    if (++p->depth > 64) return 0;
    ++p->cursor;
    whitespace(p);
    if (p->cursor < p->end && *p->cursor == end) { ++p->cursor; --p->depth; return 1; }
    for (;;) {
        if (object) {
            if (!string(p, NULL, NULL)) return 0;
            whitespace(p);
            if (p->cursor == p->end || *p->cursor++ != ':') return 0;
        }
        if (!value(p)) return 0;
        whitespace(p);
        if (p->cursor == p->end) return 0;
        if (*p->cursor == end) { ++p->cursor; --p->depth; return 1; }
        if (*p->cursor++ != ',') return 0;
        whitespace(p);
    }
}

static int number(parser *p)
{
    if (*p->cursor == '-') ++p->cursor;
    if (p->cursor == p->end) return 0;
    if (*p->cursor == '0') ++p->cursor;
    else {
        if (*p->cursor < '1' || *p->cursor > '9') return 0;
        do { ++p->cursor; } while (p->cursor < p->end && *p->cursor >= '0' && *p->cursor <= '9');
    }
    if (p->cursor < p->end && *p->cursor == '.') {
        ++p->cursor;
        if (p->cursor == p->end || *p->cursor < '0' || *p->cursor > '9') return 0;
        do { ++p->cursor; } while (p->cursor < p->end && *p->cursor >= '0' && *p->cursor <= '9');
    }
    if (p->cursor < p->end && (*p->cursor == 'e' || *p->cursor == 'E')) {
        ++p->cursor;
        if (p->cursor < p->end && (*p->cursor == '+' || *p->cursor == '-')) ++p->cursor;
        if (p->cursor == p->end || *p->cursor < '0' || *p->cursor > '9') return 0;
        do { ++p->cursor; } while (p->cursor < p->end && *p->cursor >= '0' && *p->cursor <= '9');
    }
    return 1;
}

static int value(parser *p)
{
    const char *literal;
    size_t length;
    whitespace(p);
    if (p->cursor == p->end) return 0;
    switch (*p->cursor) {
        case '"': return string(p, NULL, NULL);
        case '{': return compound(p, 1);
        case '[': return compound(p, 0);
        case 't': literal = "true"; break;
        case 'f': literal = "false"; break;
        case 'n': literal = "null"; break;
        default: return number(p);
    }
    length = strlen(literal);
    if ((size_t)(p->end - p->cursor) < length || memcmp(p->cursor, literal, length)) return 0;
    p->cursor += length;
    return 1;
}

int cpapago_json_string(const char *document, size_t length, const char *field, char **output)
{
    parser p = {(const unsigned char *)document, (const unsigned char *)document + length, 1, 0};
    char *found = NULL;
    size_t found_length = 0;
    *output = NULL;
    whitespace(&p);
    if (p.cursor == p.end || *p.cursor++ != '{') return 0;
    whitespace(&p);
    if (p.cursor < p.end && *p.cursor == '}') return 0;
    for (;;) {
        char *key = NULL;
        size_t key_length;
        int match;
        if (!string(&p, &key, &key_length)) goto invalid;
        match = key_length == strlen(field) && memcmp(key, field, key_length) == 0;
        free(key);
        whitespace(&p);
        if (p.cursor == p.end || *p.cursor++ != ':') goto invalid;
        whitespace(&p);
        if (match) {
            /* 중복 키는 JavaScript JSON.parse처럼 마지막 값이 우선합니다. */
            free(found); found = NULL;
            if (p.cursor < p.end && *p.cursor == '"') {
                if (!string(&p, &found, &found_length)) goto invalid;
            } else if (!value(&p)) goto invalid;
        } else if (!value(&p)) goto invalid;
        whitespace(&p);
        if (p.cursor == p.end) goto invalid;
        if (*p.cursor == '}') { ++p.cursor; break; }
        if (*p.cursor++ != ',') goto invalid;
        whitespace(&p);
    }
    whitespace(&p);
    /* C 문자열로 표현할 수 없는 출력의 내장 널 문자도 거부합니다. */
    if (p.cursor != p.end || !found || memchr(found, 0, found_length)) goto invalid;
    *output = found;
    return 1;
invalid:
    free(found);
    return p.memory_error ? -1 : 0;
}
