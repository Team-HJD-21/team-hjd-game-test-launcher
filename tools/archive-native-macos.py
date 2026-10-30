#!/usr/bin/env python3
"""Transfer only the four native CMake outputs between isolated macOS CI jobs."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import sys
import zipfile

OUTPUTS = ("TeamHJDClient.app", "TeamHJDLauncher.app", "LauncherCoreTests", "LauncherFixtureGame")
ENTRYPOINTS = {
    "TeamHJDClient.app/Contents/MacOS/TeamHJDClient",
    "TeamHJDLauncher.app/Contents/MacOS/TeamHJDLauncher",
    "LauncherCoreTests", "LauncherFixtureGame",
}
REQUIRED = ENTRYPOINTS | {name + "/Contents/Info.plist" for name in OUTPUTS[:2]}
FORBIDDEN_PARTS = {".git", ".local", ".deps", "seed", "Frameworks", "PlugIns", "qml", "__MACOSX"}
FORBIDDEN_NAMES = {
    "auth.json", "settings.json", "catalog.json", "invite.json", "verified-users.json",
    "publisher-private.json", "tunnel-token.txt", "queued-launcher.json", "pending-launcher.json",
}
FORBIDDEN_SUFFIXES = {".key", ".pfx", ".pem", ".dpapi", ".hjdinvite", ".dmp", ".log", ".zip", ".tgz", ".gz"}
MAX_FILES = 2_000
MAX_TOTAL = 2 * 1024**3


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def no_links(path: Path) -> Path:
    absolute = Path(os.path.abspath(path))
    for entry in reversed((absolute, *absolute.parents)):
        try:
            mode = entry.lstat().st_mode
        except FileNotFoundError:
            continue
        require(not stat.S_ISLNK(mode), "Native output path contains a symbolic link")
        require(stat.S_ISREG(mode) or stat.S_ISDIR(mode), "Native output path contains a special file")
    return absolute


def allowed_name(name: str) -> PurePosixPath:
    relative = PurePosixPath(name)
    require(name and name == relative.as_posix() and not relative.is_absolute() and "\\" not in name,
            "Non-canonical native output path")
    require(relative.parts[0] in OUTPUTS, "Archive includes an output outside the four owned targets")
    require(all(part not in {".", ".."} and ":" not in part and not part.endswith((".", " ")) for part in relative.parts),
            "Unsafe native output path")
    require(not {part.casefold() for part in relative.parts} & {part.casefold() for part in FORBIDDEN_PARTS},
            "Raw output includes an SDK, cache or packaged runtime")
    require(relative.name.lower() not in FORBIDDEN_NAMES and relative.suffix.lower() not in FORBIDDEN_SUFFIXES,
            "Raw output includes private state, an archive or credentials")
    if relative.parts[0] in OUTPUTS[2:]:
        require(len(relative.parts) == 1, "Native test target must be a single file")
    else:
        require(len(relative.parts) >= 3 and relative.parts[1] == "Contents", "Invalid raw application bundle path")
    return relative


def validate_files(files: dict[str, tuple[int, int]]) -> None:
    require(REQUIRED <= files.keys(), "One of the four native outputs or bundle Info.plist files is missing")
    require(0 < len(files) <= MAX_FILES, "Invalid native output file count")
    require(sum(size for _, size in files.values()) <= MAX_TOTAL, "Native output expansion exceeds limit")
    folded = {name.casefold() for name in files}
    require(len(folded) == len(files), "Case-colliding native output paths")
    for name, (mode, size) in files.items():
        path = allowed_name(name)
        require(stat.S_IFMT(mode) == stat.S_IFREG and not mode & 0o7022, "Unsafe native output file type or permissions")
        require(0 <= size <= MAX_TOTAL, "Invalid native output file size")
        if name in ENTRYPOINTS:
            require(size > 0 and mode & 0o111, "Native entrypoint is empty or has no executable permission")
        require(not any(parent.as_posix().casefold() in folded for parent in path.parents if parent.as_posix() != "."),
                "Native output file is also used as a directory")


def source_files(build: Path) -> dict[str, tuple[int, int]]:
    result = {}
    for name in OUTPUTS:
        target = build / name
        no_links(target)
        require(target.is_dir() if name.endswith(".app") else target.is_file(), "Missing native build target")
        entries = target.rglob("*") if target.is_dir() else (target,)
        for entry in entries:
            no_links(entry)
            mode = entry.lstat().st_mode
            if stat.S_ISDIR(mode):
                continue
            result[entry.relative_to(build).as_posix()] = (mode, entry.stat().st_size)
    validate_files(result)
    return result


def archive_files(source: zipfile.ZipFile) -> dict[str, tuple[int, int]]:
    entries = source.infolist()
    require(0 < len(entries) <= MAX_FILES, "Invalid intermediate archive entry count")
    result = {}
    for entry in entries:
        require(not entry.is_dir() and not entry.flag_bits & 1 and entry.create_system == 3,
                "Intermediate archive must contain unencrypted Unix regular files only")
        require(entry.compress_type in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED), "Unsupported intermediate archive compression")
        name = entry.filename
        allowed_name(name)
        require(name not in result, "Duplicate intermediate archive path")
        result[name] = (entry.external_attr >> 16, entry.file_size)
    validate_files(result)
    return result


def pack(build: Path, archive: Path) -> dict:
    build = no_links(build)
    archive = no_links(archive)
    require(not archive.exists(), "Select a fresh intermediate archive path")
    files = source_files(build)
    archive.parent.mkdir(parents=True, exist_ok=True)
    no_links(archive.parent)
    with zipfile.ZipFile(archive, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as output:
        for name, (mode, _) in sorted(files.items()):
            info = zipfile.ZipInfo(name)
            info.create_system = 3
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = mode << 16
            no_links(build / name)
            with (build / name).open("rb") as source, output.open(info, "w") as target:
                shutil.copyfileobj(source, target, length=1024 * 1024)
    with zipfile.ZipFile(archive) as result:
        require(archive_files(result) == files, "Intermediate archive modes or sizes differ from native build")
        require(result.testzip() is None, "Intermediate archive CRC check failed")
    return {"result": "PASS", "phase": "pack", "files": len(files), "bytes": sum(size for _, size in files.values()),
            "sha256": file_hash(archive), "ownedTargets": list(OUTPUTS)}


def extract(archive: Path, build: Path) -> dict:
    archive = no_links(archive)
    build = no_links(build)
    require(not build.exists() or build.is_dir() and not any(build.iterdir()), "Native restore destination must be new or empty")
    with zipfile.ZipFile(archive) as source:
        files = archive_files(source)
        require(source.testzip() is None, "Intermediate archive CRC check failed")
        build.mkdir(parents=True, exist_ok=True)
        no_links(build)
        for name, (mode, size) in sorted(files.items()):
            target = build.joinpath(*PurePosixPath(name).parts)
            target.parent.mkdir(parents=True, exist_ok=True)
            no_links(target)
            with source.open(name) as data, target.open("xb") as output:
                shutil.copyfileobj(data, output, length=1024 * 1024)
            require(target.stat().st_size == size, "Restored native file size differs from archive")
            # Preserve exactly the source executable flags, never special bits.
            target.chmod(stat.S_IMODE(mode))
    return {"result": "PASS", "phase": "extract", "files": len(files), "bytes": sum(size for _, size in files.values()),
            "sha256": file_hash(archive), "ownedTargets": list(OUTPUTS)}


def file_hash(path: Path) -> str:
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("phase", choices=("pack", "extract"))
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--archive", type=Path, required=True)
    arguments = parser.parse_args()
    result = (pack(arguments.build, arguments.archive) if arguments.phase == "pack"
              else extract(arguments.archive, arguments.build))
    print(json.dumps(result, sort_keys=True), flush=True)


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, zipfile.BadZipFile) as error:
        print(f"FAIL native output archive: {error}", file=sys.stderr, flush=True)
        raise SystemExit(1)
