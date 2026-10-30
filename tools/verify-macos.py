#!/usr/bin/env python3
"""Audit deployed macOS artifacts before public release. No credentials needed."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import plistlib
import re
import stat
import subprocess
import sys
import zipfile


PRIVATE_NAMES = {
    "auth.json", "settings.json", "catalog.json", "invite.json", "verified-users.json",
    "publisher-private.json", "tunnel-token.txt", "queued-launcher.json", "pending-launcher.json",
}
PRIVATE_SUFFIXES = {".key", ".pfx", ".pem", ".dpapi", ".hjdinvite", ".dmp", ".log"}
MACHO_MAGICS = {
    b"\xfe\xed\xfa\xce", b"\xce\xfa\xed\xfe", b"\xfe\xed\xfa\xcf", b"\xcf\xfa\xed\xfe",
    b"\xca\xfe\xba\xbe", b"\xbe\xba\xfe\xca", b"\xca\xfe\xba\xbf", b"\xbf\xba\xfe\xca",
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def digest(path: Path) -> str:
    with path.open("rb") as handle:
        return hashlib.file_digest(handle, "sha256").hexdigest()


def public_path(name: str) -> None:
    parts = PurePosixPath(name).parts
    require(bool(parts) and not name.startswith("/") and "\\" not in name, "Non-relative archive path")
    require(name == PurePosixPath(name).as_posix(), "Non-canonical artifact path")
    require(all(part not in {".", "..", ".git", ".local", ".deps", "__MACOSX"} for part in parts), "Private or traversing artifact path")
    require(all(part and ":" not in part and not part.endswith((" ", ".")) for part in parts), "Ambiguous artifact path")
    require(PurePosixPath(name).name.lower() not in PRIVATE_NAMES, "Private state in public artifact")
    require(PurePosixPath(name).suffix.lower() not in PRIVATE_SUFFIXES, "Private credential or log extension in artifact")


def tree_files(root: Path) -> dict[str, dict]:
    result: dict[str, dict] = {}
    for directory, subdirectories, files in os.walk(root, followlinks=False):
        for name in subdirectories + files:
            entry = Path(directory, name)
            mode = entry.lstat().st_mode
            require(not stat.S_ISLNK(mode), "Public bundle contains a symbolic link")
            require(stat.S_ISREG(mode) or stat.S_ISDIR(mode), "Public bundle contains a special file")
            require(not mode & 0o7022, "Public bundle has special or non-owner writable permissions")
            relative = entry.relative_to(root).as_posix()
            public_path(relative)
            if stat.S_ISREG(mode):
                result[relative] = {"bytes": entry.stat().st_size, "sha256": digest(entry), "executableFile": bool(mode & 0o111)}
    require(bool(result), "Empty public bundle")
    return result


def zip_files(path: Path) -> dict[str, dict]:
    result: dict[str, dict] = {}
    seen: set[str] = set()
    with zipfile.ZipFile(path) as archive:
        require(0 < len(archive.infolist()) <= 100_000, "Invalid ZIP entry count")
        for entry in archive.infolist():
            name = entry.filename.rstrip("/")
            public_path(name)
            require(name.casefold() not in seen, "Case-colliding ZIP paths")
            seen.add(name.casefold())
            mode = entry.external_attr >> 16
            require(stat.S_IFMT(mode) in {0, stat.S_IFREG, stat.S_IFDIR}, "ZIP contains link or special file")
            require(not mode & 0o7022, "ZIP preserves special or non-owner writable permissions")
            require(not entry.flag_bits & 1, "Encrypted public ZIP entry")
            if entry.is_dir():
                require(entry.file_size == 0, "Nonempty ZIP directory")
                continue
            require(entry.file_size <= 10 * 1024**3, "Oversized public ZIP entry")
            checksum = hashlib.sha256()
            count = 0
            with archive.open(entry) as handle:
                while chunk := handle.read(1024 * 1024):
                    checksum.update(chunk)
                    count += len(chunk)
            require(count == entry.file_size, "ZIP member size mismatch")
            result[name] = {"bytes": count, "sha256": checksum.hexdigest(), "executableFile": bool(mode & 0o111)}
    require(bool(result), "ZIP contains no files")
    file_names = {name.casefold() for name in result}
    for name in result:
        require(not any(parent.as_posix().casefold() in file_names for parent in PurePosixPath(name).parents if parent.as_posix() != "."),
                "ZIP file also used as directory")
    return result


def receipt_files(seed: Path) -> dict[str, dict]:
    receipt = json.loads((seed / "._teamhjd-receipt.json").read_text(encoding="utf-8"))
    require(receipt["executable"] == "TeamHJDClient.app/Contents/MacOS/TeamHJDClient", "Incorrect macOS client entrypoint")
    require(bool(re.fullmatch(r"[0-9][A-Za-z0-9._-]{0,80}", receipt["version"])), "Invalid seed version")
    expected: dict[str, dict] = {}
    for file in receipt["files"]:
        name = file["path"]
        public_path(name)
        require(name != "._teamhjd-receipt.json" and name not in expected, "Invalid seed receipt path")
        require(isinstance(file.get("executableFile"), bool), "Receipt omits executable permission")
        expected[name] = {"bytes": file["bytes"], "sha256": file["sha256"], "executableFile": file["executableFile"]}
    actual = tree_files(seed)
    actual.pop("._teamhjd-receipt.json")
    require(actual == expected, "Seed file hashes, sizes or executable permissions differ from receipt")
    return actual


def run(*arguments: str) -> str:
    result = subprocess.run(arguments, check=False, text=True, capture_output=True, timeout=60)
    require(result.returncode == 0, f"{Path(arguments[0]).name} failed: {result.stderr.strip()[:300]}")
    return result.stdout


def native_audit(app: Path, arch: str) -> int:
    require(sys.platform == "darwin", "Native macOS code-signature and dependency checks require macOS")
    run("/usr/bin/codesign", "--verify", "--deep", "--strict", str(app))
    all_files = [path for path in app.rglob("*") if path.is_file()]
    names = {path.name for path in all_files}
    count = 0
    for path in all_files:
        with path.open("rb") as handle:
            if handle.read(4) not in MACHO_MAGICS:
                continue
        count += 1
        architectures = run("/usr/bin/lipo", "-archs", str(path)).split()
        require(arch in architectures, "Mach-O file lacks the requested architecture")
        dependencies = run("/usr/bin/otool", "-L", str(path)).splitlines()[1:]
        for line in dependencies:
            dependency = line.strip().split(" (", 1)[0]
            if dependency.startswith(("/System/Library/", "/usr/lib/")):
                continue
            require(dependency.startswith(("@rpath/", "@loader_path/", "@executable_path/")), f"Mach-O {path.relative_to(app)} depends on build-machine library: {dependency}")
            require(PurePosixPath(dependency).name in names, "Bundled Mach-O dependency missing")
            require(".framework/Versions/" not in dependency, "Framework symlink dependency was not flattened")
        load_commands = run("/usr/bin/otool", "-l", str(path)).splitlines()
        for index, line in enumerate(load_commands):
            if line.strip() == "cmd LC_RPATH":
                require(index + 2 < len(load_commands), "Malformed Mach-O LC_RPATH")
                rpath = load_commands[index + 2].strip().removeprefix("path ").split(" (offset ", 1)[0]
                require(rpath.startswith(("@loader_path", "@executable_path", "/System/Library/", "/usr/lib/")), f"Build-machine runtime search path in {path.relative_to(app)}: {rpath}")
    require(count >= 3, "Expected client, bootstrap and helper Mach-O files")
    return count


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", type=Path, required=True)
    parser.add_argument("--payload", type=Path, required=True)
    parser.add_argument("--portable", type=Path)
    parser.add_argument("--arch", choices=("arm64", "x86_64"), required=True)
    parser.add_argument("--structural-only", action="store_true", help="Never use alone as native release acceptance")
    args = parser.parse_args()
    app = args.app.resolve(strict=True)
    require(app.name == "TeamHJDLauncher.app", "Incorrect outer application bundle")
    all_files = tree_files(app)
    info = plistlib.loads((app / "Contents/Info.plist").read_bytes())
    require(info["CFBundleExecutable"] == "TeamHJDLauncher", "Incorrect bootstrap entrypoint")
    require(tuple(map(int, info["LSMinimumSystemVersion"].split("."))) >= (12, 0), "macOS minimum target predates Qt support")
    icon = info.get("CFBundleIconFile", "")
    if icon and not icon.endswith(".icns"):
        icon += ".icns"
    require(bool(icon) and (app / "Contents/Resources" / icon).is_file(), "TeamHJD application icon missing")
    require(all_files["Contents/MacOS/TeamHJDLauncher"]["executableFile"], "Bootstrap executable mode missing")
    seed = app / "Contents/Resources/seed"
    expected = receipt_files(seed)
    require("TeamHJDClient.app/Contents/Helpers/cloudflared" in expected, "Pinned authentication helper missing")
    require(expected["TeamHJDClient.app/Contents/Helpers/cloudflared"]["executableFile"], "Authentication helper executable mode missing")
    require(zip_files(args.payload) == expected, "Update payload differs from verified initial client seed")
    if args.portable:
        zipped = zip_files(args.portable)
        prefix = app.name + "/"
        bundled = {name[len(prefix):]: metadata for name, metadata in zipped.items() if name.startswith(prefix)}
        require(bundled == all_files, "Portable ZIP differs from deployed application")
    macho_count = 0 if args.structural_only else native_audit(app, args.arch)
    print(json.dumps({"result": "PASS", "architecture": args.arch, "appFiles": len(all_files),
                      "payloadFiles": len(expected), "nativeMachOFilesChecked": macho_count,
                      "nativeChecksPerformed": not args.structural_only}, sort_keys=True))


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, OSError, zipfile.BadZipFile, subprocess.TimeoutExpired) as error:
        print(f"FAIL macOS artifact audit: {error}", file=sys.stderr)
        if os.environ.get('GITHUB_ACTIONS') == 'true':
            annotation = str(error).replace('%', '%25').replace('\r', '%0D').replace('\n', '%0A')
            print(f'::error title=macOS artifact audit::{annotation}', flush=True)
        raise SystemExit(1)
