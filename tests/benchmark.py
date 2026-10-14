"""최적화 전/후의 CPU 처리와 로컬 HTTP 호출을 별도로 비교합니다.

python3 tests/benchmark.py --baseline /path/to/before-optimization
서버 응답 시간을 통제하기 위해 외부 파파고 대신 로컬 HTTP/1.1 서버를 사용합니다.
프로세스 시작/컴파일 시간은 측정에서 제외합니다. 입력 결과도 함께 검증합니다.
"""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import socket
import statistics
import subprocess
import tempfile
import threading
from urllib.parse import parse_qs


ROOT = Path(__file__).resolve().parents[1]

CPU_SOURCE = r'''
#define _POSIX_C_SOURCE 200809L
#include <time.h>
#include <assert.h>
#include <stdio.h>
#include "@SOURCE@/src/cpapago.c"
#define main cpapago_cli_main
#include "@SOURCE@/src/main.c"
#undef main

static double now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

int main(void) {
    const char *keys[] = {"dict", "dictDisplay", "honorific", "useGlossary", "source", "target", "text"};
    const char *values[] = {"true", "30", "false", "false", "en", "ko", "Hello & 100% + 안녕 😀"};
    char text[32769], chunk[16384];
    size_t checksum = 0;
    memset(text, 'a', sizeof(text) - 1); text[sizeof(text) - 1] = 0;
    memset(chunk, 'b', sizeof(chunk));
    assert(cpapago_init() == CPAPAGO_OK);
    assert(freopen("/dev/null", "w", stdout));
    for (int sample = 0; sample < 6; ++sample) {
        double start = now();
        for (int i = 0; i < 200000; ++i) {
            char *body = make_form(keys, values, 7);
            assert(body); checksum += strlen(body); free(body);
        }
        double ns = (now() - start) * 1e9 / 200000;
        if (sample) fprintf(stderr, "form,%.3f\n", ns);

        start = now();
        for (int i = 0; i < 3000; ++i) {
            char *result = NULL;
            assert(cpapago_json_string(@JSON@, sizeof(@JSON@) - 1, "translatedText", &result) == 1);
            assert(strcmp(result, "안녕하세요 😀") == 0);
            checksum += strlen(result); free(result);
        }
        ns = (now() - start) * 1e9 / 3000;
        if (sample) fprintf(stderr, "json,%.3f\n", ns);

        start = now();
        for (int i = 0; i < 300; ++i) {
            response r = {0};
            for (int part = 0; part < 64; ++part)
                assert(receive_body(chunk, 1, sizeof(chunk), &r) == sizeof(chunk));
            assert(r.size == 1048576); checksum += r.size; free(r.data);
        }
        ns = (now() - start) * 1e9 / 300;
        if (sample) fprintf(stderr, "buffer,%.3f\n", ns);

        start = now();
        for (int i = 0; i < 100; ++i) print_json_string(text);
        fflush(stdout);
        ns = (now() - start) * 1e9 / 100;
        if (sample) fprintf(stderr, "cli_json,%.3f\n", ns);
    }
    assert(checksum > 0);
    cpapago_cleanup();
    return 0;
}
'''

HTTP_SOURCE = r'''
#define _POSIX_C_SOURCE 200809L
#include "cpapago.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static double now(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}
int main(int argc, char **argv) {
    if (argc != 2 || cpapago_init() != CPAPAGO_OK) return 1;
    cpapago_options options = {.base_url = argv[1], .parallelism = 4};
    cpapago_parameter parameters[32];
    cpapago_error error;
    for (int i = 0; i < 32; ++i) parameters[i] = (cpapago_parameter){NULL, "ko", "Hello"};
    for (int sample = 0; sample < 6; ++sample) {
        char **outputs = NULL;
        double start = now();
        if (cpapago_translate_many(parameters, 32, &options, &outputs, &error) != CPAPAGO_OK) {
            fprintf(stderr, "%s\n", error.message); return 1;
        }
        double ms = (now() - start) * 1e3;
        for (int i = 0; i < 32; ++i) if (strcmp(outputs[i], "Hello")) return 1;
        cpapago_free_many(outputs, 32);
        if (sample) printf("%.3f\n", ms);
    }
    cpapago_cleanup(); return 0;
}
'''


def compile_program(source, destination, code, extra, optimization):
    destination.mkdir(parents=True, exist_ok=True)
    program = destination / "bench.c"
    program.write_text(code, encoding="utf-8")
    cflags = subprocess.check_output(["curl-config", "--cflags"], text=True).split()
    libs = subprocess.check_output(["curl-config", "--libs"], text=True).split()
    subprocess.run(["cc", optimization, "-std=c11", "-Wall", "-Wextra", "-Werror", "-pthread",
                    "-I" + str(source / "include"), "-I" + str(source / "src"), *cflags,
                    str(program), *[str(source / path) for path in extra], *libs,
                    "-o", str(destination / "bench")], check=True)
    return destination / "bench"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", required=True, type=Path)
    parser.add_argument("--optimization", choices=["O2", "O3"], default="O2")
    args = parser.parse_args()
    baseline = args.baseline.resolve()
    document = {f"metadata_{i}": i for i in range(40)}
    document["dict"] = {"items": [{"entry": "example", "meaning": "단어 설명" * 8} for _ in range(100)]}
    document["translatedText"] = "안녕하세요 😀"
    json_literal = json.dumps(json.dumps(document, ensure_ascii=False), ensure_ascii=False)
    measurements = {}
    with tempfile.TemporaryDirectory(prefix="cpapago-bench-") as directory:
        temp = Path(directory)
        for name, source in [("before", baseline), ("after", ROOT)]:
            code = CPU_SOURCE.replace("@SOURCE@", str(source)).replace("@JSON@", json_literal)
            program = compile_program(source, temp / (name + "-cpu"), code, ["src/json.c"], "-" + args.optimization)
            result = subprocess.run([str(program)], text=True, capture_output=True, check=True)
            values = {}
            for line in result.stderr.splitlines():
                metric, value = line.split(",")
                values.setdefault(metric, []).append(float(value))
            measurements[name] = {metric: statistics.median(samples) for metric, samples in values.items()}

        connections = set()
        lock = threading.Lock()

        class Handler(BaseHTTPRequestHandler):
            protocol_version = "HTTP/1.1"

            def setup(self):
                super().setup()
                self.connection.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

            def log_message(self, *args):
                pass

            def do_POST(self):
                form = parse_qs(self.rfile.read(int(self.headers["Content-Length"])).decode())
                with lock:
                    connections.add(self.client_address)
                data = {"langCode": "en"} if self.path.endswith("/dect") else {"translatedText": form["text"][0]}
                body = json.dumps(data).encode()
                self.send_response(200)
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

        server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            base = f"http://127.0.0.1:{server.server_port}"
            for name, source in [("before", baseline), ("after", ROOT)]:
                program = compile_program(source, temp / (name + "-http"), HTTP_SOURCE,
                                          ["src/cpapago.c", "src/json.c"], "-" + args.optimization)
                connections.clear()
                result = subprocess.check_output([str(program), base], text=True)
                measurements[name]["http_batch_ms"] = statistics.median(float(line) for line in result.splitlines())
                measurements[name]["connections_per_batch"] = len(connections) / 6
        finally:
            server.shutdown(); server.server_close(); thread.join()
    print("Same compiler flags: -" + args.optimization + "; median of 5 warmed samples")
    for metric in measurements["before"]:
        before = measurements["before"][metric]
        after = measurements["after"][metric]
        print(f"{metric:24} {before:12.3f} -> {after:12.3f} ({(1 - after / before) * 100:+.1f}% reduction)")


if __name__ == "__main__":
    main()
