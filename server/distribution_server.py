"""Read-only TeamHJD distribution server. No execution or upload endpoint."""
from __future__ import annotations
import argparse
import base64
import hashlib
import hmac
from html import escape
import json
import os
from datetime import datetime, timedelta, timezone
from pathlib import Path
import re
import ssl
import stat
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit
if __package__:
    from .cloudflare_auth import AuditUnavailable, CloudflareAccess
else:
    from cloudflare_auth import AuditUnavailable, CloudflareAccess


TESTER_TOKEN_PATTERN = re.compile(rb"hjd1\.([0-9a-f]{16})\.[A-Za-z0-9_-]{43}")
TESTER_REGISTRY_MAX_BYTES = 2 * 1024 * 1024
TESTER_REGISTRY_MAX_COUNT = 2000

PORTAL_STYLE = """body{margin:0;background:#171b1e;color:#e8ece9;font:16px system-ui,sans-serif;line-height:1.6}main{max-width:880px;margin:auto;padding:36px 24px}header{border-bottom:1px solid #354047;padding-bottom:24px}h1{font-size:28px;margin:6px 0}h2{font-size:20px}p{color:#b8c2c8}.brand{color:#eac966;font-size:13px;letter-spacing:2px}.game{background:#22292e;border:1px solid #354047;border-radius:14px;padding:24px;margin:24px 0}.links{display:flex;flex-wrap:wrap;gap:12px}a{display:inline-block;background:#eac966;color:#171b1e;text-decoration:none;border-radius:7px;padding:12px 18px;font-weight:650}small{display:block;color:#aab6bd;margin-top:8px}li{margin:10px 0}code{overflow-wrap:anywhere}footer{border-top:1px solid #354047;margin-top:32px;padding-top:16px;color:#aab6bd;font-size:13px}"""
PORTAL_CSP = ("default-src 'none'; style-src 'sha256-" + base64.b64encode(hashlib.sha256(PORTAL_STYLE.encode()).digest()).decode()
              + "'; base-uri 'none'; form-action 'none'; frame-ancestors 'none'")


class DownloadPortal:
    """Authenticated, script-free view of cryptographically verified local releases."""
    labels = {"windows-x64": "Windows 64비트", "macos-universal": "macOS · Apple Silicon / Intel",
              "macos-arm64": "macOS · Apple Silicon", "macos-x64": "macOS · Intel"}

    def __init__(self, root: Path):
        self.root = root
        self.hash_cache = {}
        self.cache_lock = threading.Lock()

    def plain_file(self, relative: str) -> Path:
        target = self.root / relative
        current = target
        while current != self.root.parent:
            if current.is_symlink() or (hasattr(current, "is_junction") and current.is_junction()):
                raise ValueError("Linked portal file")
            current = current.parent
        if not target.is_file() or not target.resolve().is_relative_to(self.root):
            raise ValueError("Missing portal file")
        return target

    def read_bounded(self, relative: str, maximum: int) -> bytes:
        with self.plain_file(relative).open("rb") as source:
            result = source.read(maximum + 1)
        if len(result) > maximum:
            raise ValueError("Oversized portal metadata")
        return result

    def verify_package(self, metadata: dict) -> str:
        checksum = metadata.get("sha256")
        size = metadata.get("bytes")
        if (not isinstance(checksum, str) or not re.fullmatch(r"[0-9a-f]{64}", checksum)
                or type(size) is not int or not 0 < size <= 10 * 1024**3
                or metadata.get("url") != f"packages/{checksum}.zip"):
            raise ValueError("Invalid portal package metadata")
        relative = f"packages/{checksum}.zip"
        target = self.plain_file(relative)
        with target.open("rb") as source:
            info = os.fstat(source.fileno())
            if not stat.S_ISREG(info.st_mode) or info.st_size != size:
                raise ValueError("Portal package size mismatch")
            identity = (info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns, info.st_ctime_ns)
            with self.cache_lock:
                cached = self.hash_cache.get(checksum) == identity
            if not cached:
                actual = hashlib.file_digest(source, "sha256").hexdigest()
                after = os.fstat(source.fileno())
                if (actual != checksum or identity != (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns, after.st_ctime_ns)):
                    raise ValueError("Portal package digest mismatch")
                with self.cache_lock:
                    if len(self.hash_cache) >= 16:
                        self.hash_cache.clear()
                    self.hash_cache[checksum] = identity
        return "/" + relative

    def render(self) -> bytes:
        # Legacy password/catalog routes retain their standard-library-only
        # behavior. Only this optional dynamic page needs signature verification.
        from cryptography.hazmat.primitives import hashes
        from cryptography.hazmat.primitives.asymmetric import ec, utils
        invite = json.loads(self.read_bounded("invite.json", 256 * 1024).decode("utf-8-sig"))
        key = invite.get("publisherKey")
        if not isinstance(key, str) or not key.startswith("p256:"):
            raise ValueError("Missing portal public key")
        point = base64.b64decode(key[5:], validate=True)
        if len(point) != 64:
            raise ValueError("Invalid portal public key")
        public = ec.EllipticCurvePublicNumbers(int.from_bytes(point[:32]), int.from_bytes(point[32:]), ec.SECP256R1()).public_key()
        envelope = json.loads(self.read_bounded("catalog.json", 32 * 1024 * 1024).decode("utf-8-sig"))
        payload = base64.b64decode(envelope["payload"], validate=True)
        signature = base64.b64decode(envelope["signature"], validate=True)
        if len(signature) != 64:
            raise ValueError("Invalid portal signature")
        public.verify(utils.encode_dss_signature(int.from_bytes(signature[:32]), int.from_bytes(signature[32:])), payload, ec.ECDSA(hashes.SHA256()))
        catalog = json.loads(payload)
        issued = datetime.fromisoformat(catalog["issuedAt"].replace("Z", "+00:00"))
        expires = datetime.fromisoformat(catalog["expiresAt"].replace("Z", "+00:00"))
        now = datetime.now(timezone.utc)
        if (type(catalog.get("schema")) is not int or catalog["schema"] != 1
                or type(catalog.get("sequence")) is not int or not 0 <= catalog["sequence"] <= 2**53 - 1
                or issued.tzinfo is None or expires.tzinfo is None
                or issued.utcoffset() != timedelta(0) or expires.utcoffset() != timedelta(0)
                or not issued < expires or expires - issued > timedelta(days=8)
                or issued > now + timedelta(minutes=5) or expires <= now):
            raise ValueError("Invalid portal catalog validity")
        games = catalog.get("games")
        if not isinstance(games, list) or not 0 < len(games) <= 100:
            raise ValueError("Invalid portal game count")
        cards = []
        seen = set()
        for game in games:
            game_id, name = game.get("id"), game.get("name")
            if (not isinstance(game_id, str) or not re.fullmatch(r"[a-z0-9][a-z0-9-]{0,63}", game_id)
                    or game_id == "launcher" or game_id in seen or not isinstance(name, str) or not 0 < len(name) <= 120):
                raise ValueError("Invalid portal game")
            seen.add(game_id)
            variants = game.get("platforms", {})
            if not isinstance(variants, dict) or len(variants) > 3 or any(platform not in self.labels or platform == "windows-x64" for platform in variants):
                raise ValueError("Invalid portal variants")
            candidates = [(game.get("platform", "windows-x64"), game), *variants.items()]
            links = []
            platforms = set()
            for platform, metadata in candidates:
                if (platform not in self.labels or platform in platforms or not isinstance(metadata, dict)
                        or metadata.get("platform", platform) != platform or metadata.get("id") != game_id
                        or metadata.get("name") != name or (metadata is not game and "platforms" in metadata)):
                    raise ValueError("Invalid portal platform")
                platforms.add(platform)
                version = metadata.get("version")
                if not isinstance(version, str) or not re.fullmatch(r"[0-9][A-Za-z0-9._-]{0,79}", version) or version.endswith("."):
                    raise ValueError("Invalid portal version")
                url = self.verify_package(metadata)
                filename = f"{game_id}-{version}-{platform}.zip"
                links.append(f'<div><a href="{url}" download="{filename}">{escape(self.labels[platform])} 다운로드</a><small>{escape(version)} · {metadata["bytes"] / 1024**2:.1f} MB</small></div>')
            cards.append(f'<section class="game"><h2>{escape(name)}</h2><div class="links">{"".join(links)}</div></section>')
        page = ('<!doctype html><html lang="ko"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">'
                f'<title>TeamHJD · 게임 다운로드</title><style>{PORTAL_STYLE}</style></head><body><main><header><div class="brand">TEAMHJD PLAYTEST</div>'
                '<h1>게임 다운로드</h1><p>이메일 인증을 마친 테스터를 위한 배포입니다. 내 컴퓨터에 맞는 ZIP을 선택하세요.</p></header>'
                + ''.join(cards) + '<section><h2>실행 방법</h2><ol><li>ZIP을 내려받아 압축을 완전히 풉니다. Windows에서는 게임 실행 파일을 실행하세요.</li>'
                '<li>맥에서는 압축을 푼 <strong>HoseForHire.app</strong>을 실행하세요. 실행이 차단되면 공식 배포 파일을 확인한 뒤 시스템 설정 → 개인정보 보호 및 보안에서 이 앱의 <strong>그래도 열기</strong>를 선택하세요. 전체 보안 설정은 끄지 마세요.</li>'
                '<li>다음 빌드는 이 페이지에서 다시 받거나 런처의 업데이트 기능으로 적용할 수 있습니다. 함께 플레이할 팀원은 같은 BUILD 번호를 사용하세요.</li></ol></section>'
                '<footer>서명된 배포 목록과 ZIP의 SHA-256·용량을 확인했습니다. 이 페이지에는 인증 토큰이나 접속자 정보가 포함되지 않습니다.</footer></main></body></html>')
        return page.encode("utf-8")


def tester_token_authorized(root: Path, token: bytes) -> bool:
    """Read a bounded current registry so atomic operator revocations apply immediately."""
    match = TESTER_TOKEN_PATTERN.fullmatch(token)
    if not match:
        return False
    registry_path = root / "testers.json"
    try:
        if registry_path.is_symlink() or (hasattr(registry_path, "is_junction") and registry_path.is_junction()):
            return False
        with registry_path.open("rb") as registry_file:
            raw = registry_file.read(TESTER_REGISTRY_MAX_BYTES + 1)
        if len(raw) > TESTER_REGISTRY_MAX_BYTES:
            return False
        registry = json.loads(raw.decode("utf-8-sig"))
        if not isinstance(registry, dict) or type(registry.get("schema")) is not int or registry["schema"] != 1:
            return False
        testers = registry.get("testers")
        if not isinstance(testers, list) or len(testers) > TESTER_REGISTRY_MAX_COUNT:
            return False
        requested_id = match[1].decode("ascii")
        seen = set()
        selected = None
        for entry in testers:
            if not isinstance(entry, dict):
                return False
            token_id = entry.get("tokenId")
            if not isinstance(token_id, str) or not re.fullmatch(r"[0-9a-f]{16}", token_id) or token_id in seen:
                return False
            seen.add(token_id)
            digest = entry.get("tokenSha256")
            expiry = entry.get("expiresAt")
            if (not isinstance(digest, str) or not re.fullmatch(r"[0-9a-f]{64}", digest)
                    or not isinstance(expiry, str) or len(expiry) > 64 or type(entry.get("revoked")) is not bool):
                return False
            expires_at = datetime.fromisoformat(expiry.replace("Z", "+00:00"))
            if expires_at.tzinfo is None or expires_at.utcoffset() != timedelta(0):
                return False
            if token_id == requested_id:
                selected = (bytes.fromhex(digest), expires_at, entry["revoked"])
        if selected is None:
            return False
        expected_digest, expires_at, revoked = selected
        valid_digest = hmac.compare_digest(hashlib.sha256(token).digest(), expected_digest)
        return valid_digest and not revoked and expires_at > datetime.now(timezone.utc)
    except (OSError, ValueError, TypeError, KeyError, UnicodeError, RecursionError, OverflowError):
        # Fail closed without writing credentials, registry contents or client identity to logs.
        return False


class DistributionServer(ThreadingHTTPServer):
    daemon_threads = True
    request_queue_size = 32

    def __init__(self, address, handler, context=None):
        self.tls_context = context
        self.connection_slots = threading.BoundedSemaphore(40)
        super().__init__(address, handler)

    def process_request(self, request, client_address):
        if not self.connection_slots.acquire(blocking=False):
            self.shutdown_request(request)
            return
        try:
            super().process_request(request, client_address)
        except BaseException:
            self.connection_slots.release()
            raise

    def process_request_thread(self, request, client_address):
        try:
            request.settimeout(15)
            if self.tls_context:
                request = self.tls_context.wrap_socket(request, server_side=True)
            super().process_request_thread(request, client_address)
        except (ssl.SSLError, OSError):
            self.shutdown_request(request)
        finally:
            self.connection_slots.release()

    def handle_error(self, request, client_address):
        # Avoid exposing client identity or authorization data in operational logs.
        pass


def serve(args):
    root = Path(args.root).resolve(strict=True)
    if root.is_symlink():
        raise ValueError("Distribution root must be a regular directory")
    auth = json.loads((root / "auth.json").read_text("utf-8-sig"))
    salt = base64.b64decode(auth["salt"], validate=True)
    expected = base64.b64decode(auth["hash"], validate=True)
    iterations = int(auth["iterations"])
    if len(salt) < 16 or len(expected) != 32 or not 100_000 <= iterations <= 2_000_000:
        raise ValueError("Invalid password verifier")
    attempts: dict[str, list[float]] = {}
    attempts_lock = threading.Lock()
    slots = threading.BoundedSemaphore(12)
    cloudflare = CloudflareAccess(root)
    portal = DownloadPortal(root)

    class Handler(BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.1"
        server_version = "TeamHJDDistribution/1"
        sys_version = ""

        def log_message(self, fmt, *values):
            # Never log peer addresses, passwords, URLs or private filenames.
            pass

        def setup(self):
            super().setup()
            self.connection.settimeout(30)

        def reject(self, code, retry=False):
            self.send_response(code)
            self.send_header("Content-Length", "0")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Connection", "close")
            if retry:
                self.send_header("Retry-After", "60")
            self.end_headers()
            self.close_connection = True

        def authorized(self):
            peer = self.client_address[0]
            now = time.monotonic()
            with attempts_lock:
                # Bounded cache; no persistent identity/address logging.
                if len(attempts) > 4096:
                    attempts.clear()
                recent = [v for v in attempts.get(peer, []) if now - v < 60]
                if len(recent) >= 10:
                    self.reject(429, True)
                    return False
                attempts[peer] = recent
            audit_unavailable = False
            assertions = self.headers.get_all("Cf-Access-Jwt-Assertion", [])
            if len(assertions) == 1:
                identity = cloudflare.verify(assertions[0])
                if identity is not None:
                    try:
                        cloudflare.record_access(identity)
                        return True
                    except AuditUnavailable:
                        audit_unavailable = True
            header = self.headers.get("Authorization", "")
            password = b""
            try:
                if not header.startswith("Bearer ") or len(header) > 2048:
                    raise ValueError("missing")
                password = base64.b64decode(header[7:], validate=True)
                if not 1 <= len(password) <= 1024:
                    raise ValueError("size")
            except (ValueError, TypeError):
                if not audit_unavailable:
                    with attempts_lock:
                        attempts.setdefault(peer, []).append(now)
                self.reject(503 if audit_unavailable else 401, audit_unavailable)
                return False
            if password.startswith(b"hjd1."):
                valid = tester_token_authorized(root, password)
            else:
                valid = hmac.compare_digest(hashlib.pbkdf2_hmac("sha256", password, salt, iterations), expected)
            if not valid:
                if not audit_unavailable:
                    with attempts_lock:
                        attempts.setdefault(peer, []).append(now)
                self.reject(503 if audit_unavailable else 401, audit_unavailable)
                return False
            return True

        def do_GET(self):
            if not slots.acquire(blocking=False):
                self.reject(503, True)
                return
            try:
                self.send_file()
            except (BrokenPipeError, ConnectionResetError, TimeoutError):
                pass
            finally:
                slots.release()

        def do_HEAD(self):
            self.do_GET()

        def send_file(self):
            if not self.authorized():
                return
            request = urlsplit(self.path)
            if request.query or request.fragment:
                self.reject(404)
                return
            path = request.path
            if path in ("/", "/downloads"):
                if self.headers.get("Range"):
                    self.reject(416)
                    return
                try:
                    body = portal.render()
                except Exception:
                    # Missing dependencies, invalid signatures or local files
                    # fail closed. No metadata, claims or exception text leaks.
                    self.reject(503)
                    return
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Content-Length", str(len(body)))
                self.send_header("Cache-Control", "no-store")
                self.send_header("Content-Security-Policy", PORTAL_CSP)
                self.send_header("X-Content-Type-Options", "nosniff")
                self.send_header("Referrer-Policy", "no-referrer")
                self.send_header("X-Frame-Options", "DENY")
                self.end_headers()
                if self.command != "HEAD":
                    self.wfile.write(body)
                return
            if path == "/catalog.json":
                target = root / "catalog.json"
            elif re.fullmatch(r"/packages/[0-9a-f]{64}\.zip", path):
                target = root / path[1:]
            else:
                self.reject(404)
                return
            # Check every ancestor for symlinks/reparse points, including junctions.
            current = target
            while current != root.parent:
                if current.is_symlink() or (hasattr(current, "is_junction") and current.is_junction()):
                    self.reject(404)
                    return
                current = current.parent
            if not target.is_file() or not target.resolve().is_relative_to(root):
                self.reject(404)
                return
            with target.open("rb") as stream:
                size = target.stat().st_size
                start = 0
                partial = False
                requested_range = self.headers.get("Range")
                if requested_range:
                    match = re.fullmatch(r"bytes=(\d+)-", requested_range)
                    if path == "/catalog.json" or not match or int(match[1]) >= size:
                        self.reject(416)
                        return
                    start = int(match[1])
                    partial = True
                self.send_response(206 if partial else 200)
                self.send_header("Content-Type", "application/json" if path == "/catalog.json" else "application/zip")
                self.send_header("Content-Length", str(size - start))
                self.send_header("Cache-Control", "no-store")
                self.send_header("X-Content-Type-Options", "nosniff")
                self.send_header("Accept-Ranges", "bytes")
                if partial:
                    self.send_header("Content-Range", f"bytes {start}-{size-1}/{size}")
                self.end_headers()
                if self.command == "HEAD":
                    return
                stream.seek(start)
                while chunk := stream.read(256 * 1024):
                    self.wfile.write(chunk)

    if not args.cert and (not args.test_http or args.bind not in ("127.0.0.1", "::1", "localhost")):
        raise ValueError("TLS certificate and key required; test HTTP is loopback only")
    context = None
    if args.cert:
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.minimum_version = ssl.TLSVersion.TLSv1_2
        context.load_cert_chain(args.cert, args.key)
    httpd = DistributionServer((args.bind, args.port), Handler, context)
    print(f"TeamHJD distribution ready; TLS={bool(args.cert)}; port={httpd.server_port}", flush=True)
    httpd.serve_forever(poll_interval=0.25)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True)
    parser.add_argument("--bind", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8443)
    parser.add_argument("--cert")
    parser.add_argument("--key")
    parser.add_argument("--test-http", action="store_true")
    serve(parser.parse_args())
