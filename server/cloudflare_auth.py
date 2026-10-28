"""Optional Cloudflare Access verification and private, bounded email audit storage.

Only the operator's fixed issuer supplies keys. Incoming JWT URLs are never used.
Third-party JWT/crypto imports are deliberately lazy so legacy servers stay usable.
"""
from __future__ import annotations

from contextlib import contextmanager
from dataclasses import dataclass
from datetime import datetime, timezone
import hashlib
import importlib
import json
import os
from pathlib import Path
import re
import ssl
import threading
import time
import urllib.error
import urllib.request
import uuid


MAX_TOKEN_BYTES = 16 * 1024
MAX_JWKS_BYTES = 256 * 1024
MAX_AUDIT_BYTES = 8 * 1024 * 1024
MAX_AUDIT_USERS = 10000
MAX_ACTIVE_SESSIONS = 64
JWKS_TTL_SECONDS = 300
JWKS_RETRY_SECONDS = 15
MAX_SESSION_SECONDS = 31 * 24 * 3600
CLOCK_LEEWAY_SECONDS = 30
ISSUER_PATTERN = re.compile(r"https://[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\.cloudflareaccess\.com")
ID_PATTERN = re.compile(r"[A-Za-z0-9_-]{1,128}")


class AccessDependencyError(RuntimeError):
    pass


class AuditUnavailable(RuntimeError):
    pass


@dataclass(frozen=True)
class VerifiedIdentity:
    email: str
    session_hash: str
    expires_at: int


def _not_linked(path: Path) -> None:
    current = path.absolute()
    while True:
        if current.is_symlink() or (hasattr(current, "is_junction") and current.is_junction()):
            raise ValueError("Linked authentication storage is not allowed")
        if current.parent == current:
            return
        current = current.parent


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("Duplicate JSON member")
        result[key] = value
    return result


def _json(raw: bytes):
    return json.loads(raw.decode("utf-8-sig"), object_pairs_hook=_unique_object)


def _read_json(path: Path, maximum: int):
    _not_linked(path)
    with path.open("rb") as stream:
        raw = stream.read(maximum + 1)
    if len(raw) > maximum:
        raise ValueError("Authentication storage exceeds its size limit")
    return _json(raw)


def _email(value) -> str | None:
    if not isinstance(value, str) or not 3 <= len(value) <= 254 or value != value.strip() or value.count("@") != 1:
        return None
    if any(char.isspace() or ord(char) < 32 or ord(char) == 127 or char in '<>()",;' for char in value):
        return None
    local, domain = value.rsplit("@", 1)
    if not 1 <= len(local) <= 64 or local.startswith(".") or local.endswith(".") or ".." in local:
        return None
    try:
        ascii_domain = domain.encode("idna").decode("ascii").lower()
    except UnicodeError:
        return None
    labels = ascii_domain.split(".")
    if len(labels) < 2 or len(ascii_domain) > 253 or any(not re.fullmatch(r"[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?", label) for label in labels):
        return None
    return local + "@" + ascii_domain


class _NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, message, headers, new_url):
        return None


def fetch_jwks(url: str) -> bytes:
    """The URL is constructed solely from a validated operator issuer."""
    opener = urllib.request.build_opener(_NoRedirect(), urllib.request.HTTPSHandler(context=ssl.create_default_context()))
    request = urllib.request.Request(url, headers={"Accept": "application/json", "User-Agent": "TeamHJD-Distribution/1"})
    with opener.open(request, timeout=5) as response:
        if response.status != 200 or response.geturl() != url:
            raise ValueError("Unexpected Access key response")
        raw = response.read(MAX_JWKS_BYTES + 1)
    if len(raw) > MAX_JWKS_BYTES:
        raise ValueError("Access key response is too large")
    return raw


@contextmanager
def _file_lock(path: Path):
    _not_linked(path)
    stream = path.open("a+b")
    locked = False
    try:
        if stream.tell() == 0:
            stream.write(b"\0")
            stream.flush()
        stream.seek(0)
        if os.name == "nt":
            import msvcrt
            msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
        else:
            import fcntl
            fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
        locked = True
        yield
    finally:
        if locked:
            stream.seek(0)
            if os.name == "nt":
                import msvcrt
                msvcrt.locking(stream.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                import fcntl
                fcntl.flock(stream, fcntl.LOCK_UN)
        stream.close()


def _atomic_json(path: Path, value):
    _not_linked(path)
    raw = json.dumps(value, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    if len(raw) > MAX_AUDIT_BYTES:
        raise ValueError("Verified-user storage exceeds its size limit")
    temporary = path.parent / (".verified-users-" + uuid.uuid4().hex + ".tmp")
    try:
        fd = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(fd, "wb") as stream:
            stream.write(raw)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if temporary.exists():
            temporary.unlink()


class CloudflareAccess:
    def __init__(self, root: Path, *, key_fetcher=fetch_jwks, monotonic=time.monotonic):
        self.root = root
        self.key_fetcher = key_fetcher
        self.monotonic = monotonic
        self._jwt = None
        self._keys = {}
        self._retry_after = {}
        self._key_lock = threading.Lock()
        self._audit_lock = threading.Lock()
        if self._configuration():
            self._dependencies()

    def _configuration(self):
        path = self.root / "cloudflare-access.json"
        if not path.exists():
            return None
        try:
            value = _read_json(path, 16 * 1024)
            if not isinstance(value, dict) or type(value.get("enabled")) is not bool:
                return None
            if not value["enabled"]:
                return None
            issuer, audience = value.get("issuer"), value.get("audience")
            if not isinstance(issuer, str) or not ISSUER_PATTERN.fullmatch(issuer):
                return None
            if not isinstance(audience, str) or not re.fullmatch(r"[A-Za-z0-9_-]{16,256}", audience):
                return None
            return issuer, audience
        except (OSError, ValueError, TypeError, UnicodeError, RecursionError):
            return None

    def _dependencies(self):
        if self._jwt is None:
            try:
                self._jwt = importlib.import_module("jwt")
                importlib.import_module("cryptography.hazmat.primitives.asymmetric.rsa")
            except ImportError as error:
                self._jwt = None
                raise AccessDependencyError("Cloudflare Access is enabled but JWT dependencies are missing. Use the project .deps/server-venv Python and server/requirements.txt.") from error
        return self._jwt

    def _parse_keys(self, raw: bytes):
        if len(raw) > MAX_JWKS_BYTES:
            raise ValueError("Access key response is too large")
        data = _json(raw)
        keys = data.get("keys") if isinstance(data, dict) else None
        if not isinstance(keys, list) or not 1 <= len(keys) <= 16:
            raise ValueError("Invalid Access key set")
        result = {}
        jwt = self._dependencies()
        for item in keys:
            if not isinstance(item, dict):
                raise ValueError("Invalid Access key")
            kid = item.get("kid")
            if not isinstance(kid, str) or not ID_PATTERN.fullmatch(kid) or kid in result:
                raise ValueError("Invalid or duplicate Access key ID")
            if item.get("kty") != "RSA" or item.get("alg") != "RS256" or item.get("use") != "sig":
                raise ValueError("Unsupported Access key")
            if any(not isinstance(item.get(name), str) or not re.fullmatch(r"[A-Za-z0-9_-]+", item[name]) for name in ("n", "e")):
                raise ValueError("Invalid RSA key encoding")
            if len(item["n"]) > 1400 or len(item["e"]) > 8:
                raise ValueError("RSA key exceeds its bounds")
            public = jwt.algorithms.RSAAlgorithm.from_jwk(json.dumps(item))
            if not 2048 <= public.key_size <= 8192 or not 3 <= public.public_numbers().e <= 2**32 - 1:
                raise ValueError("RSA key size is outside its bounds")
            result[kid] = public
        return result

    def _key(self, issuer: str, kid: str):
        with self._key_lock:
            now = self.monotonic()
            cached = self._keys.get(issuer)
            if cached and cached[0] > now and kid in cached[1]:
                return cached[1][kid]
            if self._retry_after.get(issuer, 0) > now:
                return None
            self._retry_after[issuer] = now + JWKS_RETRY_SECONDS
            try:
                keys = self._parse_keys(self.key_fetcher(issuer + "/cdn-cgi/access/certs"))
            except (OSError, ValueError, TypeError, UnicodeError, RecursionError, AccessDependencyError):
                return None
            if len(self._keys) >= 4 and issuer not in self._keys:
                self._keys.clear()
                self._retry_after = {issuer: now + JWKS_RETRY_SECONDS}
            self._keys[issuer] = (now + JWKS_TTL_SECONDS, keys)
            return keys.get(kid)

    def verify(self, token: str) -> VerifiedIdentity | None:
        config = self._configuration()
        if not config or not isinstance(token, str) or not 1 <= len(token) <= MAX_TOKEN_BYTES:
            return None
        try:
            parts = token.split(".")
            if len(parts) != 3 or any(not re.fullmatch(r"[A-Za-z0-9_-]+", part) for part in parts):
                return None
            jwt = self._dependencies()
            header = jwt.get_unverified_header(token)
            if header.get("alg") != "RS256" or header.get("typ", "JWT") != "JWT" or header.get("crit"):
                return None
            kid = header.get("kid")
            if not isinstance(kid, str) or not ID_PATTERN.fullmatch(kid):
                return None
            issuer, audience = config
            key = self._key(issuer, kid)
            if key is None:
                return None
            claims = jwt.decode(token, key, algorithms=["RS256"], audience=audience, issuer=issuer, leeway=CLOCK_LEEWAY_SECONDS,
                                options={"require": ["iss", "aud", "exp", "iat", "nbf", "email", "sub", "type"]})
            if claims.get("type") != "app" or not isinstance(claims.get("sub"), str) or not 1 <= len(claims["sub"]) <= 256:
                return None
            if claims.get("aud") not in (audience, [audience]):
                return None
            if "email_verified" in claims and claims["email_verified"] is not True:
                return None
            email = _email(claims.get("email"))
            if email is None or any(type(claims.get(name)) is not int for name in ("exp", "iat", "nbf")):
                return None
            now = time.time()
            if (not 0 < claims["iat"] <= now + CLOCK_LEEWAY_SECONDS or not 0 < claims["nbf"] <= now + CLOCK_LEEWAY_SECONDS
                    or claims["exp"] <= now - CLOCK_LEEWAY_SECONDS or claims["exp"] <= max(claims["iat"], claims["nbf"])
                    or claims["exp"] - claims["iat"] > MAX_SESSION_SECONDS):
                return None
            return VerifiedIdentity(email, hashlib.sha256(token.encode("ascii")).hexdigest(), claims["exp"])
        except (OSError, ValueError, TypeError, UnicodeError, RecursionError, AccessDependencyError):
            return None
        except Exception:
            # Optional crypto/key-fetch failures also fail closed, without logging a token.
            return None

    def record_access(self, identity: VerifiedIdentity) -> None:
        try:
            with self._audit_lock, _file_lock(self.root / "verified-users.lock"):
                path = self.root / "verified-users.json"
                data = _read_json(path, MAX_AUDIT_BYTES) if path.exists() else {"schema": 1, "users": []}
                if not isinstance(data, dict) or type(data.get("schema")) is not int or data["schema"] != 1 or not isinstance(data.get("users"), list) or len(data["users"]) > MAX_AUDIT_USERS:
                    raise ValueError("Invalid verified-user registry")
                users = data["users"]
                seen = set()
                selected = None
                for user in users:
                    email = _email(user.get("email")) if isinstance(user, dict) else None
                    if email is None or email.casefold() in seen:
                        raise ValueError("Invalid verified-user entry")
                    seen.add(email.casefold())
                    if email.casefold() == identity.email.casefold():
                        selected = user
                now = time.time()
                stamp = datetime.fromtimestamp(now, timezone.utc).isoformat(timespec="seconds").replace("+00:00", "Z")
                if selected is None:
                    if len(users) >= MAX_AUDIT_USERS:
                        raise ValueError("Verified-user limit reached")
                    selected = {"email": identity.email, "firstVerifiedAt": stamp, "lastVerifiedAt": stamp, "lastAccessAt": stamp, "loginCount": 0, "lastSessionHash": "", "source": "cloudflare-access-otp", "activeSessions": []}
                    users.append(selected)
                sessions = selected.get("activeSessions", [])
                if not isinstance(sessions, list) or len(sessions) > MAX_ACTIVE_SESSIONS or type(selected.get("loginCount")) is not int or selected["loginCount"] < 0:
                    raise ValueError("Invalid verified-user sessions")
                active = []
                for session in sessions:
                    if not isinstance(session, dict) or not isinstance(session.get("hash"), str) or not re.fullmatch(r"[0-9a-f]{64}", session["hash"]) or type(session.get("expiresAt")) is not int:
                        raise ValueError("Invalid verified session")
                    if session["expiresAt"] > now - CLOCK_LEEWAY_SECONDS:
                        active.append(session)
                # Persist active hashes so A -> B -> A and a process restart do not inflate counts.
                if not any(session["hash"] == identity.session_hash for session in active):
                    if len(active) >= MAX_ACTIVE_SESSIONS:
                        raise ValueError("Active verified-session limit reached")
                    selected["loginCount"] += 1
                    selected["lastVerifiedAt"] = stamp
                    active.append({"hash": identity.session_hash, "expiresAt": identity.expires_at})
                selected["lastAccessAt"] = stamp
                selected["lastSessionHash"] = identity.session_hash
                selected["activeSessions"] = active
                _atomic_json(path, data)
        except (OSError, ValueError, TypeError, UnicodeError, RecursionError, OverflowError) as error:
            raise AuditUnavailable("Verified-user storage is unavailable") from error
