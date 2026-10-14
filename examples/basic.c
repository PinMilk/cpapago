#include "cpapago.h"
#include <stdio.h>
#include <stdlib.h>

/* nodepapago README의 단일 번역 예제와 동일한 입력입니다. */
int main(void)
{
    cpapago_parameter parameter = {NULL, "ko", "So far, so good!"};
    cpapago_options options = {0};
    cpapago_error error = {0};
    char *translated = NULL;
    cpapago_status status;
    if (cpapago_init() != CPAPAGO_OK) return 1;
    status = cpapago_translate(&parameter, &options, &translated, &error);
    if (status == CPAPAGO_OK) puts(translated);
    else fprintf(stderr, "%s\n", error.message);
    free(translated);
    cpapago_cleanup();
    return status == CPAPAGO_OK ? 0 : 1;
}
