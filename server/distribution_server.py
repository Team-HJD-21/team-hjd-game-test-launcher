"""Read-only TeamHJD distribution server. No execution or upload endpoint."""
from __future__ import annotations
import argparse
import base64
import hashlib
import hmac
import json
from pathlib import Path
import re
import ssl
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit


class DistributionServer(ThreadingHTTPServer):
    daemon_threads = True
    request_queue_size = 32


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
            header = self.headers.get("Authorization", "")
            password = b""
            try:
                if not header.startswith("Bearer ") or len(header) > 2048:
                    raise ValueError("missing")
                password = base64.b64decode(header[7:], validate=True)
                if not 1 <= len(password) <= 1024:
                    raise ValueError("size")
            except (ValueError, TypeError):
                with attempts_lock:
                    attempts.setdefault(peer, []).append(now)
                self.reject(401)
                return False
            if not hmac.compare_digest(hashlib.pbkdf2_hmac("sha256", password, salt, iterations), expected):
                with attempts_lock:
                    attempts.setdefault(peer, []).append(now)
                self.reject(401)
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
    httpd = DistributionServer((args.bind, args.port), Handler)
    if args.cert:
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.minimum_version = ssl.TLSVersion.TLSv1_2
        context.load_cert_chain(args.cert, args.key)
        httpd.socket = context.wrap_socket(httpd.socket, server_side=True)
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
