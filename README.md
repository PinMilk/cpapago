# cpapago

[`nodepapago`](../nodepapago)의 현재 번역·언어 감지 기능을 **C11**로 포팅한 비공식 파파고 라이브러리입니다.
원본 프로젝트는 수정하지 않았습니다. macOS/Linux에서 libcurl과 POSIX 스레드를 사용합니다.

> 파파고의 비공식 웹 API를 사용하므로 서비스 변경으로 동작이 멈출 수 있습니다.
> 실제 번역 문구는 파파고 모델에 따라 달라집니다. 원문이 파파고 서버로 전송되므로 민감 정보를 입력하지 마세요.

## 지원 기능

| nodepapago 기능 | C API / CLI |
| --- | --- |
| `Translator.detect(text)` | `cpapago_detect()` / `cpapago detect` |
| 단일 번역 | `cpapago_translate()` / `cpapago translate` |
| 원본 언어 생략 또는 `detect` | 자동 언어 감지 후 번역 |
| 매개변수 배열·다중 번역 | `cpapago_translate_many()` / `cpapago batch` |
| `honorific: true` | 옵션의 `honorific` / `--honorific` |
| `verbose: true` | 전체 JSON 응답 문자열 / `--verbose` |
| `zh-cn`, `zh-tw` | API의 `zh-CN`, `zh-TW`로 변환 |

현재 API에서 사용하지 않는 과거 HMAC-MD5 인증 코드는 포팅하지 않았습니다.
`verbose`는 JavaScript 객체 대신 **JSON 문자열**을 반환합니다.
다중 번역은 최대 16개의 작업자로 처리하고 결과 순서를 유지합니다. C API는 동기식이므로
작업자들이 완료된 뒤 반환하며, 하나라도 실패하면 부분 결과를 해제합니다.

언어 코드는 원본과 동일합니다: `ko`, `en`, `ja`, `zh-cn`, `zh-tw`, `hi`, `es`, `fr`,
`de`, `pt`, `vi`, `id`, `fa`, `ar`, `mm`, `th`, `ru`, `it`.
지원되는 번역 방향은 파파고 서버의 언어 쌍 제한에 따릅니다.

## 빌드

### 1. 의존성 준비

필수 도구는 **C11 컴파일러, Make, libcurl 개발 파일과 `curl-config`**입니다.
테스트를 실행하려면 Python 3도 필요합니다.

**macOS** — Xcode Command Line Tools를 설치하면 기본 컴파일러와 libcurl을 사용할 수 있습니다.
이미 설치되어 있다면 이 단계는 건너뜁니다.

```bash
xcode-select --install
```

**Ubuntu / Debian**:

```bash
sudo apt update
sudo apt install build-essential libcurl4-openssl-dev python3
```

**Fedora**:

```bash
sudo dnf install gcc make libcurl-devel python3
```

다른 Linux 배포판은 같은 도구에 해당하는 패키지를 설치하세요.
현재 구현은 POSIX 스레드를 사용하므로 Windows에서는 WSL의 Linux 환경에서 빌드할 수 있으며,
네이티브 MSVC 빌드는 지원하지 않습니다.

### 2. 프로젝트 디렉터리에서 빌드

내려받거나 복사한 `cpapago` 디렉터리의 **상위 디렉터리**에서 실행합니다.
개인 홈 경로나 특정 설치 위치는 필요하지 않습니다.

```bash
cd cpapago
make
```

이미 `Makefile`이 있는 프로젝트 루트에 있다면 `make`만 실행하면 됩니다.
이후의 CLI·예제·테스트 명령도 프로젝트 루트에서 실행하세요.

다른 위치에서 빌드하려면 `-C`로 디렉터리를 지정할 수 있습니다.
아래 `/path/to/cpapago`를 자신의 실제 프로젝트 경로로 바꾸세요.

```bash
make -C /path/to/cpapago
```

### 별도 설치한 libcurl 사용

Makefile은 기본적으로 PATH의 `curl-config`에서 헤더·링크 옵션을 가져옵니다.
macOS에서 시스템 libcurl 대신 Homebrew 버전을 사용하려면:

```bash
brew install curl
make CURL_CONFIG="$(brew --prefix curl)/bin/curl-config"
```

다른 libcurl 설치도 `make CURL_CONFIG=/path/to/curl-config`로 지정할 수 있습니다.
컴파일러는 `make CC=clang` 또는 `make CC=gcc`로 선택합니다.
이미 빌드한 뒤 컴파일러나 libcurl을 변경할 때는 `make clean` 후 다시 빌드하세요.

### 빌드 결과

생성 파일:

- `.build/libcpapago.a`: 정적 라이브러리
- `.build/cpapago`: 명령줄 프로그램
- `.build/basic`: 단일 번역 예제

공개 API는 [`include/cpapago.h`](include/cpapago.h)에 있습니다.

## 명령줄 사용

```bash
# 언어 감지: en
.build/cpapago detect 'So far, so bueno!'

# 원본 언어를 자동 감지하여 한국어로 번역
.build/cpapago translate ko 'So far, so good!'

# 원본 언어와 높임말 지정
.build/cpapago translate ko 'Morning, sir.' --source en --honorific

# 문자열 배열로 다중 번역 결과 출력
.build/cpapago batch ko 'Morning, sir.' "Morning, ma'am." --source en --honorific

# 전체 응답 객체의 JSON 배열 출력
.build/cpapago batch ko 'Morning!' 'Morning, baby!' --source en --verbose

# 중국어 번역
.build/cpapago translate ko '你好，很高兴认识你。' --source zh-cn
```

`--timeout 30000`으로 HTTP 요청별 제한 시간(ms), `--parallel 4`로 다중 번역 작업자 수를 지정할 수 있습니다.
기본값은 30초와 4개입니다. `--` 뒤의 문자열은 옵션으로 해석하지 않습니다.
`--base-url`은 로컬 테스트 서버 등으로 요청 주소를 바꾸는 옵션입니다. 보통 지정하지 않습니다.

## C 라이브러리 사용

```c
#include "cpapago.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    cpapago_parameter parameter = {NULL, "ko", "So far, so good!"};
    cpapago_options options = {0};
    cpapago_error error = {0};
    char *translated = NULL;

    if (cpapago_init() != CPAPAGO_OK) return 1;
    cpapago_status status = cpapago_translate(&parameter, &options, &translated, &error);
    if (status == CPAPAGO_OK) puts(translated);
    else fprintf(stderr, "%s\n", error.message);

    free(translated);
    cpapago_cleanup();
    return status == CPAPAGO_OK ? 0 : 1;
}
```

위 예제를 `myapp.c`로 저장했다면:

```bash
cc -std=c11 -Iinclude myapp.c .build/libcpapago.a $(curl-config --libs) -pthread -o .build/myapp
```

### 메모리와 초기화

- `cpapago_init()`은 요청 스레드를 만들기 전에 한 번 호출합니다.
- `cpapago_cleanup()`은 모든 요청과 작업자 스레드가 종료된 뒤 호출합니다.
- 단일 번역·언어 감지 결과는 호출자가 `free()`해야 합니다.
- 다중 결과는 `cpapago_free_many(outputs, count)`로 한 번에 해제합니다.
- 실패 시 출력 포인터는 `NULL`이며, `cpapago_error`에 상태·HTTP 코드·설명이 기록됩니다.
- 입력 문자열과 옵션은 함수 실행 중 해제하거나 수정하면 안 됩니다.

### 다중 번역

```c
cpapago_parameter parameters[] = {
    {"en", "ko", "Morning, sir."},
    {"en", "ko", "Morning, ma'am."}
};
cpapago_options options = {.honorific = true};
cpapago_error error = {0};
char **outputs = NULL;

/* cpapago_init()을 먼저 호출한 상태에서 사용합니다. */
if (cpapago_translate_many(parameters, 2, &options, &outputs, &error) == CPAPAGO_OK) {
    puts(outputs[0]);
    puts(outputs[1]);
    cpapago_free_many(outputs, 2);
}
```

## 테스트

```bash
# 오프라인: JSON, 인코딩, 병렬 작업, HTTP 오류, 시간 초과, 인수 검증
make test

# Clang AddressSanitizer / UndefinedBehaviorSanitizer 메모리 검사
make test-sanitize

# 실제 파파고 API: 네트워크 연결 필요, 원문이 외부 서버로 전송됨
make test-live
```

JSON 처리기는 UTF-8·유니코드 escape·중첩 문법을 검사합니다. 비정상 응답, 출력 문자열의
내장 널 문자, 64단계를 넘는 중첩, 8 MiB를 넘는 HTTP 응답을 거부합니다.
TLS 인증서 검증은 비활성화하지 않으며 자동 리다이렉트도 사용하지 않습니다.

## 파일 구성

```text
include/cpapago.h     공개 타입·함수·메모리 계약
src/cpapago.c         libcurl 요청, 폼 인코딩, 자동 감지, 병렬 번역
src/json.c           JSON 응답 검증과 문자열 추출
src/main.c           명령줄 프로그램
examples/basic.c     C 호출 예제
tests/               오프라인 단위·HTTP 테스트 및 실제 API 테스트
Makefile             빌드 및 테스트
```

## 라이선스

MIT. 원본 nodepapago의 저작권 고지는 [`LICENSE`](LICENSE)에 유지했습니다.
