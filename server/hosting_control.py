"""Private loopback dashboard controller. Never attach this service to a tunnel."""
import argparse
from datetime import datetime, timezone
import hashlib
import hmac
import json
from http.cookies import SimpleCookie
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import os
from pathlib import Path
import re
import secrets
import subprocess
import threading
import time
from urllib.parse import parse_qs

ACTIONS = {"refresh", "start", "stop", "restart", "auto-enable", "auto-disable", "export"}
MESSAGES = {
    "refresh": "상태를 새로 확인했습니다.",
    "start": "서버와 자동 복구 감시를 시작했습니다.",
    "stop": "서버와 감시를 중지했습니다. 다음 Windows 로그인 시 자동 시작 설정은 유지됩니다.",
    "restart": "서버와 자동 복구 감시를 다시 시작했습니다.",
    "auto-enable": "Windows 로그인 시 자동 시작을 켰습니다.",
    "auto-disable": "다음 Windows 로그인 시 자동 시작을 껐습니다. 현재 실행 상태는 유지됩니다.",
    "export": "인증된 테스터 목록을 이 PC의 비공개 CSV로 내보냈습니다.",
}


def safe_file(path, maximum):
    for ancestor in (path, *path.parents):
        if ancestor.exists() and (ancestor.is_symlink() or getattr(ancestor.lstat(), "st_file_attributes", 0) & 0x400):
            raise ValueError("Linked management paths are not allowed")
    if path.stat().st_size > maximum:
        raise ValueError("Management file exceeds its size limit")
    return path.read_bytes()


class HostingController:
    def __init__(self, root, pwsh, game_port):
        self.root = root
        self.project = Path(__file__).resolve().parents[1]
        self.pwsh = pwsh
        self.game_port = game_port
        self.last_refresh = 0.0
        self.refresh_lock = threading.Lock()

    def run(self, name, arguments=()):
        result = subprocess.run(
            [str(self.pwsh), "-NoLogo", "-NoProfile", "-File", str(self.project / "tools" / name),
             "-PrivateRoot", str(self.root), *arguments],
            cwd=self.project, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            timeout=120, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
        )
        if result.returncode:
            raise RuntimeError("Local management command failed")

    def refresh(self):
        with self.refresh_lock:
            if time.monotonic() - self.last_refresh < 3:
                return
            self.run("Update-HostingDashboard.ps1")
            self.last_refresh = time.monotonic()

    def operate(self, action):
        launch_arguments = []
        if action in ("start", "restart", "auto-enable"):
            launch = json.loads(safe_file(self.root / "hosting-launch.json", 16384))
            if not isinstance(launch, dict) or Path(launch.get("PrivateRoot", "")) != self.root or launch.get("Port") != self.game_port:
                raise ValueError("Hosting launch parameters do not match this controller")
            if set(launch) - {"PrivateRoot", "PythonPath", "CloudflaredPath", "TokenFile", "TunnelId", "Port"}:
                raise ValueError("Unexpected hosting launch parameter")
            for name, value in launch.items():
                if name == "PrivateRoot":
                    continue
                if not isinstance(value, (str, int)) or isinstance(value, bool) or any(c in str(value) for c in "\r\n\x00"):
                    raise ValueError("Invalid hosting launch parameter")
                launch_arguments.extend(("-" + name, str(value)))
        if action in ("stop", "restart"):
            self.run("Stop-PlaytestHosting.ps1")
        if action in ("start", "restart"):
            # The controller is intentionally independent of the managed origin/watchdog.
            self.run("Start-PlaytestHosting.ps1", (*launch_arguments, "-NoDashboardControl"))
        if action in ("auto-enable", "auto-disable"):
            self.run("Enable-HostingAutostart.ps1", ("-Action", "Enable" if action == "auto-enable" else "Disable", *launch_arguments))
        if action == "export":
            self.run("Update-HostingDashboard.ps1", ("-ExportCsv",))
        self.last_refresh = 0


def create_server(root, port, token, controller):
    if not re.fullmatch(r"[0-9a-f]{64}", token):
        raise ValueError("Invalid local control capability")
    session = hmac.new(bytes.fromhex(token), b"dashboard-session", hashlib.sha256).hexdigest()
    operation_lock = threading.Lock()
    open_lock = threading.Lock()

    class Handler(BaseHTTPRequestHandler):
        server_version = "TeamHJDLocalControl"

        def setup(self):
            super().setup()
            self.connection.settimeout(8)

        def log_message(self, *_args):
            pass  # Never log request paths, cookies or form capabilities.

        def host_valid(self):
            return (len(self.headers.get_all("Host", [])) == 1 and
                    self.headers["Host"] == f"127.0.0.1:{self.server.server_port}")

        def authenticated(self):
            try:
                cookie = SimpleCookie(self.headers.get("Cookie", ""))
                value = cookie.get("hjd_local_admin")
                return value is not None and hmac.compare_digest(value.value, session)
            except (ValueError, TypeError):
                return False

        def respond(self, status, body=b"", content_type="text/plain; charset=utf-8", login=False, location=None):
            self.send_response(status)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header("X-Frame-Options", "DENY")
            self.send_header("Referrer-Policy", "no-referrer")
            self.send_header("Content-Security-Policy", "frame-ancestors 'none'")
            if location:
                self.send_header("Location", location)
            if login:
                self.send_header("Set-Cookie", f"hjd_local_admin={session}; Path=/; HttpOnly; SameSite=Strict")
            self.end_headers()
            self.wfile.write(body)

        def page(self, message="", login=False, status=200):
            try:
                controller.refresh()
                page = safe_file(root / "dashboard/index.html", 16 * 1024 * 1024)
                page = page.replace(b'http-equiv="refresh" content="15"', b'http-equiv="refresh" content="15;url=/"')
                # Fixed messages only; never insert an exception or any request text.
                if message:
                    page = page.replace(b"<!--CONTROL-FEEDBACK-->",
                                        ('<p class="control-feedback" role="status">' + message + '</p>').encode("utf-8"))
                self.respond(status, page, "text/html; charset=utf-8", login)
            except (OSError, ValueError, RuntimeError, subprocess.SubprocessError):
                self.respond(503, "관리 화면 갱신에 실패했습니다. 로컬 운영 도구로 상태를 확인하세요.".encode("utf-8"))

        def do_GET(self):
            if not self.host_valid():
                self.respond(403)
            elif self.path == "/health":
                self.respond(204)
            elif re.fullmatch(r"/open/[0-9a-f]{64}", self.path):
                # One-use, 60-second local browser handoff. The long-lived capability never enters a URL.
                try:
                    with open_lock:
                        path = root / "dashboard-open.json"
                        handoff = json.loads(safe_file(path, 1024))
                        now = time.time()
                        digest = hashlib.sha256(self.path.rsplit("/", 1)[1].encode("ascii")).hexdigest()
                        if (not isinstance(handoff, dict) or handoff.get("used") is not False or
                                type(handoff.get("expiresAt")) is not int or not now < handoff["expiresAt"] <= now + 90 or
                                not isinstance(handoff.get("sha256"), str) or not hmac.compare_digest(handoff["sha256"], digest)):
                            raise ValueError()
                        handoff["used"] = True
                        temporary = path.with_suffix("." + secrets.token_hex(8) + ".tmp")
                        try:
                            temporary.write_text(json.dumps(handoff), encoding="utf-8")
                            os.replace(temporary, path)
                        finally:
                            temporary.unlink(missing_ok=True)
                    self.respond(303, login=True, location="/")
                except (OSError, ValueError, TypeError):
                    self.respond(401)
            elif not self.authenticated():
                self.respond(401, "이 PC의 서버 관리 바로가기 또는 비공개 dashboard/index.html에서 관리 화면을 여세요.".encode("utf-8"))
            elif self.path == "/":
                self.page()
            elif self.path == "/verified-testers.csv":
                try:
                    data = safe_file(root / "dashboard/verified-testers.csv", 8 * 1024 * 1024)
                    self.respond(200, data, "text/csv; charset=utf-8")
                except (OSError, ValueError):
                    self.respond(404)
            else:
                self.respond(404)

        def do_POST(self):
            origin = f"http://127.0.0.1:{self.server.server_port}"
            # file: form navigation is the bootstrap. It still requires the private random token.
            if not self.host_valid() or self.headers.get("Origin") not in (origin, "null"):
                self.respond(403)
                return
            if self.path != "/control" or self.headers.get("Transfer-Encoding") or len(self.headers.get_all("Content-Length", [])) != 1:
                self.respond(400)
                return
            try:
                length = int(self.headers["Content-Length"])
                if not 0 < length <= 4096 or self.headers.get_content_type() != "application/x-www-form-urlencoded":
                    raise ValueError()
                values = parse_qs(self.rfile.read(length).decode("ascii"), strict_parsing=True, max_num_fields=2)
                if set(values) != {"token", "action"} or any(len(value) != 1 for value in values.values()):
                    raise ValueError()
                action = values["action"][0]
                if not hmac.compare_digest(values["token"][0], token) or action not in ACTIONS:
                    raise ValueError()
            except (ValueError, TypeError, UnicodeError, OSError):
                self.respond(403)
                return
            if not operation_lock.acquire(blocking=False):
                self.respond(409, "다른 관리 작업을 처리 중입니다. 잠시 후 다시 시도하세요.".encode("utf-8"))
                return
            try:
                controller.operate(action)
                self.page(MESSAGES[action], login=True)
            except (OSError, ValueError, RuntimeError, subprocess.SubprocessError):
                self.page("명령을 완료하지 못했습니다. 구성 요소 상태와 비공개 운영 로그를 확인하세요.", login=True, status=503)
            finally:
                operation_lock.release()

    class Server(ThreadingHTTPServer):
        daemon_threads = True
        request_queue_size = 8

    return Server(("127.0.0.1", port), Handler)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--port", type=int, default=8844)
    parser.add_argument("--pwsh", type=Path, required=True)
    parser.add_argument("--game-port", type=int, default=8443)
    args = parser.parse_args()
    token = safe_file(args.root / "dashboard-control-token.txt", 128).decode("ascii").strip()
    controller = HostingController(args.root, args.pwsh, args.game_port)
    with create_server(args.root, args.port, token, controller) as server:
        print("Private loopback dashboard control ready", flush=True)
        server.serve_forever(poll_interval=.5)


if __name__ == "__main__":
    main()
