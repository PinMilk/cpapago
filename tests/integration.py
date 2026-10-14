"""네트워크 서비스에 의존하지 않는 로컬 HTTP 통합 테스트입니다."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import subprocess
import socket
import threading
import time
from urllib.parse import parse_qs

ROOT = Path(__file__).resolve().parents[1]
CLI = Path(os.environ.get("CPAPAGO_CLI", str(ROOT / ".build/cpapago"))).resolve()
REQUESTS = []
LOCK = threading.Lock()
STATE = {"active": 0, "maximum": 0}
CONNECTIONS = set()


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def setup(self):
        super().setup()
        # 작은 응답의 Nagle 지연이 연결 재사용 테스트에 영향을 주지 않도록 합니다.
        self.connection.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

    def log_message(self, *args):
        pass

    def do_POST(self):
        form = parse_qs(self.rfile.read(int(self.headers["Content-Length"])).decode(), keep_blank_values=True)
        assert self.headers["Content-Type"] == "application/x-www-form-urlencoded; charset=UTF-8"
        assert "Authorization" not in self.headers
        with LOCK:
            REQUESTS.append((self.path, form))
            CONNECTIONS.add(self.client_address)
        if self.path == "/api/langs/dect":
            text = form["query"][0]
            response = {"langCode": "zh-CN" if "你好" in text else "en"}
        elif self.path == "/api/text/translation":
            assert form["dict"] == ["true"] and form["dictDisplay"] == ["30"]
            assert form["useGlossary"] == ["false"]
            assert form["honorific"][0] in ("true", "false")
            text = form["text"][0]
            if text == "__http_error__":
                self.reply(403, b'{"error":"blocked"}'); return
            if text == "__bad_json__":
                self.reply(200, b'<html>not JSON</html>'); return
            if text == "__wrong_type__":
                self.reply(200, b'{"translatedText":123}'); return
            if text == "__trailing__":
                self.reply(200, b'{"translatedText":"ok"} garbage'); return
            if text == "__raw_nul__":
                self.reply(200, b'{"translatedText":"ok"}\0garbage'); return
            if text == "__large__":
                self.reply(200, b'{"translatedText":"' + b'x' * (8 * 1024 * 1024) + b'"}'); return
            if text == "__slow__":
                time.sleep(0.2)
            if text.startswith("batch-"):
                with LOCK:
                    STATE["active"] += 1
                    STATE["maximum"] = max(STATE["maximum"], STATE["active"])
                time.sleep(0.08 if text == "batch-0" else 0.02)
                with LOCK:
                    STATE["active"] -= 1
            response = {"translatedText": text, "srcLangType": form["source"][0],
                        "tarLangType": form["target"][0], "dict": {"nested": [True, None, 1.5]}}
        else:
            self.reply(404, b'{}'); return
        # ensure_ascii=True는 실제 Unicode escape 복호화 경로를 테스트합니다.
        self.reply(200, json.dumps(response, ensure_ascii=True).encode())

    def reply(self, status, body):
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass


def main():
    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    base = f"http://127.0.0.1:{server.server_port}"

    def run(*args, success=True):
        result = subprocess.run([str(CLI), *args, "--base-url", base], text=True, capture_output=True)
        assert (result.returncode == 0) == success, (args, result.stdout, result.stderr)
        return result

    try:
        text = 'Hello & text=wrong + 100% #테스트\n안녕하세요 😀 "quote" \\ slash'
        assert run("detect", text).stdout.strip() == "en"
        assert run("detect", "你好").stdout.strip() == "zh-cn"
        before_connections = set(CONNECTIONS)
        assert run("translate", "ko", text).stdout.rstrip("\n") == text
        assert len(CONNECTIONS - before_connections) == 1, "Detection and translation must reuse a connection"
        assert run("translate", "ko", "", "--source", "en").stdout == "\n"
        ascii_text = ''.join(chr(value) for value in range(1, 128))
        # 폼 바이트 인코딩과 CLI JSON escape가 모든 비널 ASCII를 보존하는지 검사합니다.
        ascii_batch = json.loads(run("batch", "ko", ascii_text, "--source", "en").stdout)
        assert ascii_batch == [ascii_text]
        verbose = json.loads(run("translate", "ko", "你好", "--verbose", "--honorific").stdout)
        assert verbose["srcLangType"] == "zh-CN" and verbose["translatedText"] == "你好"
        translated = json.loads(run("translate", "zh-tw", text, "--source", "zh-cn", "--verbose").stdout)
        assert translated["srcLangType"] == "zh-CN" and translated["tarLangType"] == "zh-TW"
        batch = [f"batch-{index}" for index in range(8)]
        before_connections = set(CONNECTIONS)
        assert json.loads(run("batch", "ko", *batch, "--source", "en", "--parallel", "4").stdout) == batch
        assert len(CONNECTIONS - before_connections) <= 4, "Each worker must reuse its connection"
        assert 1 < STATE["maximum"] <= 4, STATE
        verbose_batch = json.loads(run("batch", "ko", "first", "second", "--source", "en", "--verbose").stdout)
        assert [item["translatedText"] for item in verbose_batch] == ["first", "second"]
        for bad in ["__http_error__", "__bad_json__", "__wrong_type__", "__trailing__", "__raw_nul__", "__large__"]:
            assert run("translate", "ko", bad, "--source", "en", success=False).stderr
        failure = run("batch", "ko", "good", "__http_error__", "good2", "--source", "en", success=False)
        assert failure.stdout == "" and "HTTP=403" in failure.stderr
        assert run("translate", "ko", "__slow__", "--source", "en", "--timeout", "20", success=False).stderr
        assert any(form.get("honorific") == ["true"] for _, form in REQUESTS)
        print("Offline HTTP integration tests passed (encoding, auto-detection, JSON, concurrency, errors, timeout)")
    finally:
        server.shutdown()
        server.server_close()
        thread.join()


if __name__ == "__main__":
    main()
