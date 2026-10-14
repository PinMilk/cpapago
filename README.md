# cpapago

[한국어](README.md) · [English](README.en.md) · [日本語](README.JA.md)

[`nodepapago`](https://github.com/PinMilk/nodepapago)의 번역·언어 감지 기능을 **C11**로 포팅한 비공식 파파고 라이브러리입니다.
macOS/Linux에서 동작하며, **C API와 CLI**를 제공합니다. HTTP 통신에는 libcurl, 병렬 처리에는 POSIX 스레드를 사용합니다.

> 비공식 웹 API이므로 파파고의 서비스 변경에 따라 동작이 멈출 수 있습니다.
> 번역 문구는 모델에 따라 달라지며, 입력은 파파고 서버로 전송됩니다. 민감 정보는 입력하지 마세요.

## 목차

- [빠른 시작](#빠른-시작)
- [빌드 환경](#빌드-환경)
- [CLI 사용](#cli-사용)
- [C 라이브러리 사용](#c-라이브러리-사용)
- [언어 코드](#언어-코드)
- [테스트](#테스트)
- [성능](#성능)
- [제한 및 보안](#제한-및-보안)
- [프로젝트 구성](#프로젝트-구성)

## 빠른 시작

[필수 도구](#빌드-환경)를 설치한 뒤 실행하세요.

```bash
git clone https://github.com/PinMilk/cpapago.git
cd cpapago
make

# 원본 언어를 자동 감지해 한국어로 번역
./.build/cpapago translate ko 'So far, so good!'

# 언어 감지: en
./.build/cpapago detect 'So far, so bueno!'
```

이미 저장소를 내려받았다면 복제 단계는 생략합니다. 이후 명령은 **프로젝트 루트**에서 실행하세요.

| 빌드 결과 | 용도 |
| --- | --- |
| `.build/cpapago` | 명령줄 프로그램 |
| `.build/libcpapago.a` | 정적 라이브러리 |
| `.build/basic` | 단일 번역 실행 예제 |

## 빌드 환경

필수: **C11 컴파일러, Make, libcurl 개발 파일, `curl-config`**.
테스트에는 Python 3이 추가로 필요하며, 메모리 검사에는 Clang과 sanitizer 지원이 필요합니다.

### 의존성 설치

**macOS** — Xcode Command Line Tools를 설치합니다. 이미 설치했다면 생략하세요.

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

다른 Linux 배포판에서는 같은 도구에 해당하는 패키지를 설치하세요.
Windows는 WSL의 Linux 환경에서 빌드할 수 있으며, **네이티브 MSVC 빌드는 지원하지 않습니다**.

### 빌드 설정

```bash
make                               # 기본 빌드
make CC=clang                      # 컴파일러 지정
make -C /path/to/cpapago            # 다른 디렉터리에서 빌드
```

`/path/to/cpapago`는 실제 프로젝트 경로로 바꾸세요. Makefile은 PATH의 `curl-config`에서 헤더·링크 옵션을 가져옵니다.
macOS에서 Homebrew libcurl을 사용하려면:

```bash
brew install curl
make CURL_CONFIG="$(brew --prefix curl)/bin/curl-config"
```

다른 설치도 `make CURL_CONFIG=/path/to/curl-config`로 지정할 수 있습니다.
이미 빌드한 상태에서 컴파일러·libcurl·컴파일 옵션을 변경하면 `make clean` 후 다시 빌드하세요.

## CLI 사용

```text
cpapago detect TEXT
cpapago translate TARGET TEXT [OPTIONS]
cpapago batch TARGET TEXT... [OPTIONS]
```

### 사용 예제

```bash
# 원본 언어 지정 및 높임말 번역
./.build/cpapago translate ko 'Morning, sir.' --source en --honorific

# 입력 순서대로 문자열 배열 출력
./.build/cpapago batch ko 'Morning, sir.' "Morning, ma'am." --source en --honorific

# 전체 API 응답 객체의 JSON 배열 출력
./.build/cpapago batch ko 'Morning!' 'Morning, baby!' --source en --verbose

# 중국어 코드의 대소문자 변환은 라이브러리가 처리
./.build/cpapago translate ko '你好，很高兴认识你。' --source zh-cn
```

| 옵션 | 설명 | 기본값 |
| --- | --- | --- |
| `--source CODE` | 원본 언어. 생략하거나 `detect`이면 자동 감지 | 자동 감지 |
| `--honorific` | 높임말 번역 요청. 지원 여부는 언어 쌍에 따름 | 비활성화 |
| `--verbose` | 번역문 대신 전체 API 응답 JSON 출력 | 비활성화 |
| `--timeout MS` | **HTTP 요청별** 제한 시간(ms), 양의 정수 | `30000` |
| `--parallel N` | 다중 번역 작업자 수, `1`~`16` | `4` |
| `--base-url URL` | API 서버 주소 변경. 로컬 HTTP 테스트 등에 사용 | `https://papago.naver.com` |
| `--` | 이후 인수를 옵션이 아닌 텍스트로 처리 | — |

언어 감지는 소문자 언어 코드, 단일 번역은 문자열, 다중 번역은 JSON 배열을 출력합니다.
`--verbose`이면 단일 번역은 JSON 객체, 다중 번역은 객체 배열입니다.
오류는 표준 오류로 출력하며 종료 코드는 성공 `0`, 요청 실패 `1`, 사용법 오류 `2`입니다.

## C 라이브러리 사용

공개 타입·함수와 메모리 계약은 [`include/cpapago.h`](include/cpapago.h)에 있습니다.

| 함수 | 성공 시 반환 내용 | 결과 해제 |
| --- | --- | --- |
| `cpapago_detect()` | 소문자 언어 코드 | `free(output)` |
| `cpapago_translate()` | 번역문 또는 전체 JSON 문자열 | `free(output)` |
| `cpapago_translate_many()` | 입력 순서를 유지하는 결과 문자열 배열 | `cpapago_free_many(outputs, count)` |

함수의 반환 상태가 `CPAPAGO_OK`인지 확인한 뒤 출력 포인터를 사용하세요.

### 단일 번역

아래 내용을 `myapp.c`로 저장하세요.

```c
#include "cpapago.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    cpapago_parameter parameter = {
        .source = NULL,  /* 자동 감지 */
        .target = "ko",
        .text = "So far, so good!"
    };
    cpapago_options options = {0};
    cpapago_error error = {0};
    char *output = NULL;

    if (cpapago_init() != CPAPAGO_OK) return 1;
    cpapago_status status = cpapago_translate(&parameter, &options, &output, &error);
    if (status == CPAPAGO_OK) puts(output);
    else fprintf(stderr, "%s\n", error.message);

    free(output);
    cpapago_cleanup();
    return status == CPAPAGO_OK ? 0 : 1;
}
```

라이브러리를 빌드한 후 링크합니다.

```bash
cc -std=c11 -Iinclude myapp.c .build/libcpapago.a $(curl-config --libs) -pthread -o .build/myapp
./.build/myapp
```

별도 libcurl로 빌드했다면 링크할 때도 **동일한 `curl-config`**를 사용하세요.

### 다중 번역

다음은 `cpapago_init()` 이후, `cpapago_cleanup()` 이전에 사용하는 코드입니다.

```c
cpapago_parameter parameters[] = {
    {"en", "ko", "Morning, sir."},
    {"en", "ko", "Morning, ma'am."}
};
cpapago_options options = {.honorific = true};
cpapago_error error = {0};
char **outputs = NULL;

if (cpapago_translate_many(parameters, 2, &options, &outputs, &error) == CPAPAGO_OK) {
    puts(outputs[0]);
    puts(outputs[1]);
} else {
    fprintf(stderr, "%s\n", error.message);
}
cpapago_free_many(outputs, 2);
```

### C 옵션

`cpapago_options`는 `NULL` 또는 `{0}`이면 기본값을 사용합니다.

| 필드 | 설명 | 기본값 |
| --- | --- | --- |
| `honorific` | 높임말 번역 요청 | `false` |
| `verbose` | 번역문 대신 전체 JSON 문자열 반환 | `false` |
| `timeout_ms` | HTTP 요청별 제한 시간. `0`은 기본값 | `30000` |
| `parallelism` | 다중 번역 작업자 수. `0`은 기본값, 최대 `16` | `4` |
| `base_url` | API 서버 주소. `NULL`은 기본값 | `https://papago.naver.com` |

### 메모리와 오류 처리

- 입력은 **널 종료된 UTF-8 문자열**이며, 호출이 끝날 때까지 문자열·옵션을 수정하거나 해제하지 마세요.
- `cpapago_init()`은 요청 스레드를 만들기 전에 한 번, `cpapago_cleanup()`은 모든 작업이 끝난 뒤 호출합니다.
- 성공 결과는 위 표의 함수로 해제합니다. 실패 시 출력 포인터는 `NULL`입니다.
- `cpapago_error`에는 상태·HTTP 코드·설명이 기록됩니다. 오류 정보가 필요 없으면 `NULL`을 전달할 수 있습니다.
- 다중 번역은 동기식입니다. 작업자들이 완료된 뒤 반환하며, 하나라도 실패하면 부분 결과를 모두 해제합니다.
  항목 수가 `0`이면 성공하고 결과는 `NULL`입니다.
- C의 `verbose` 결과는 JavaScript 객체가 아닌 **JSON 문자열**입니다. 필요한 경우 별도로 파싱하세요.

## 언어 코드

| 코드 | 언어 | 코드 | 언어 |
| --- | --- | --- | --- |
| `ko` | 한국어 | `en` | 영어 |
| `ja` | 일본어 | `zh-cn` | 중국어(간체) |
| `zh-tw` | 중국어(번체) | `hi` | 힌디어 |
| `es` | 스페인어 | `fr` | 프랑스어 |
| `de` | 독일어 | `pt` | 포르투갈어 |
| `vi` | 베트남어 | `id` | 인도네시아어 |
| `fa` | 페르시아어 | `ar` | 아랍어 |
| `mm` | 미얀마어 | `th` | 태국어 |
| `ru` | 러시아어 | `it` | 이탈리아어 |

지원되는 번역 방향은 파파고 서버의 언어 쌍 제한에 따릅니다.
중국어 코드는 요청 시 `zh-CN`·`zh-TW`로 변환하고, 감지 결과는 소문자로 반환합니다.

## 테스트

| 명령 | 검증 범위 | 외부 API 호출 |
| --- | --- | --- |
| `make test` | JSON·인수 검증, 로컬 HTTP 인코딩·병렬 처리·오류·시간 초과 | 없음 |
| `make test-sanitize` | 위 테스트를 Clang ASan/UBSan으로 실행 | 없음 |
| `make test-live` | 실제 언어 감지·번역·높임말·전체 JSON 응답 | 있음 |

실제 API 테스트는 네트워크 연결이 필요하며, 번역 문구가 아닌 반환 형식을 중심으로 검증합니다.

## 성능

폼·JSON 처리의 임시 할당을 줄이고, 자동 감지·다중 번역에서는 작업자별 HTTP 연결을 재사용합니다.
원본 언어를 이미 안다면 `--source` 또는 C 매개변수의 `source`를 지정해 감지 요청을 생략할 수 있습니다.
측정 조건, 변경 전후 수치와 재현 방법은 [PERFORMANCE.md](PERFORMANCE.md)를 참고하세요.
실제 번역 시간은 네트워크와 파파고 서버의 응답 시간에 따라 달라집니다.

## 제한 및 보안

- TLS 인증서 검증을 유지하며 자동 리다이렉트는 사용하지 않습니다.
- JSON 문법·UTF-8·유니코드 escape를 검증합니다. 출력의 내장 널 문자, 과도한 중첩(64단계 초과),
  8 MiB 초과 HTTP 응답은 거부합니다.
- 시간 제한은 요청 단위입니다. 자동 감지는 감지와 번역 요청을 각각 수행하므로 전체 호출 시간은 더 길 수 있습니다.
- 현재 API가 사용하지 않는 과거 HMAC-MD5 인증 코드는 포함하지 않았습니다.

## 프로젝트 구성

```text
include/cpapago.h  공개 타입·함수
src/cpapago.c     HTTP 요청·자동 감지·병렬 번역
src/json.c        JSON 검증·문자열 추출
src/main.c        CLI
examples/basic.c  단일 번역 예제
tests/            단위·로컬 HTTP·실제 API 테스트
Makefile          빌드·테스트
```

## 라이선스

MIT. 원본 nodepapago의 저작권 고지는 [`LICENSE`](LICENSE)에 유지했습니다.
