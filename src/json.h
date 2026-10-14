#ifndef CPAPAGO_JSON_H
#define CPAPAGO_JSON_H
#include <stddef.h>

/* JSON 전체를 검증하고 최상위 객체의 문자열 필드를 추출합니다.
 * 1: 성공, 0: 형식/필드 오류, -1: 메모리 부족. 성공한 문자열은 free()해야 합니다. */
int cpapago_json_string(const char *document, size_t length, const char *field, char **output);
#endif
