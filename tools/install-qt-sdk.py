#!/usr/bin/env python3
"""Install the fixed, free Qt SDK with aqt's metadata/archive hash checks intact."""
from __future__ import annotations
import argparse
import importlib.metadata
from pathlib import Path
import subprocess
import sys

QT_VERSION = "6.11.3"
AQT_VERSION = "3.3.0"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", choices=("windows", "mac"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    if importlib.metadata.version("aqtinstall") != AQT_VERSION:
        raise ValueError("Install the pinned aqtinstall version in an isolated environment.")
    from aqt.installer import Cli
    from aqt.metadata import QtRepoProperty
    # The official Windows 6.11.3 repository splits metadata by compiler:
    # qt6_6113/qt6_6113_msvc2022_64. aqt3.3 defaults to the absent qt6_6113.
    # Only select that exact official path; all metadata/archive checks remain on.
    original = QtRepoProperty.extension_for_arch
    if args.host == "windows":
        def extension(architecture, is_version_ge_6):
            if architecture == "win64_msvc2022_64" and is_version_ge_6:
                return "msvc2022_64"
            return original(architecture, is_version_ge_6)
        QtRepoProperty.extension_for_arch = staticmethod(extension)
    arch = "win64_msvc2022_64" if args.host == "windows" else "clang_64"
    destination = args.output.resolve()
    command = ["install-qt", args.host, "desktop", QT_VERSION, arch,
               "-O", str(destination), "--archives", "qtbase", "qtdeclarative", "qttools", "qtsvg"]
    if args.dry_run:
        command.append("--dry-run")
    result = Cli().run(command)
    if result:
        return result
    if not args.dry_run:
        suffix = "msvc2022_64" if args.host == "windows" else "macos"
        qmake = destination / QT_VERSION / suffix / "bin" / ("qmake.exe" if args.host == "windows" else "qmake")
        version = subprocess.check_output([str(qmake), "-query", "QT_VERSION"], text=True).strip()
        if version != QT_VERSION:
            raise ValueError("The installed SDK version differs from the pinned security baseline.")
        print("Verified Qt SDK " + version + "; metadata and archives checked by aqt.", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
