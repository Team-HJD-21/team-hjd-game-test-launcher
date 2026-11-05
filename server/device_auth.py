"""Private per-device public-key registry and single-use request challenges.

Enrollment requires an independently verified Cloudflare email identity. This
module never accepts a client-supplied email as proof and stores no private key.
"""
from __future__ import annotations
import base64
from datetime import datetime, timezone
import hashlib
from pathlib import Path
import re
import secrets
import threading
import time

try:
    from .cloudflare_auth import _atomic_json, _email, _file_lock, _not_linked, _read_json
except ImportError:
    from cloudflare_auth import _atomic_json, _email, _file_lock, _not_linked, _read_json

DEVICE_ID = re.compile(r"[A-Za-z0-9_-]{32}\Z")
NONCE = re.compile(r"[A-Za-z0-9_-]{43}\Z")
RETENTION = 180 * 86400
MAX_DEVICES = 10000
MAX_PER_EMAIL = 10
MAX_REGISTRATIONS_PER_EMAIL = 32
MAX_BYTES = 8 * 1024 * 1024
CHALLENGE_SECONDS = 60


class DeviceRejected(ValueError):
    pass


class DeviceCapacity(ValueError):
    pass


class DeviceStorageUnavailable(ValueError):
    pass


class DeviceNonceExpired(DeviceRejected):
    pass


def public_key(value):
    from cryptography.hazmat.primitives.asymmetric import ec
    if not isinstance(value, str) or not value.startswith("p256:") or len(value) != 93:
        raise DeviceRejected("Invalid device public key")
    try:
        raw = base64.b64decode(value[5:], validate=True)
        if len(raw) != 64:
            raise ValueError("size")
        return ec.EllipticCurvePublicKey.from_encoded_point(ec.SECP256R1(), b"\x04" + raw)
    except ValueError:
        raise DeviceRejected("Invalid device public key") from None


def proof_message(nonce, method, path, byte_range, version, consent):
    fields = (nonce, method, path, byte_range, version, consent)
    if (not all(isinstance(v, str) and len(v) <= 512 and all(32 <= ord(c) <= 126 for c in v) for v in fields)
            or not NONCE.fullmatch(nonce) or method not in ("GET", "HEAD", "POST")
            or not path.startswith("/client-device/") or "?" in path or "#" in path):
        raise DeviceRejected("Invalid device proof")
    return ("TeamHJD-Device-v1\n" + "\n".join(fields)).encode("ascii")


class DeviceRegistry:
    def __init__(self, root: Path, *, clock=time.time, monotonic=time.monotonic):
        self.root = root
        self.path = root / "trusted-devices.json"
        self.lock_path = root / "trusted-devices.lock"
        self.clock, self.monotonic = clock, monotonic
        self._lock = threading.Lock()
        self._challenges = {}
        self._challenge_tokens = 48.0
        self._challenge_refill = self.monotonic()

    def _load(self):
        try:
            return self._load_checked()
        except (OSError, ValueError, TypeError, UnicodeError, RecursionError, ImportError) as error:
            raise DeviceStorageUnavailable("Device registry unavailable") from error

    def _load_checked(self):
        _not_linked(self.path)
        data = _read_json(self.path, MAX_BYTES) if self.path.exists() else {"schema": 1, "devices": []}
        if (not isinstance(data, dict) or type(data.get("schema")) is not int or data["schema"] != 1
                or not isinstance(data.get("devices"), list) or len(data["devices"]) > MAX_DEVICES):
            raise DeviceRejected("Invalid device registry")
        seen = set()
        now = self.clock()
        for device in data["devices"]:
            if (not isinstance(device, dict) or not isinstance(device.get("id"), str)
                    or not DEVICE_ID.fullmatch(device["id"]) or device["id"] in seen
                    or _email(device.get("email")) is None
                    or any(type(device.get(k)) is not int for k in ("createdAt", "lastAccessAt", "revokedAt"))
                    or not 0 < device["createdAt"] <= device["lastAccessAt"] <= now + 30
                    or not 0 <= device["revokedAt"] <= now + 30):
                raise DeviceRejected("Invalid device registry")
            public_key(device.get("publicKey"))
            seen.add(device["id"])
        return data

    def _active(self, device):
        return not device["revokedAt"] and self.clock() - device["lastAccessAt"] < RETENTION

    def enroll(self, verified_email: str, key: str):
        email = _email(verified_email)
        if email is None:
            raise DeviceRejected("Verified email required")
        public_key(key)
        with self._lock, _file_lock(self.lock_path):
            data = self._load()
            current = [d for d in data["devices"] if d["email"].casefold() == email.casefold() and self._active(d)]
            for device in current:
                if secrets.compare_digest(device["publicKey"], key):
                    return device["id"]
            history = sum(d["email"].casefold() == email.casefold() for d in data["devices"])
            if len(current) >= MAX_PER_EMAIL or history >= MAX_REGISTRATIONS_PER_EMAIL or len(data["devices"]) >= MAX_DEVICES:
                raise DeviceCapacity("Device registration limit reached")
            now = int(self.clock())
            device_id = secrets.token_urlsafe(24)
            data["devices"].append({"id": device_id, "email": email, "publicKey": key,
                                    "createdAt": now, "lastAccessAt": now, "revokedAt": 0})
            _atomic_json(self.path, data)
            return device_id

    def _lookup(self, device_id):
        if not isinstance(device_id, str) or not DEVICE_ID.fullmatch(device_id):
            raise DeviceRejected("Device authentication required")
        devices = self._load()["devices"]
        found = next((d for d in devices if d["id"] == device_id and self._active(d)), None)
        if found is None:
            raise DeviceRejected("Device authentication required")
        return found

    def challenge(self, device_id):
        with self._lock, _file_lock(self.lock_path):
            now = self.monotonic()
            # Bound anonymous registry/EC work before looking up attacker-chosen
            # IDs. No IP address is collected or used as the shared tunnel key.
            self._challenge_tokens = min(48.0, self._challenge_tokens + max(0.0, now - self._challenge_refill) * 20.0)
            self._challenge_refill = now
            if self._challenge_tokens < 1.0:
                raise DeviceCapacity("Device challenge rate limited")
            self._challenge_tokens -= 1.0
            self._lookup(device_id)
            self._challenges = {n: item for n, item in self._challenges.items() if item[1] > now}
            if len(self._challenges) >= 4096 or sum(item[0] == device_id for item in self._challenges.values()) >= 8:
                raise DeviceCapacity("Too many device challenges")
            nonce = secrets.token_urlsafe(32)
            self._challenges[nonce] = (device_id, now + CHALLENGE_SECONDS)
            return nonce

    def verify(self, device_id, nonce, method, path, byte_range, version, consent, signature):
        from cryptography.exceptions import InvalidSignature
        from cryptography.hazmat.primitives import hashes
        from cryptography.hazmat.primitives.asymmetric import ec, utils
        message = proof_message(nonce, method, path, byte_range, version, consent)
        if not isinstance(signature, str) or len(signature) != 88:
            raise DeviceRejected("Invalid device proof")
        try:
            raw = base64.b64decode(signature, validate=True)
            if len(raw) != 64:
                raise ValueError("size")
            der = utils.encode_dss_signature(int.from_bytes(raw[:32], "big"), int.from_bytes(raw[32:], "big"))
        except ValueError:
            raise DeviceRejected("Invalid device proof") from None
        with self._lock, _file_lock(self.lock_path):
            device = self._lookup(device_id)
            pending = self._challenges.get(nonce)
            if pending is None or pending[1] <= self.monotonic():
                raise DeviceNonceExpired("Device challenge expired or used")
            if pending[0] != device_id:
                raise DeviceRejected("Device challenge belongs to another device")
            try:
                public_key(device["publicKey"]).verify(der, message, ec.ECDSA(hashes.SHA256()))
            except InvalidSignature:
                raise DeviceRejected("Invalid device proof") from None
            del self._challenges[nonce]
            # Return a snapshot. Authorization/audit must not recreate an email
            # record deleted by the operator after initial enrollment.
            return dict(device)

    def touch(self, device_id):
        with self._lock, _file_lock(self.lock_path):
            data = self._load()
            device = next((d for d in data["devices"] if d["id"] == device_id and self._active(d)), None)
            if device is None:
                raise DeviceRejected("Device access revoked")
            device["lastAccessAt"] = max(int(self.clock()), device["lastAccessAt"])
            _atomic_json(self.path, data)

    def revoke(self, device_id):
        with self._lock, _file_lock(self.lock_path):
            data = self._load()
            device = next((d for d in data["devices"] if d["id"] == device_id), None)
            if device is None:
                raise DeviceRejected("Unknown device")
            device["revokedAt"] = max(1, int(self.clock()))
            _atomic_json(self.path, data)
            self._challenges = {n: item for n, item in self._challenges.items() if item[0] != device_id}

    def maintain(self, retained_emails: set[str]):
        with self._lock, _file_lock(self.lock_path):
            data = self._load()
            before = len(data["devices"])
            data["devices"] = [d for d in data["devices"] if self.clock() - d["lastAccessAt"] < RETENTION
                               and d["email"].casefold() in retained_emails]
            if len(data["devices"]) != before:
                _atomic_json(self.path, data)
            return before - len(data["devices"])

    def summaries(self, *, show_emails=False):
        with self._lock, _file_lock(self.lock_path):
            result = []
            for device in self._load()["devices"]:
                email = device["email"]
                if not show_emails:
                    local, domain = email.rsplit("@", 1)
                    email = local[:1] + "***@" + domain
                stamp = lambda value: datetime.fromtimestamp(value, timezone.utc).isoformat(timespec="seconds").replace("+00:00", "Z") if value else ""
                result.append({"deviceId": device["id"], "email": email, "registeredAt": stamp(device["createdAt"]),
                               "lastAccessAt": stamp(device["lastAccessAt"]), "revokedAt": stamp(device["revokedAt"]), "active": self._active(device)})
            return result


def account_key(email):
    return hashlib.sha256(b"access:" + email.casefold().encode("utf-8")).digest()


if __name__ == "__main__":
    import argparse
    import json
    parser = argparse.ArgumentParser(description="Private device registration management. Never expose through the game tunnel.")
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--action", choices=("list", "revoke"), default="list")
    parser.add_argument("--device-id")
    parser.add_argument("--apply", action="store_true")
    parser.add_argument("--show-emails", action="store_true")
    args = parser.parse_args()
    if not args.root.is_absolute() or not args.root.is_dir():
        parser.error("Explicit absolute private root required")
    _not_linked(args.root)
    registry = DeviceRegistry(args.root)
    try:
        if args.action == "list":
            if args.apply or args.device_id:
                parser.error("List is read-only")
            print(json.dumps(registry.summaries(show_emails=args.show_emails), ensure_ascii=True))
        else:
            if not args.device_id or not DEVICE_ID.fullmatch(args.device_id):
                parser.error("Exact device ID required")
            # Validate exact ownership before either preview or mutation.
            if not any(d["deviceId"] == args.device_id for d in registry.summaries()):
                raise DeviceRejected("Unknown device")
            if args.apply:
                registry.revoke(args.device_id)
            print(json.dumps({"result": "APPLIED" if args.apply else "PREVIEW", "deviceId": args.device_id, "revoked": args.apply}))
    except (OSError, ValueError, TypeError, UnicodeError, ImportError):
        raise SystemExit("Device management unavailable; inspect private storage and its lock.") from None
