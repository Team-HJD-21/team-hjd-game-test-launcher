import base64
from datetime import datetime, timedelta, timezone
import hashlib
import http.client
import json
import os
from pathlib import Path
import socket
import shutil
import subprocess
import sys
import time
import unittest
import uuid

class ServerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = Path(__file__).resolve().parents[1] / ".local" / ("server-test-" + uuid.uuid4().hex)
        cls.root.mkdir(parents=True)
        (cls.root / "packages").mkdir()
        cls.package = "/packages/" + "a" * 64 + ".zip"
        (cls.root / cls.package[1:]).write_bytes(b"fixture-package")
        (cls.root / "catalog.json").write_text('{"fixture":true}')
        salt = os.urandom(32)
        cls.password = b"generated-test-password"
        (cls.root / "auth.json").write_text(json.dumps({"salt": base64.b64encode(salt).decode(), "hash": base64.b64encode(hashlib.pbkdf2_hmac("sha256", cls.password, salt, 100000)).decode(), "iterations": 100000}))
        # P-256 generator point: synthetic public metadata, never an operator key.
        public_point = bytes.fromhex("6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c2964fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5")
        cls.public_key = "p256:" + base64.b64encode(public_point).decode()

    def setUp(self):
        self.tokens = []
        self.write_registry([])
        (self.root / "catalog.json").write_text('{"fixture":true}')
        with socket.socket() as sock:
            sock.bind(("127.0.0.1", 0))
            self.port = sock.getsockname()[1]
        template = {"schema": 1, "server": f"http://127.0.0.1:{self.port}/", "publisherKey": self.public_key, "certificate": "synthetic public certificate fixture"}
        (self.root / "invite.json").write_text(json.dumps(template))
        self.process = subprocess.Popen([sys.executable, str(Path(__file__).parents[1] / "server/distribution_server.py"), "--root", str(self.root), "--port", str(self.port), "--test-http"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        for _ in range(100):
            try:
                with socket.create_connection(("127.0.0.1", self.port), timeout=.2):
                    return
            except OSError:
                time.sleep(.05)
        self.process.terminate()
        self.process.wait(5)
        raise AssertionError("Test server did not start")

    def tearDown(self):
        self.process.terminate()
        stdout, stderr = self.process.communicate(timeout=5)
        for token in self.tokens:
            self.assertNotIn(token.encode(), stdout + stderr, "Credential leaked into a server log")
        # Preserve these small synthetic fixtures under the ignored workspace test directory.

    def write_registry(self, entries, **metadata):
        temporary = self.root / ("registry-" + uuid.uuid4().hex + ".tmp")
        temporary.write_text(json.dumps({"schema": 1, "testers": entries, **metadata}), encoding="utf-8")
        temporary.replace(self.root / "testers.json")

    def token(self, *, days=7, revoked=False, token_id=None):
        token_id = token_id or os.urandom(8).hex()
        token = "hjd1." + token_id + "." + base64.urlsafe_b64encode(os.urandom(32)).decode().rstrip("=")
        self.tokens.append(token)
        entry = {"tokenId": token_id, "tokenSha256": hashlib.sha256(token.encode()).hexdigest(), "recipientLabel": "Synthetic tester", "expiresAt": (datetime.now(timezone.utc) + timedelta(days=days)).isoformat(), "revoked": revoked}
        return token, entry

    def request(self, path, headers=None, auth=True, method="GET", credential=None):
        connection = http.client.HTTPConnection("127.0.0.1", self.port, timeout=5)
        raw = self.password if credential is None else credential.encode() if isinstance(credential, str) else credential
        h = {"Authorization": "Bearer " + base64.b64encode(raw).decode()} if auth else {}
        h.update(headers or {})
        connection.request(method, path, headers=h)
        response = connection.getresponse()
        data = response.read()
        result = response.status, dict(response.getheaders()), data
        connection.close()
        return result

    def test_catalog_requires_password(self):
        self.assertEqual(self.request("/catalog.json", auth=False)[0], 401)
        self.assertEqual(self.request("/catalog.json")[0], 200)

    def test_private_routes_are_hidden(self):
        for p in ["/auth.json", "/invite.json", "/testers.json", "/team-invites/tester.hjdinvite", "/publisher.dpapi", "/../auth.json", "/%2e%2e/auth.json", "/catalog.json?password=secret"]:
            self.assertEqual(self.request(p)[0], 404, p)

    def test_retired_pages_require_authentication(self):
        for method in ("GET", "HEAD"):
            for route in ("/", "/downloads"):
                status, headers, body = self.request(route, auth=False, method=method)
                self.assertEqual((status, body), (401, b""))
                self.assertNotIn("Location", headers)

    def test_retired_pages_are_empty_without_redirect(self):
        for method in ("GET", "HEAD"):
            for route in ("/", "/downloads"):
                status, headers, body = self.request(route, method=method)
                self.assertEqual((status, body, headers["Content-Length"]), (410, b"", "0"))
                self.assertEqual(headers["Cache-Control"], "no-store")
                self.assertNotIn("Location", headers)
                self.assertNotIn("Content-Type", headers)
        self.assertEqual(self.request("/downloads", {"Range": "bytes=0-"})[0], 410)

    def test_retired_pages_do_not_render_private_metadata(self):
        private = '<script>PRIVATE_GAME_AND_TOKEN_SENTINEL</script>'
        (self.root / "catalog.json").write_text(private, encoding="utf-8")
        (self.root / "invite.json").write_text(private, encoding="utf-8")
        for route in ("/", "/downloads"):
            status, headers, body = self.request(route)
            self.assertEqual((status, body), (410, b""))
            self.assertNotIn("Location", headers)
        self.assertEqual(self.request(self.package)[2], b"fixture-package")

    def test_retirement_preserves_access_probes_and_tester_api(self):
        # cloudflared 2026.10.5 adds this query while verifying a fetched token.
        # The old origin already returned 404 here; only the edge login redirect
        # means invalid Access credentials to that pinned helper.
        token, entry = self.token()
        self.write_registry([entry])
        for route in ("/?cloudflared_token_check=true", "/downloads?platform=macos", "/downloads/"):
            status, headers, body = self.request(route, credential=token)
            self.assertEqual((status, body), (404, b""))
            self.assertNotIn("Location", headers)
        self.assertEqual(self.request("/downloads", credential=token)[0], 410)
        self.assertEqual(self.request("/catalog.json", credential=token)[0], 200)
        self.assertEqual(self.request("/catalog.json", credential=token, method="HEAD")[2], b"")
        status, headers, body = self.request(self.package, {"Range": "bytes=8-"}, credential=token)
        self.assertEqual((status, body, headers["Content-Range"]), (206, b"package", "bytes 8-14/15"))
        status, headers, body = self.request(self.package, credential=token, method="HEAD")
        self.assertEqual((status, body, headers["Content-Length"]), (200, b"", "15"))

    def test_package_and_resume(self):
        status, headers, data = self.request(self.package)
        self.assertEqual((status, data), (200, b"fixture-package"))
        status, headers, data = self.request(self.package, {"Range": "bytes=8-"})
        self.assertEqual((status, data), (206, b"package"))
        self.assertEqual(headers["Content-Range"], "bytes 8-14/15")

    def test_invalid_ranges(self):
        for value in ["bytes=999-", "bytes=-4", "bytes=1-2", "bytes=0-1,3-4"]:
            self.assertEqual(self.request(self.package, {"Range": value})[0], 416)

    def test_head(self):
        status, headers, data = self.request(self.package, method="HEAD")
        self.assertEqual((status, data, headers["Content-Length"]), (200, b"", "15"))

    def test_valid_tester_token(self):
        token, entry = self.token()
        self.write_registry([entry])
        self.assertEqual(self.request("/catalog.json", credential=token)[0], 200)
        self.assertEqual(self.request(self.package, {"Range": "bytes=8-"}, credential=token)[0], 206)

    def test_expired_tester_token(self):
        token, entry = self.token(days=-1)
        self.write_registry([entry])
        self.assertEqual(self.request("/catalog.json", credential=token)[0], 401)
        self.assertEqual(self.request("/catalog.json")[0], 200)

    def test_revoked_tester_token(self):
        token, entry = self.token(revoked=True)
        self.write_registry([entry])
        self.assertEqual(self.request("/catalog.json", credential=token)[0], 401)

    def test_wrong_tester_secret(self):
        token, entry = self.token()
        self.write_registry([entry])
        wrong = token[:-1] + ("A" if token[-1] != "A" else "B")
        self.assertEqual(self.request("/catalog.json", credential=wrong)[0], 401)
        unknown, _ = self.token()
        self.assertEqual(self.request("/catalog.json", credential=unknown)[0], 401)

    def test_malformed_tester_tokens(self):
        malformed = ["hjd1.bad", "hjd1." + "a" * 16 + "." + "A" * 42,
                     "hjd1." + "A" * 16 + "." + "A" * 43, "hjd1." + "a" * 16 + "." + "A" * 42 + "+",
                     "hjd1." + "a" * 16 + "." + "A" * 43 + ".extra", "hjd1." + "a" * 16 + "." + "A" * 43 + "\n"]
        for token in malformed:
            self.assertEqual(self.request("/catalog.json", credential=token)[0], 401)

    def test_live_registry_reload(self):
        token, entry = self.token()
        process_id = self.process.pid
        self.assertEqual(self.request("/catalog.json", credential=token)[0], 401)
        self.write_registry([entry])
        self.assertEqual(self.request("/catalog.json", credential=token)[0], 200)
        self.write_registry([{**entry, "revoked": True}])
        self.assertEqual(self.request("/catalog.json", credential=token)[0], 401)
        self.assertEqual(self.process.pid, process_id, "Registry changes must not require a server restart")

    def test_missing_or_invalid_registry_fails_closed(self):
        token, entry = self.token()
        (self.root / "testers.json").unlink()
        self.assertEqual(self.request("/catalog.json", credential=token)[0], 401)
        (self.root / "testers.json").write_text('{"schema":1,"testers":"bad"}')
        self.assertEqual(self.request("/catalog.json", credential=token)[0], 401)
        self.write_registry([entry, entry])
        self.assertEqual(self.request("/catalog.json", credential=token)[0], 401)
        self.assertEqual(self.request("/catalog.json")[0], 200, "Operator password must remain available")

    def test_registry_bounds_and_utc_expiry(self):
        token, entry = self.token()
        (self.root / "testers.json").write_bytes(b" " * (2 * 1024 * 1024 + 1))
        self.assertEqual(self.request("/catalog.json", credential=token)[0], 401)
        oversized = [{**entry, "tokenId": f"{i:016x}"} for i in range(2001)]
        self.write_registry(oversized)
        self.assertEqual(self.request("/catalog.json", credential=token)[0], 401)
        self.write_registry([{**entry, "expiresAt": "2099-01-01T00:00:00"}])
        self.assertEqual(self.request("/catalog.json", credential=token)[0], 401)
        self.write_registry([{**entry, "expiresAt": "2099-01-01T00:00:00+09:00"}])
        self.assertEqual(self.request("/catalog.json", credential=token)[0], 401)
        self.write_registry([entry])
        self.assertEqual(self.request("/catalog.json", credential=token)[0], 200)

    def run_invite_tool(self, name, *arguments):
        powershell = shutil.which("pwsh")
        if not powershell:
            self.skipTest("PowerShell 7 is required for operator script tests")
        return subprocess.run([powershell, "-NoProfile", "-File", str(Path(__file__).parents[1] / "tools" / name), "-PrivateRoot", str(self.root), *arguments], capture_output=True, text=True, encoding="utf-8", timeout=15, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))

    def test_invite_tools_roundtrip_and_metadata(self):
        existing_token, existing = self.token()
        existing["customNote"] = "Preserve synthetic metadata"
        self.write_registry([existing], operatorNote="Keep registry metadata")
        created = self.run_invite_tool("New-TestInvite.ps1", "-Label", "Synthetic recipient", "-Days", "2")
        self.assertEqual(created.returncode, 0, created.stderr)
        self.assertNotIn("hjd1.", created.stdout)
        result = dict(line.strip().split("=", 1) for line in created.stdout.splitlines() if "=" in line)
        invite = json.loads(Path(result["InvitePath"]).read_text("utf-8-sig"))
        token = invite["accessToken"]
        self.tokens.append(token)
        self.assertEqual(invite["tokenId"], result["TokenId"])
        self.assertEqual(invite["publisherKey"], self.public_key)
        self.assertEqual(invite["server"], f"http://127.0.0.1:{self.port}/")
        self.assertEqual(invite["certificate"], "synthetic public certificate fixture")
        self.assertEqual(invite["recipientLabel"], "Synthetic recipient")
        self.assertEqual(self.request("/catalog.json", credential=token)[0], 200)
        registry_bytes = (self.root / "testers.json").read_bytes()
        self.assertNotIn(token.encode(), registry_bytes)
        registry = json.loads(registry_bytes)
        self.assertEqual(registry["operatorNote"], "Keep registry metadata")
        self.assertEqual(registry["testers"][0]["customNote"], existing["customNote"])
        issued = next(record for record in registry["testers"] if record["tokenId"] == invite["tokenId"])
        self.assertEqual(issued["tokenSha256"], hashlib.sha256(token.encode()).hexdigest())
        expires = datetime.fromisoformat(invite["expiresAt"])
        self.assertGreater(expires, datetime.now(timezone.utc) + timedelta(hours=47))
        self.assertLess(expires, datetime.now(timezone.utc) + timedelta(hours=49))
        revoked = self.run_invite_tool("Revoke-TestInvite.ps1", "-TokenId", invite["tokenId"])
        self.assertEqual(revoked.returncode, 0, revoked.stderr)
        self.assertNotIn("hjd1.", revoked.stdout)
        self.assertEqual(self.request("/catalog.json", credential=token)[0], 401)
        self.assertEqual(self.request("/catalog.json", credential=existing_token)[0], 200)
        registry = json.loads((self.root / "testers.json").read_bytes())
        self.assertEqual(registry["operatorNote"], "Keep registry metadata")
        self.assertEqual(registry["testers"][0]["customNote"], existing["customNote"])
        self.assertTrue(next(record for record in registry["testers"] if record["tokenId"] == invite["tokenId"])["revoked"])
        again = self.run_invite_tool("Revoke-TestInvite.ps1", "-TokenId", invite["tokenId"])
        self.assertEqual(again.returncode, 0, again.stderr)

    def test_invite_tools_limits_and_lock(self):
        original = (self.root / "testers.json").read_bytes()
        invalid = self.run_invite_tool("New-TestInvite.ps1", "-Label", "Synthetic recipient", "-Days", "31")
        self.assertNotEqual(invalid.returncode, 0)
        self.assertEqual((self.root / "testers.json").read_bytes(), original)
        if os.name == "nt":
            with (self.root / "testers.lock").open("a+b"):
                locked = self.run_invite_tool("New-TestInvite.ps1", "-Label", "Synthetic recipient")
            self.assertNotEqual(locked.returncode, 0)
            self.assertEqual((self.root / "testers.json").read_bytes(), original)

    def test_password_throttle(self):
        for _ in range(10):
            self.request("/catalog.json", {"Authorization": "Bearer " + base64.b64encode(b"wrong").decode()})
        self.assertEqual(self.request("/catalog.json", auth=False)[0], 429)


if __name__ == "__main__":
    # Each test restarts only its own synthetic server; live reload is tested within one process.
    suite = unittest.TestSuite()
    for name in ["test_catalog_requires_password", "test_private_routes_are_hidden", "test_package_and_resume", "test_invalid_ranges", "test_head", "test_valid_tester_token", "test_expired_tester_token", "test_revoked_tester_token", "test_wrong_tester_secret", "test_malformed_tester_tokens", "test_live_registry_reload", "test_missing_or_invalid_registry_fails_closed", "test_registry_bounds_and_utc_expiry", "test_invite_tools_roundtrip_and_metadata", "test_invite_tools_limits_and_lock", "test_password_throttle", "test_retired_pages_require_authentication", "test_retired_pages_are_empty_without_redirect", "test_retired_pages_do_not_render_private_metadata", "test_retirement_preserves_access_probes_and_tester_api"]:
        suite.addTest(ServerTests(name))
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    raise SystemExit(not result.wasSuccessful())
