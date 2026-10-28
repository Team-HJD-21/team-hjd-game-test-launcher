"""Synthetic Access JWT tests. Run with .deps/server-venv/Scripts/python.exe."""
from datetime import datetime, timezone
import hashlib
import http.client
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import importlib.util
import json
import os
from pathlib import Path
import socket
import ssl
import subprocess
import sys
import threading
import time
import unittest
from unittest import mock
import urllib.request
import uuid

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT / "server"))
from cloudflare_auth import AccessDependencyError, AuditUnavailable, CloudflareAccess, JWKS_TTL_SECONDS, fetch_jwks

HAS_CRYPTO = importlib.util.find_spec("jwt") is not None and importlib.util.find_spec("cryptography") is not None
if HAS_CRYPTO:
    import jwt
    from cryptography.hazmat.primitives.asymmetric import rsa


@unittest.skipUnless(HAS_CRYPTO, "Use the project server-venv for Cloudflare JWT tests")
class CloudflareTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.private_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
        cls.other_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
        cls.issuer = "https://synthetic-team.cloudflareaccess.com"
        cls.audience = "synthetic-application-audience"
        cls.kid = "synthetic-kid"
        cls.jwk = json.loads(jwt.algorithms.RSAAlgorithm.to_jwk(cls.private_key.public_key()))
        cls.jwk.update({"kid": cls.kid, "alg": "RS256", "use": "sig"})
        cls.jwks_bytes = json.dumps({"keys": [cls.jwk]}).encode()

    def setUp(self):
        self.root = PROJECT / ".local" / ("cf-test-" + uuid.uuid4().hex)
        self.root.mkdir(parents=True)
        self.config()
        self.calls = []
        self.clock = [1000.0]
        self.fail_fetch = False
        self.tokens = []
        self.auth = CloudflareAccess(self.root, key_fetcher=self.key_fetcher, monotonic=lambda: self.clock[0])

    def config(self, **overrides):
        value = {"enabled": True, "issuer": self.issuer, "audience": self.audience, **overrides}
        (self.root / "cloudflare-access.json").write_text(json.dumps(value), encoding="utf-8")

    def key_fetcher(self, url):
        self.calls.append(url)
        if self.fail_fetch:
            raise OSError("Synthetic unavailable key endpoint")
        return self.jwks_bytes

    def token(self, claims=None, *, headers=None, key=None, algorithm="RS256"):
        now = int(time.time())
        payload = {"iss": self.issuer, "aud": [self.audience], "exp": now + 3600, "iat": now - 10,
                   "nbf": now - 10, "email": "verified@example.test", "sub": "synthetic-user", "type": "app"}
        payload.update(claims or {})
        value = jwt.encode(payload, key or self.private_key, algorithm=algorithm, headers={"kid": self.kid, **(headers or {})})
        self.tokens.append(value)
        return value

    def test_normal_cloudflare_app_without_email_verified(self):
        token = self.token()
        identity = self.auth.verify(token)
        self.assertIsNotNone(identity)
        self.assertEqual(identity.email, "verified@example.test")
        self.assertEqual(identity.session_hash, hashlib.sha256(token.encode()).hexdigest())
        self.assertEqual(self.calls, [self.issuer + "/cdn-cgi/access/certs"])

    def test_signature_issuer_audience_algorithm(self):
        self.assertIsNone(self.auth.verify(self.token(key=self.other_key)))
        self.assertIsNone(self.auth.verify(self.token({"iss": "https://different-team.cloudflareaccess.com"})))
        self.assertIsNone(self.auth.verify(self.token({"aud": ["different-application"]})))
        self.assertIsNone(self.auth.verify(self.token({"aud": [self.audience, "other-app"]})))
        self.assertIsNone(self.auth.verify(self.token(algorithm="HS256", key="synthetic-hmac-key-with-at-least-32-bytes")))

    def test_time_bounds_and_claim_types(self):
        now = int(time.time())
        for claims in [{"exp": now - 60}, {"nbf": now + 90}, {"iat": now + 90}, {"exp": now + 32 * 86400},
                       {"exp": "9999999999"}, {"iat": True}, {"nbf": 0}, {"type": "org"}, {"sub": ""}]:
            with self.subTest(claim=next(iter(claims))):
                self.assertIsNone(self.auth.verify(self.token(claims)))

    def test_missing_and_unverified_email(self):
        for claims in [{"email": ""}, {"email": None}, {"email": "not-an-email"}, {"email": "x@example.test\r\nInjected"},
                       {"email_verified": False}, {"email_verified": "true"}]:
            self.assertIsNone(self.auth.verify(self.token(claims)))
        self.assertIsNotNone(self.auth.verify(self.token({"email_verified": True})))

    def test_missing_config_and_invalid_issuer_fail_closed(self):
        token = self.token()
        for issuer in ["http://synthetic-team.cloudflareaccess.com", "https://cloudflareaccess.com.evil.test", "https://synthetic-team.cloudflareaccess.com/", "https://user@synthetic-team.cloudflareaccess.com", "https://synthetic-team.cloudflareaccess.com:443", "https://synthetic-team.cloudflareaccess.com?redirect=evil"]:
            self.config(issuer=issuer)
            self.assertIsNone(self.auth.verify(token))
        self.assertEqual(self.calls, [], "Invalid configuration must not trigger network requests")
        (self.root / "cloudflare-access.json").unlink()
        with mock.patch("cloudflare_auth.importlib.import_module", side_effect=ImportError("synthetic")):
            legacy = CloudflareAccess(self.root)
            self.assertIsNone(legacy.verify(token))
        self.config()
        with mock.patch("cloudflare_auth.importlib.import_module", side_effect=ImportError("synthetic")):
            with self.assertRaises(AccessDependencyError):
                CloudflareAccess(self.root)

    def test_untrusted_header_urls_are_ignored(self):
        token = self.token(headers={"jku": "http://127.0.0.1/attacker", "x5u": "https://evil.invalid/key"})
        self.assertIsNotNone(self.auth.verify(token))
        self.assertEqual(self.calls, [self.issuer + "/cdn-cgi/access/certs"])

    def test_key_cache_failure_expiry_and_unknown_kid(self):
        token = self.token()
        self.assertIsNotNone(self.auth.verify(token))
        self.fail_fetch = True
        self.assertIsNotNone(self.auth.verify(token), "A matching key within TTL should stay usable")
        self.assertEqual(len(self.calls), 1)
        self.assertIsNone(self.auth.verify(self.token(headers={"kid": "unknown-kid"})))
        self.assertEqual(len(self.calls), 1, "Unknown-key spam must respect refresh cooldown")
        self.clock[0] += JWKS_TTL_SECONDS + 1
        self.assertIsNone(self.auth.verify(token), "An expired key cache must not bypass a fetch failure")
        self.assertEqual(len(self.calls), 2)
        self.fail_fetch = False
        self.clock[0] += 16
        self.assertIsNotNone(self.auth.verify(token))
        self.assertEqual(len(self.calls), 3)
        unexpected = CloudflareAccess(self.root, key_fetcher=mock.Mock(side_effect=RuntimeError("synthetic fetch failure")))
        self.assertIsNone(unexpected.verify(token))

    def test_key_set_bounds_and_duplicate_kid(self):
        for raw in [b" " * (256 * 1024 + 1), json.dumps({"keys": [self.jwk, self.jwk]}).encode(), json.dumps({"keys": [{**self.jwk, "alg": "HS256"}]}).encode()]:
            auth = CloudflareAccess(self.root, key_fetcher=lambda url, data=raw: data)
            self.assertIsNone(auth.verify(self.token()))

    def test_email_audit_deduplicates_interleaved_sessions_and_restart(self):
        first = self.token()
        second = self.token({"identity_nonce": "second-synthetic-session"})
        for token in (first, first, second, first):
            self.auth.record_access(self.auth.verify(token))
        data = json.loads((self.root / "verified-users.json").read_text("utf-8"))
        self.assertEqual(len(data["users"]), 1)
        record = data["users"][0]
        self.assertEqual(record["loginCount"], 2)
        self.assertEqual(record["source"], "cloudflare-access-otp")
        for field in ("firstVerifiedAt", "lastVerifiedAt", "lastAccessAt"):
            self.assertEqual(datetime.fromisoformat(record[field].replace("Z", "+00:00")).utcoffset().total_seconds(), 0)
        fresh = CloudflareAccess(self.root, key_fetcher=self.key_fetcher)
        fresh.record_access(fresh.verify(first))
        after = json.loads((self.root / "verified-users.json").read_text("utf-8"))
        self.assertEqual(after["users"][0]["loginCount"], 2)
        raw = (self.root / "verified-users.json").read_text("utf-8")
        self.assertNotIn(first, raw)
        self.assertNotIn(second, raw)
        self.assertNotIn("ip", record)

    def test_audit_failure_is_explicit_and_preserves_existing_file(self):
        path = self.root / "verified-users.json"
        path.write_text('{"schema":1,"users":"broken"}')
        before = path.read_bytes()
        with self.assertRaises(AuditUnavailable):
            self.auth.record_access(self.auth.verify(self.token()))
        self.assertEqual(path.read_bytes(), before)

    def test_parallel_audit_is_atomic(self):
        identity = self.auth.verify(self.token())
        errors = []
        def record():
            try:
                self.auth.record_access(identity)
            except Exception as error:
                errors.append(type(error).__name__)
        workers = [threading.Thread(target=record) for _ in range(12)]
        for worker in workers:
            worker.start()
        for worker in workers:
            worker.join()
        self.assertEqual(errors, [])
        data = json.loads((self.root / "verified-users.json").read_text("utf-8"))
        self.assertEqual(data["users"][0]["loginCount"], 1)

    def test_fetcher_keeps_tls_validation_timeout_and_no_redirect(self):
        url = self.issuer + "/cdn-cgi/access/certs"
        response = mock.MagicMock()
        response.status = 200
        response.geturl.return_value = url
        response.read.return_value = self.jwks_bytes
        opener = mock.MagicMock()
        opener.open.return_value.__enter__.return_value = response
        with mock.patch("cloudflare_auth.urllib.request.build_opener", return_value=opener) as builder:
            self.assertEqual(fetch_jwks(url), self.jwks_bytes)
            https = next(handler for handler in builder.call_args.args if isinstance(handler, urllib.request.HTTPSHandler))
            self.assertEqual(https._context.verify_mode, ssl.CERT_REQUIRED)
            self.assertTrue(https._context.check_hostname)
            self.assertEqual(opener.open.call_args.kwargs["timeout"], 5)
            response.geturl.return_value = "http://evil.invalid/keys"
            with self.assertRaises(ValueError):
                fetch_jwks(url)

    def test_actual_http_origin_accepts_only_verified_header_and_audits(self):
        # A local key HTTP fixture is injected only in this synthetic launcher wrapper.
        owner = self
        key_requests = []
        class Keys(BaseHTTPRequestHandler):
            def log_message(self, *args):
                pass
            def do_GET(self):
                key_requests.append(self.path)
                self.send_response(200)
                self.send_header("Content-Length", str(len(owner.jwks_bytes)))
                self.end_headers()
                self.wfile.write(owner.jwks_bytes)
        keys = ThreadingHTTPServer(("127.0.0.1", 0), Keys)
        threading.Thread(target=keys.serve_forever, daemon=True).start()
        salt = os.urandom(32)
        operator = b"synthetic-operator-password"
        (self.root / "auth.json").write_text(json.dumps({"salt": __import__("base64").b64encode(salt).decode(), "hash": __import__("base64").b64encode(hashlib.pbkdf2_hmac("sha256", operator, salt, 100000)).decode(), "iterations": 100000}))
        (self.root / "catalog.json").write_text('{"synthetic":true}')
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", 0))
            port = listener.getsockname()[1]
        wrapper = self.root / "synthetic_server.py"
        wrapper.write_text("import sys,urllib.request\nfrom types import SimpleNamespace\nsys.path.insert(0," + repr(str(PROJECT / "server")) + ")\nimport distribution_server as server\nfrom cloudflare_auth import CloudflareAccess\ndef fetch(url):\n assert url==" + repr(self.issuer + "/cdn-cgi/access/certs") + "\n with urllib.request.urlopen(" + repr(f"http://127.0.0.1:{keys.server_port}/keys") + ",timeout=2) as response:return response.read()\nserver.CloudflareAccess=lambda root:CloudflareAccess(root,key_fetcher=fetch)\nserver.serve(SimpleNamespace(root=" + repr(str(self.root)) + ",bind='127.0.0.1',port=" + str(port) + ",cert=None,key=None,test_http=True))\n", encoding="utf-8")
        process = subprocess.Popen([sys.executable, str(wrapper)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        token = self.token()
        try:
            for _ in range(100):
                try:
                    with socket.create_connection(("127.0.0.1", port), timeout=.2):
                        break
                except OSError:
                    time.sleep(.05)
            def request(headers, path="/catalog.json"):
                connection = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
                connection.request("GET", path, headers=headers)
                response = connection.getresponse()
                data = response.read()
                result = response.status, data
                connection.close()
                return result
            self.assertEqual(request({"Cf-Access-Jwt-Assertion": token})[0], 200)
            self.assertEqual(request({"Cf-Access-Jwt-Assertion": token})[0], 200)
            self.assertEqual(request({"Cookie": "CF_Authorization=" + token})[0], 401)
            self.assertEqual(request({"Cf-Access-Authenticated-User-Email": "unverified@example.test"})[0], 401)
            self.assertEqual(request({"Cf-Access-Jwt-Assertion": self.token(key=self.other_key)})[0], 401)
            self.assertEqual(request({"Authorization": "Bearer " + __import__("base64").b64encode(operator).decode()})[0], 200)
            self.assertEqual(request({"Cf-Access-Jwt-Assertion": token}, "/verified-users.json")[0], 404)
            registry = json.loads((self.root / "verified-users.json").read_text("utf-8"))
            self.assertEqual(registry["users"][0]["loginCount"], 1)
            self.assertEqual(key_requests, ["/keys"])
            original = (self.root / "verified-users.json").read_bytes()
            (self.root / "verified-users.json").write_text('{"schema":1,"users":"unavailable"}')
            self.assertEqual(request({"Cf-Access-Jwt-Assertion": token})[0], 503)
            self.assertEqual(request({"Cf-Access-Jwt-Assertion": token, "Authorization": "Bearer " + __import__("base64").b64encode(operator).decode()})[0], 200)
            (self.root / "verified-users.json").write_bytes(original)
            self.config(issuer="http://invalid.example")
            self.assertEqual(request({"Cf-Access-Jwt-Assertion": token})[0], 401)
            self.assertEqual(request({"Authorization": "Bearer " + __import__("base64").b64encode(operator).decode()})[0], 200)
        finally:
            process.terminate()
            stdout, stderr = process.communicate(timeout=5)
            keys.shutdown()
            keys.server_close()
            for value in self.tokens:
                self.assertNotIn(value.encode(), stdout + stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
