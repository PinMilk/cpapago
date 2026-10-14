#ifndef CPAPAGO_H
#define CPAPAGO_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 함수 반환 상태입니다. 출력 포인터는 실패 시 NULL입니다. */
typedef enum {
    CPAPAGO_OK = 0,
    CPAPAGO_INVALID_ARGUMENT,
    CPAPAGO_OUT_OF_MEMORY,
    CPAPAGO_NETWORK_ERROR,
    CPAPAGO_HTTP_ERROR,
    CPAPAGO_INVALID_RESPONSE,
    CPAPAGO_INTERNAL_ERROR
} cpapago_status;

/** 호출자 소유 오류 정보입니다. error 인수에 NULL을 전달해도 됩니다. */
typedef struct {
    cpapago_status code;
    long http_status;
    char message[256];
} cpapago_error;

/** 모든 문자열은 널 종료된 UTF-8이며, 함수가 실행되는 동안 유효해야 합니다. */
typedef struct {
    const char *source; /* NULL 또는 "detect"이면 자동 감지합니다. */
    const char *target; /* 필수 대상 언어. zh-cn / zh-tw도 지원합니다. */
    const char *text;   /* 필수 원문. 특수문자는 폼 인코딩합니다. */
} cpapago_parameter;

/** NULL 또는 {0}으로 전달하면 기본값을 사용합니다. */
typedef struct {
    bool honorific;       /* 높임말 요청. 기본값 false. */
    bool verbose;         /* true이면 번역문 대신 전체 JSON 문자열 반환. */
    long timeout_ms;      /* HTTP 요청별 제한 시간. 0이면 30000ms. */
    size_t parallelism;   /* 다중 번역 동시 작업 수. 0이면 4, 최대 16. */
    const char *base_url; /* NULL이면 https://papago.naver.com. 테스트 서버용 변경 가능. */
} cpapago_options;

/** 스레드를 만들기 전에 한 번 호출하고, 모든 요청 완료 후 cleanup을 호출하세요. */
cpapago_status cpapago_init(void);
void cpapago_cleanup(void);

/** 성공 시 *output에 소문자 언어 코드를 할당합니다. 호출자는 free()해야 합니다. */
cpapago_status cpapago_detect(const char *text, const cpapago_options *options,
                              char **output, cpapago_error *error);

/** 성공 시 *output에 번역문 또는 전체 JSON을 할당합니다. 호출자는 free()해야 합니다. */
cpapago_status cpapago_translate(const cpapago_parameter *parameter,
                                 const cpapago_options *options,
                                 char **output, cpapago_error *error);

/**
 * 입력 순서대로 결과를 반환하는 병렬 다중 번역입니다.
 * 하나라도 실패하면 부분 결과를 해제하고 첫 실패 항목의 오류를 반환합니다.
 * count==0은 성공하며 *outputs==NULL입니다. 성공한 결과는 free_many로 해제합니다.
 * parameter/options 문자열은 호출이 끝날 때까지 수정하거나 해제하면 안 됩니다.
 */
cpapago_status cpapago_translate_many(const cpapago_parameter *parameters, size_t count,
                                      const cpapago_options *options,
                                      char ***outputs, cpapago_error *error);
void cpapago_free_many(char **outputs, size_t count);

#ifdef __cplusplus
}
#endif
#endif
