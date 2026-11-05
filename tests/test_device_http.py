"""Real HTTP/device proofs with a synthetic verified email at the edge boundary."""
import base64
from concurrent.futures import ThreadPoolExecutor
import hashlib
import http.client
import json
import os
from pathlib import Path
import subprocess
import sys
import threading
import time
from types import SimpleNamespace
import unittest
from unittest import mock
import uuid

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils
from server import distribution_server as distribution
from server.cloudflare_auth import CloudflareAccess, VerifiedIdentity
from server.device_auth import proof_message


class DeviceHttpTests(unittest.TestCase):
    def setUp(self):
        self.root = Path(__file__).resolve().parents[1] / ".local" / ("device-http-" + uuid.uuid4().hex)
        self.root.mkdir()
        (self.root / "packages").mkdir()
        self.package = b"device-http-package\n" * 16384
        self.digest = hashlib.sha256(self.package).hexdigest()
        (self.root / "packages" / (self.digest + ".zip")).write_bytes(self.package)
        (self.root / "catalog.json").write_bytes(b'{"fixture":"device-catalog"}')
        salt = os.urandom(32)
        (self.root / "auth.json").write_text(json.dumps({"salt": base64.b64encode(salt).decode(), "hash": base64.b64encode(hashlib.pbkdf2_hmac("sha256", b"fixture-owner-password", salt, 100000)).decode(), "iterations": 100000}), encoding="utf-8")
        (self.root / "client-policy.json").write_text(json.dumps({"schema": 1, "minimumVersion": "0.2.3", "privacyVersion": "2026-11-05.3"}), encoding="utf-8")
        self.cf = CloudflareAccess(self.root)
        identity = VerifiedIdentity("person@example.invalid", "a" * 64, int(time.time()) + 3600)
        self.cf.verify = lambda token: identity if token == "synthetic-verified-edge-assertion" else None
        self.httpd = None
        ready = threading.Event()
        original_server = distribution.DistributionServer
        def create_server(address, handler, context):
            # Only this isolated test adapter models the edge's Cookie→JWT header
            # handoff. Production origin continues to reject cookies as identity.
            class SyntheticEdge(handler):
                def authorized(edge_self):
                    if edge_self.headers.get("Cookie") == "CF_Authorization=synthetic-edge-session":
                        edge_self.headers["Cf-Access-Jwt-Assertion"] = "synthetic-verified-edge-assertion"
                    return super().authorized()
            self.httpd = original_server(address, SyntheticEdge, context)
            ready.set()
            return self.httpd
        args = SimpleNamespace(root=str(self.root), bind="127.0.0.1", port=0, cert=None, key=None, test_http=True)
        self.patches = [mock.patch.object(distribution, "CloudflareAccess", return_value=self.cf),
                        mock.patch.object(distribution, "DistributionServer", side_effect=create_server)]
        for patch in self.patches:
            patch.start()
        self.thread = threading.Thread(target=distribution.serve, args=(args,), daemon=True)
        self.thread.start()
        if not ready.wait(5):
            self.fail("isolated origin startup timed out")
        self.port = self.httpd.server_port
        self.addCleanup(self.stop)
        self.key = ec.generate_private_key(ec.SECP256R1())
        point = self.key.public_key().public_bytes(serialization.Encoding.X962, serialization.PublicFormat.UncompressedPoint)
        self.public = "p256:" + base64.b64encode(point[1:]).decode("ascii")

    def stop(self):
        if self.httpd:
            self.httpd.shutdown()
            self.thread.join(5)
            self.httpd.server_close()
        for patch in reversed(self.patches):
            patch.stop()

    def request(self, method, path, body=None, headers=None):
        connection = http.client.HTTPConnection("127.0.0.1", self.port, timeout=5)
        try:
            connection.request(method, path, body=body, headers=headers or {})
            response = connection.getresponse()
            return response.status, response.read(), dict(response.getheaders())
        finally:
            connection.close()

    def policy(self):
        return {"X-TeamHJD-Launcher-Version": "0.2.3", "X-TeamHJD-Privacy-Version": "2026-11-05.3"}

    def enroll(self):
        headers = self.policy() | {"Cf-Access-Jwt-Assertion": "synthetic-verified-edge-assertion"}
        status, body, _ = self.request("POST", "/launcher-device/enroll", json.dumps({"publicKey": self.public}), headers)
        self.assertEqual(status, 200)
        return json.loads(body)["deviceId"]

    def signed(self, device, path="/client-device/catalog.json", method="GET", byte_range=""):
        status, body, _ = self.request("POST", "/client-device/challenge", json.dumps({"deviceId": device}))
        self.assertEqual(status, 200)
        nonce = json.loads(body)["challenge"]
        der = self.key.sign(proof_message(nonce, method, path, byte_range, "0.2.3", "2026-11-05.3"), ec.ECDSA(hashes.SHA256()))
        r, s = utils.decode_dss_signature(der)
        headers = self.policy() | {"X-TeamHJD-Device": device, "X-TeamHJD-Challenge": nonce,
                                    "X-TeamHJD-Proof": base64.b64encode(r.to_bytes(32, "big") + s.to_bytes(32, "big")).decode("ascii")}
        if byte_range:
            headers["Range"] = byte_range
        return headers

    def test_enrollment_requires_verified_email_consent_and_no_browser_origin(self):
        body = json.dumps({"publicKey": self.public})
        self.assertEqual(self.request("POST", "/launcher-device/enroll", body, self.policy())[0], 401)
        owner = self.policy() | {"Authorization": "Bearer " + base64.b64encode(b"fixture-owner-password").decode("ascii")}
        self.assertEqual(self.request("POST", "/launcher-device/enroll", body, owner)[0], 401)
        headers = {"Cf-Access-Jwt-Assertion": "synthetic-verified-edge-assertion", "X-TeamHJD-Launcher-Version": "0.2.3"}
        self.assertEqual(self.request("POST", "/launcher-device/enroll", body, headers)[0], 428)
        headers |= self.policy() | {"Origin": "https://untrusted.invalid"}
        status, _, result_headers = self.request("POST", "/launcher-device/enroll", body, headers)
        self.assertEqual(status, 403)
        self.assertNotIn("Access-Control-Allow-Origin", result_headers)
        self.enroll()

    def test_catalog_range_head_replay_and_route_binding(self):
        device = self.enroll()
        headers = self.signed(device)
        self.assertEqual(self.request("GET", "/client-device/catalog.json", headers=headers)[:2], (200, b'{"fixture":"device-catalog"}'))
        self.assertEqual(self.request("GET", "/client-device/catalog.json", headers=headers)[0], 409)
        package = "/client-device/packages/" + self.digest + ".zip"
        headers = self.signed(device, package, byte_range="bytes=123-")
        status, data, response_headers = self.request("GET", package, headers=headers)
        self.assertEqual((status, data), (206, self.package[123:]))
        self.assertEqual(response_headers["Content-Range"], f"bytes 123-{len(self.package)-1}/{len(self.package)}")
        self.assertEqual(self.request("HEAD", package, headers=self.signed(device, package, "HEAD"))[:2], (200, b""))
        headers = self.signed(device)
        self.assertEqual(self.request("GET", package, headers=headers)[0], 401)
        self.assertEqual(self.request("GET", "/catalog.json", headers=headers)[0], 401)
        self.assertEqual(self.request("GET", "/client-device/../catalog.json", headers=headers)[0], 403)

    def test_revoke_is_immediate_and_user_deletion_does_not_recreate_identity(self):
        device = self.enroll()
        before = json.loads((self.root / "verified-users.json").read_text(encoding="utf-8-sig"))["users"][0]
        for _ in range(3):
            self.assertEqual(self.request("GET", "/client-device/catalog.json", headers=self.signed(device))[0], 200)
        after = json.loads((self.root / "verified-users.json").read_text(encoding="utf-8-sig"))["users"][0]
        self.assertEqual((before["loginCount"], before["lastVerifiedAt"]), (after["loginCount"], after["lastVerifiedAt"]))
        headers = self.signed(device, "/client-device/revoke", "POST")
        self.assertEqual(self.request("POST", "/client-device/revoke", "{}", headers)[0], 200)
        self.assertEqual(self.request("POST", "/client-device/challenge", json.dumps({"deviceId": device}))[0], 401)
        self.key = ec.generate_private_key(ec.SECP256R1())
        point = self.key.public_key().public_bytes(serialization.Encoding.X962, serialization.PublicFormat.UncompressedPoint)
        self.public = "p256:" + base64.b64encode(point[1:]).decode("ascii")
        device = self.enroll()
        headers = self.signed(device)
        self.cf.maintain_users(apply=True, remove_email="person@example.invalid")
        self.assertEqual(self.request("GET", "/client-device/catalog.json", headers=headers)[0], 401)
        self.assertEqual(json.loads((self.root / "verified-users.json").read_text(encoding="utf-8-sig"))["users"], [])
        self.assertEqual(json.loads((self.root / "trusted-devices.json").read_text(encoding="utf-8-sig"))["devices"], [])

    def test_storage_error_is_unavailable_not_revocation_and_malformed_body_is_bounded(self):
        device = self.enroll()
        headers = self.signed(device)
        (self.root / "trusted-devices.json").write_text("{}", encoding="utf-8")
        self.assertEqual(self.request("GET", "/client-device/catalog.json", headers=headers)[0], 503)
        self.assertEqual(self.request("POST", "/client-device/challenge", "not json")[0], 400)
        self.assertEqual(self.request("POST", "/client-device/challenge", "x" * 4097)[0], 400)
        self.assertEqual(self.request("POST", "/client-device/challenge", "{}", {"Origin": "https://untrusted.invalid"})[0], 403)

    def test_new_device_flow_can_be_verified_before_legacy_policy_cutover(self):
        (self.root / "client-policy.json").write_text(json.dumps({"schema": 1, "minimumVersion": "0.2.2", "privacyVersion": "2026-11-05.1"}), encoding="utf-8")
        device = self.enroll()
        old = {"Cf-Access-Jwt-Assertion": "synthetic-verified-edge-assertion", "X-TeamHJD-Launcher-Version": "0.2.2", "X-TeamHJD-Privacy-Version": "2026-11-05.1"}
        self.assertEqual(self.request("GET", "/catalog.json", headers=old)[0], 200)
        user = json.loads((self.root / "verified-users.json").read_text(encoding="utf-8-sig"))["users"][0]
        self.assertEqual(user["privacyConsent"]["version"], "2026-11-05.3")
        self.assertEqual(self.request("GET", "/client-device/catalog.json", headers=self.signed(device))[0], 200)
        # This feature-specific gate must not permit old/no-consent enrollment.
        self.assertEqual(self.request("POST", "/launcher-device/enroll", json.dumps({"publicKey": self.public}), old)[0], 426)

    @unittest.skipUnless((os.name == "nt" or sys.platform == "darwin") and os.environ.get("HJD_RUN_NATIVE_DEVICE_PROBE") == "1", "explicit native cross-language transport probe")
    def test_native_client_enroll_persist_catalog_resume_and_logout(self):
        default = Path(__file__).resolve().parents[1] / "build/LauncherClientExperienceTests.exe"
        executable = Path(os.environ.get("HJD_DEVICE_PROBE_EXECUTABLE", str(default)))
        output = self.root / "native-profile"
        command = [str(executable), "--device-probe", f"http://127.0.0.1:{self.port}/", str(output), self.digest, str(len(self.package))]
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=40, check=False)
        self.assertEqual(result.returncode, 0, result.stdout.decode("utf-8", errors="replace"))
        self.assertIn(b"PASS native device HTTP enrollment/persistence/catalog/resume/revocation", result.stdout)


if __name__ == "__main__":
    unittest.main()
