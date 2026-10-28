import base64
import hashlib
import http.client
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
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
        with socket.socket() as sock:
            sock.bind(("127.0.0.1", 0))
            cls.port = sock.getsockname()[1]
        cls.process = subprocess.Popen([sys.executable, str(Path(__file__).parents[1] / "server/distribution_server.py"), "--root", str(cls.root), "--port", str(cls.port), "--test-http"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        for _ in range(100):
            try:
                with socket.create_connection(("127.0.0.1", cls.port), timeout=.2):
                    return
            except OSError:
                time.sleep(.05)
        raise AssertionError("Test server did not start")

    @classmethod
    def tearDownClass(cls):
        cls.process.terminate()
        cls.process.wait(5)
        # Preserve these small synthetic fixtures under the ignored workspace test directory.

    def request(self, path, headers=None, auth=True, method="GET"):
        connection = http.client.HTTPConnection("127.0.0.1", self.port, timeout=5)
        h = {"Authorization": "Bearer " + base64.b64encode(self.password).decode()} if auth else {}
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
        for p in ["/auth.json", "/invite.json", "/publisher.dpapi", "/../auth.json", "/%2e%2e/auth.json", "/", "/catalog.json?password=secret"]:
            self.assertEqual(self.request(p)[0], 404, p)

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

    def test_password_throttle(self):
        for _ in range(10):
            self.request("/catalog.json", {"Authorization": "Bearer " + base64.b64encode(b"wrong").decode()})
        self.assertEqual(self.request("/catalog.json", auth=False)[0], 429)


if __name__ == "__main__":
    # Keep the rate-limit test last so it cannot interfere with the other cases.
    suite = unittest.TestSuite()
    for name in ["test_catalog_requires_password", "test_private_routes_are_hidden", "test_package_and_resume", "test_invalid_ranges", "test_head", "test_password_throttle"]:
        suite.addTest(ServerTests(name))
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    raise SystemExit(not result.wasSuccessful())
