"""Version/consent compatibility gate. No user identity or request logging."""
from __future__ import annotations
import json
from pathlib import Path
import re

VERSION = re.compile(r"(0|[1-9][0-9]{0,3})\.(0|[1-9][0-9]{0,3})\.(0|[1-9][0-9]{0,3})\Z")
PRIVACY_VERSION = "2026-11-05.1"
DEVICE_PRIVACY_VERSION = "2026-11-05.3"
SUPPORTED_PRIVACY_VERSIONS = {PRIVACY_VERSION, DEVICE_PRIVACY_VERSION}


def privacy_rank(version: str):
    return tuple(map(int, version.replace("-", ".").split(".")))

def check_client(root: Path, headers, *, device_login=False) -> tuple[int | None, str]:
    path = root / "client-policy.json"
    if path.is_symlink() or (hasattr(path, "is_junction") and path.is_junction()):
        raise ValueError("Invalid client policy")
    if not path.exists():
        if not device_login:
            return None, ""  # Operator activates only after the public release/feed.
        policy = {"schema": 1, "minimumVersion": "0.2.3", "privacyVersion": DEVICE_PRIVACY_VERSION}
    else:
        with path.open("rb") as source:
            raw = source.read(4097)
        if len(raw) > 4096:
            raise ValueError("Invalid client policy")
        policy = json.loads(raw.decode("utf-8-sig"))
    if (not isinstance(policy, dict) or type(policy.get("schema")) is not int or policy["schema"] != 1
            or not isinstance(policy.get("minimumVersion"), str) or not VERSION.fullmatch(policy["minimumVersion"])
            or policy.get("privacyVersion") not in SUPPORTED_PRIVACY_VERSIONS):
        raise ValueError("Invalid client policy")
    minimum = tuple(map(int, policy["minimumVersion"].split(".")))
    required_privacy = policy["privacyVersion"]
    if device_login:
        minimum = max(minimum, (0, 2, 3))
        required_privacy = max((required_privacy, DEVICE_PRIVACY_VERSION), key=privacy_rank)
    versions = headers.get_all("X-TeamHJD-Launcher-Version", [])
    if len(versions) != 1 or not VERSION.fullmatch(versions[0]) or tuple(map(int, versions[0].split("."))) < minimum:
        return 426, ""
    consent = headers.get_all("X-TeamHJD-Privacy-Version", [])
    if consent != [required_privacy]:
        return 428, ""
    return None, required_privacy
