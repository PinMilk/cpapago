# cpapago

[한국어](README.md) · [English](README.en.md) · [日本語](README.JA.md)

An unofficial Papago library ported from [`nodepapago`](https://github.com/PinMilk/nodepapago) to **C11**.
It provides a **C API and CLI** for macOS/Linux, using libcurl for HTTP and POSIX threads for parallel translation.

> This project uses an unofficial web API and may stop working when Papago changes its service.
> Translations may change with the model. Input is sent to Papago's servers; do not submit sensitive information.

## Contents

- [Quick start](#quick-start)
- [Build requirements](#build-requirements)
- [CLI usage](#cli-usage)
- [C library usage](#c-library-usage)
- [Language codes](#language-codes)
- [Tests](#tests)
- [Performance](#performance)
- [Limits and security](#limits-and-security)
- [Project layout](#project-layout)

## Quick start

Install the [required tools](#build-requirements) first.

```bash
git clone https://github.com/PinMilk/cpapago.git
cd cpapago
make

# Detect the source language and translate into Korean
./.build/cpapago translate ko 'So far, so good!'

# Detect the language: en
./.build/cpapago detect 'So far, so bueno!'
```

Skip cloning if you already have the repository. Run subsequent commands from the **project root**.

| Build output | Purpose |
| --- | --- |
| `.build/cpapago` | Command-line program |
| `.build/libcpapago.a` | Static library |
| `.build/basic` | Single-translation example executable |

## Build requirements

Required: **a C11 compiler, Make, libcurl development files, and `curl-config`**.
Tests also require Python 3. Memory checks require Clang with sanitizer support.

### Install dependencies

**macOS** — Install Xcode Command Line Tools. Skip this step if already installed.

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

For other Linux distributions, install the equivalent packages.
On Windows, use a Linux environment under WSL; **native MSVC builds are not supported**.

### Build settings

```bash
make                               # Default build
make CC=clang                      # Select a compiler
make -C /path/to/cpapago            # Build from another directory
```

Replace `/path/to/cpapago` with your actual project path. The Makefile obtains header and linker flags from `curl-config` on PATH.
To use Homebrew libcurl on macOS:

```bash
brew install curl
make CURL_CONFIG="$(brew --prefix curl)/bin/curl-config"
```

For another installation, use `make CURL_CONFIG=/path/to/curl-config`.
If you change the compiler, libcurl, or compiler flags after building, run `make clean` before rebuilding.

## CLI usage

```text
cpapago detect TEXT
cpapago translate TARGET TEXT [OPTIONS]
cpapago batch TARGET TEXT... [OPTIONS]
```

### Examples

```bash
# Specify the source language and request honorific translation
./.build/cpapago translate ko 'Morning, sir.' --source en --honorific

# Print an array of strings in input order
./.build/cpapago batch ko 'Morning, sir.' "Morning, ma'am." --source en --honorific

# Print a JSON array of complete API response objects
./.build/cpapago batch ko 'Morning!' 'Morning, baby!' --source en --verbose

# The library converts Chinese language code capitalization
./.build/cpapago translate ko '你好，很高兴认识你。' --source zh-cn
```

| Option | Description | Default |
| --- | --- | --- |
| `--source CODE` | Source language; omit or use `detect` for automatic detection | Automatic detection |
| `--honorific` | Request honorific translation; availability depends on the language pair | Disabled |
| `--verbose` | Print complete API response JSON instead of translated text | Disabled |
| `--timeout MS` | **Per-HTTP-request** timeout in milliseconds; positive integer | `30000` |
| `--parallel N` | Number of batch workers, from `1` to `16` | `4` |
| `--base-url URL` | Override the API server, for example for local HTTP tests | `https://papago.naver.com` |
| `--` | Treat subsequent arguments as text, not options | — |

Detection prints a lowercase language code. Single translation prints text; batch translation prints a JSON array.
With `--verbose`, single translation prints a JSON object and batch translation prints an array of objects.
Errors go to standard error. Exit codes are `0` for success, `1` for request failure, and `2` for invalid usage.

## C library usage

See [`include/cpapago.h`](include/cpapago.h) for public types, functions, and memory ownership contracts.

| Function | Successful output | Release with |
| --- | --- | --- |
| `cpapago_detect()` | Lowercase language code | `free(output)` |
| `cpapago_translate()` | Translated text or complete JSON string | `free(output)` |
| `cpapago_translate_many()` | Array of result strings in input order | `cpapago_free_many(outputs, count)` |

Check that the returned status is `CPAPAGO_OK` before using the output pointer.

### Single translation

Save the following as `myapp.c`:

```c
#include "cpapago.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    cpapago_parameter parameter = {
        .source = NULL,  /* Automatic detection */
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

After building the library, compile and link:

```bash
cc -std=c11 -Iinclude myapp.c .build/libcpapago.a $(curl-config --libs) -pthread -o .build/myapp
./.build/myapp
```

If you built with a separate libcurl installation, use the **same `curl-config`** when linking.

### Batch translation

Use this code after `cpapago_init()` and before `cpapago_cleanup()`:

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

### C options

Pass `NULL` or a zero-initialized `cpapago_options` (`{0}`) to use defaults.

| Field | Description | Default |
| --- | --- | --- |
| `honorific` | Request honorific translation | `false` |
| `verbose` | Return complete JSON instead of translated text | `false` |
| `timeout_ms` | Per-HTTP-request timeout; `0` selects the default | `30000` |
| `parallelism` | Batch workers; `0` selects the default, maximum `16` | `4` |
| `base_url` | API server address; `NULL` selects the default | `https://papago.naver.com` |

### Memory and error handling

- Inputs are **NUL-terminated UTF-8 strings**. Do not modify or free input strings or options until the call completes.
- Call `cpapago_init()` once before creating request threads and `cpapago_cleanup()` after all work completes.
- Release successful outputs using the functions listed above. Output pointers are `NULL` on failure.
- `cpapago_error` contains a status, HTTP code, and message. Pass `NULL` if you do not need error details.
- Batch translation is synchronous: it waits for workers to finish. Any failure releases all partial results.
  A count of `0` succeeds with a `NULL` output.
- In C, `verbose` returns a **JSON string**, not a JavaScript object. Parse it separately if needed.

## Language codes

| Code | Language | Code | Language |
| --- | --- | --- | --- |
| `ko` | Korean | `en` | English |
| `ja` | Japanese | `zh-cn` | Chinese (Simplified) |
| `zh-tw` | Chinese (Traditional) | `hi` | Hindi |
| `es` | Spanish | `fr` | French |
| `de` | German | `pt` | Portuguese |
| `vi` | Vietnamese | `id` | Indonesian |
| `fa` | Persian | `ar` | Arabic |
| `mm` | Burmese | `th` | Thai |
| `ru` | Russian | `it` | Italian |

Translation directions depend on the language pairs supported by Papago.
Chinese codes are converted to `zh-CN`/`zh-TW` for requests; detection returns lowercase codes.

## Tests

| Command | Coverage | External API calls |
| --- | --- | --- |
| `make test` | JSON/argument validation; local HTTP encoding, concurrency, errors, and timeouts | No |
| `make test-sanitize` | The same tests under Clang ASan/UBSan | No |
| `make test-live` | Live detection, translation, honorifics, and complete JSON responses | Yes |

Live tests require a network connection and primarily check return formats, not exact translated wording.

## Performance

The implementation reduces temporary allocations in form/JSON handling and reuses per-worker HTTP connections for detection and batch translation.
If the source language is known, set `--source` or the C parameter's `source` to skip detection requests.
See [PERFORMANCE.md](PERFORMANCE.md) (Korean) for measurement conditions, results, and reproduction steps.
Actual translation times depend on the network and Papago's response times.

## Limits and security

- TLS certificate verification remains enabled; automatic redirects are disabled.
- JSON syntax, UTF-8, and Unicode escapes are validated. Embedded NULs in output, nesting beyond 64 levels,
  and HTTP responses larger than 8 MiB are rejected.
- Timeouts apply per request. Automatic detection makes separate detection and translation requests, so a complete call can take longer.
- Legacy HMAC-MD5 authentication code is not included because the current API does not use it.

## Project layout

```text
include/cpapago.h  Public types and functions
src/cpapago.c     HTTP, automatic detection, and parallel translation
src/json.c        JSON validation and string extraction
src/main.c        CLI
examples/basic.c  Single-translation example
tests/            Unit, local HTTP, and live API tests
Makefile          Build and test targets
```

## License

MIT. The original nodepapago copyright notice is preserved in [`LICENSE`](LICENSE).
