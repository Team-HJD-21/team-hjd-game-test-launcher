import base64
from concurrent.futures import ThreadPoolExecutor
import json
import subprocess
import sys
import time
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils
from server.device_auth import DeviceRegistry, DeviceRejected, DeviceCapacity, DeviceStorageUnavailable, RETENTION, account_key, proof_message


class DeviceAuthenticationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.now = 1793868000
        self.tick = 100.0
        self.registry = DeviceRegistry(self.root, clock=lambda: self.now, monotonic=lambda: self.tick)
        self.key = ec.generate_private_key(ec.SECP256R1())
        self.public = self.public_of(self.key)
        self.device = self.registry.enroll("person@example.invalid", self.public)

    def public_of(self, key):
        raw = key.public_key().public_bytes(serialization.Encoding.X962, serialization.PublicFormat.UncompressedPoint)
        return "p256:" + base64.b64encode(raw[1:]).decode("ascii")

    def sign(self, nonce, *, method="GET", path="/client-device/catalog.json", byte_range="", key=None):
        message = proof_message(nonce, method, path, byte_range, "0.2.3", "2026-11-05.3")
        der = (key or self.key).sign(message, ec.ECDSA(hashes.SHA256()))
        r, s = utils.decode_dss_signature(der)
        return base64.b64encode(r.to_bytes(32, "big") + s.to_bytes(32, "big")).decode("ascii")

    def verify(self, original_nonce, original_signature, **changes):
        args = {"device_id": self.device, "nonce": original_nonce, "method": "GET", "path": "/client-device/catalog.json",
                "byte_range": "", "version": "0.2.3", "consent": "2026-11-05.3", "signature": original_signature}
        args.update(changes)
        return self.registry.verify(**args)

    def test_valid_device_and_no_server_private_key(self):
        nonce = self.registry.challenge(self.device)
        self.assertEqual(self.verify(nonce, self.sign(nonce))["email"], "person@example.invalid")
        data = json.loads(self.registry.path.read_text(encoding="utf-8-sig"))
        self.assertEqual(set(data["devices"][0]), {"id", "email", "publicKey", "createdAt", "lastAccessAt", "revokedAt"})
        self.assertEqual(self.registry.enroll("person@example.invalid", self.public), self.device)
        restored = DeviceRegistry(self.root, clock=lambda: self.now, monotonic=lambda: self.tick)
        next_nonce = restored.challenge(self.device)
        self.assertEqual(restored.verify(self.device, next_nonce, "GET", "/client-device/catalog.json", "", "0.2.3", "2026-11-05.3", self.sign(next_nonce))["id"], self.device)

    def test_replay_and_concurrent_consumption(self):
        nonce = self.registry.challenge(self.device)
        signature = self.sign(nonce)
        def attempt(_):
            try:
                self.verify(nonce, signature)
                return True
            except DeviceRejected:
                return False
        with ThreadPoolExecutor(max_workers=12) as workers:
            self.assertEqual(sum(workers.map(attempt, range(20))), 1)

    def test_method_path_range_version_and_consent_are_bound(self):
        for changes in ({"method": "HEAD"}, {"path": "/client-device/packages/" + "a" * 64 + ".zip"},
                        {"byte_range": "bytes=123-"}, {"version": "0.2.2"}, {"consent": "2026-11-05.1"}):
            with self.subTest(changes=changes):
                nonce = self.registry.challenge(self.device)
                signature = self.sign(nonce)
                with self.assertRaises(DeviceRejected):
                    self.verify(nonce, signature, **changes)
                self.verify(nonce, signature)  # Invalid proof did not consume somebody else's challenge.

    def test_wrong_key_and_expiration_and_restart(self):
        nonce = self.registry.challenge(self.device)
        with self.assertRaises(DeviceRejected):
            self.verify(nonce, self.sign(nonce, key=ec.generate_private_key(ec.SECP256R1())))
        self.tick += 61
        with self.assertRaises(DeviceRejected):
            self.verify(nonce, self.sign(nonce))
        nonce = self.registry.challenge(self.device)
        restarted = DeviceRegistry(self.root, clock=lambda: self.now, monotonic=lambda: self.tick)
        with self.assertRaises(DeviceRejected):
            restarted.verify(self.device, nonce, "GET", "/client-device/catalog.json", "", "0.2.3", "2026-11-05.3", self.sign(nonce))

    def test_challenge_capacity_and_device_identity(self):
        other = self.registry.enroll("second@example.invalid", self.public_of(ec.generate_private_key(ec.SECP256R1())))
        nonce = self.registry.challenge(self.device)
        with self.assertRaises(DeviceRejected):
            self.verify(nonce, self.sign(nonce), device_id=other)
        for _ in range(7):
            self.registry.challenge(self.device)
        with self.assertRaises(DeviceCapacity):
            self.registry.challenge(self.device)
        self.assertTrue(self.registry.challenge(other))

    def test_revocation_remains_after_restart(self):
        nonce = self.registry.challenge(self.device)
        self.registry.revoke(self.device)
        with self.assertRaises(DeviceRejected):
            self.verify(nonce, self.sign(nonce))
        with self.assertRaises(DeviceRejected):
            DeviceRegistry(self.root, clock=lambda: self.now).challenge(self.device)

    def test_retention_and_deleted_email(self):
        self.now += 100
        self.registry.touch(self.device)
        self.now += RETENTION - 1
        self.assertTrue(self.registry.challenge(self.device))
        self.now += 1
        with self.assertRaises(DeviceRejected):
            self.registry.challenge(self.device)
        self.assertEqual(self.registry.maintain({"person@example.invalid"}), 1)
        self.device = self.registry.enroll("person@example.invalid", self.public)
        self.assertEqual(self.registry.maintain(set()), 1)
        with self.assertRaises(DeviceRejected):
            self.registry.challenge(self.device)

    def test_invalid_keys_inputs_and_corrupt_storage_fail_closed(self):
        for key in ("", self.public[:-1], "p256:" + base64.b64encode(bytes(64)).decode("ascii")):
            with self.assertRaises(DeviceRejected):
                self.registry.enroll("person@example.invalid", key)
        nonce = self.registry.challenge(self.device)
        for changes in ({"signature": "x" * 88}, {"nonce": "bad"}, {"path": "/client-device/catalog.json?escape=1"}, {"byte_range": "\nInjected: yes"}):
            with self.assertRaises(DeviceRejected):
                self.verify(nonce, self.sign(nonce), **changes)
        self.registry.path.write_text('{"schema":1,"devices":[{}]}', encoding="utf-8")
        with self.assertRaises(DeviceStorageUnavailable):
            self.registry.challenge(self.device)

    def test_registration_bound_and_account_quota_identity(self):
        for _ in range(9):
            self.registry.enroll("person@example.invalid", self.public_of(ec.generate_private_key(ec.SECP256R1())))
        with self.assertRaises(DeviceCapacity):
            self.registry.enroll("person@example.invalid", self.public_of(ec.generate_private_key(ec.SECP256R1())))
        self.assertEqual(account_key("Person@Example.invalid"), account_key("person@example.invalid"))
        self.assertNotEqual(account_key("other@example.invalid"), account_key("person@example.invalid"))

    def test_anonymous_challenge_work_is_rate_bounded(self):
        for _ in range(48):
            with self.assertRaises(DeviceRejected):
                self.registry.challenge("Z" * 32)
        with self.assertRaises(DeviceCapacity):
            self.registry.challenge(self.device)
        self.tick += 1
        self.assertTrue(self.registry.challenge(self.device))

    def test_operator_revocation_from_another_process_is_immediate(self):
        nonce = self.registry.challenge(self.device)
        other = DeviceRegistry(self.root, clock=lambda: self.now)
        self.assertIn("***@", other.summaries()[0]["email"])
        self.assertNotIn("publicKey", other.summaries()[0])
        other.revoke(self.device)
        with self.assertRaises(DeviceRejected):
            self.verify(nonce, self.sign(nonce))

    def test_operator_cli_preview_and_exact_revocation(self):
        self.now = int(time.time())
        self.registry.path.unlink()  # This test's own temporary registry only.
        with mock.patch("server.device_auth.secrets.token_urlsafe", return_value="-" + "A" * 31):
            self.device = self.registry.enroll("person@example.invalid", self.public)
        command = [sys.executable, str(Path(__file__).resolve().parents[1] / "server/device_auth.py"), "--root", str(self.root)]
        result = subprocess.run(command, capture_output=True, text=True, check=True)
        listed = json.loads(result.stdout)
        self.assertEqual(listed[0]["deviceId"], self.device)
        self.assertIn("***@", listed[0]["email"])
        self.assertNotIn(self.public, result.stdout)
        preview = subprocess.run(command + ["--action", "revoke", "--device-id=" + self.device], capture_output=True, text=True, check=True)
        self.assertEqual(json.loads(preview.stdout)["result"], "PREVIEW")
        self.assertTrue(self.registry.challenge(self.device))
        applied = subprocess.run(command + ["--action", "revoke", "--device-id=" + self.device, "--apply"], capture_output=True, text=True, check=True)
        self.assertTrue(json.loads(applied.stdout)["revoked"])
        with self.assertRaises(DeviceRejected):
            self.registry.challenge(self.device)


if __name__ == "__main__":
    unittest.main()
