# TeamHJD Game Test Launcher

Windows x64 and macOS game playtest library, written in C++20 and Qt Quick. Public launcher updates come from this repository's GitHub Releases. Private games are distributed separately through an authenticated HTTPS server and a signed catalog. No game build, music, private log, credential or signing key belongs in this repository or its Releases.

## Maintainers: start here

- [Release runbook and completion checklist (Korean)](docs/RELEASING.ko.md): fresh output directories, native Mac build/reuse rules, draft upload/digest checks, publication and old-client acceptance.
- [Troubleshooting and verified repairs (Korean)](docs/TROUBLESHOOTING.ko.md): known symptoms, minimum diagnostics, evidence and unresolved limits.
- [Hosting and verified-user reports (Korean)](docs/PLAYTEST_HOSTING.ko.md): Tunnel, logon startup/recovery, local dashboard and email/timestamp reports.
- [v0.1.3 acceptance and final artifact identities](docs/RELEASE_VALIDATION_0.1.3.md). Historical validation reports describe their own release, not the current server state.

Read [AGENTS.md](AGENTS.md) before agent-led maintenance. Ignored local evidence is not the only source of the procedure.

## Players

Open **teamhjd.com → footer → Related sites → Game Test Launcher**, or the [launcher download page](https://testlauncher.teamhjd.com/). It links to the latest official Windows and Mac portable ZIPs. See the [site's maintenance contract](docs/PUBLIC_DOWNLOADS.ko.md). The old `playtest` web download pages are retired; its authenticated game API remains in use.

Download **TeamHJD-GameTestLauncher-Windows-x64.zip**, extract it to a writable folder and run **TeamHJDLauncher.exe**. Keep the whole extracted folder together. [GitHub Releases](https://github.com/Team-HJD-21/team-hjd-game-test-launcher/releases/latest) remains the source of the files. Windows 10/11 x64 is the target; the first release was tested on Windows 11.

On **macOS 12 or later**, download **TeamHJD-GameTestLauncher-macOS-arm64.zip** for Apple Silicon or **TeamHJD-GameTestLauncher-macOS-x64.zip** for Intel. Move **TeamHJDLauncher.app** to Applications and run it. See the [Mac player guide](docs/PLAYER_GUIDE_MACOS.ko.md), including the first-launch security approval for this ad hoc signed, unnotarized playtest distribution. The launcher automatically selects a compatible game package and refuses Windows games on macOS.

Select **이메일로 시작** and verify the code sent to your email in the browser, then return to install and play. The default channel uses Cloudflare Access One-time PIN with no email allowlist. Verified email and authentication/access timestamps are kept in private operator storage. Session tokens are protected with Windows DPAPI or a Mac Keychain-held encryption key. Team-issued invitation files remain available for separately configured channels.

Latest game versions are checked automatically. When a new version is available, choose **업데이트** to download and apply it, or **현재 버전 플레이** to keep playing the installed version. Opening the launcher or exiting a game does not start a game update. An identical verified package is reused rather than installed again. The launcher prepares its own update for the next launch. Games remain playable offline. The game menu includes repair, rollback, version/change information and uninstall. AppData saves and external playtest logs are preserved.

The integrated title bar contains File, View, Help and notifications. Shortcuts: `Ctrl+,` settings, `Ctrl+B` sidebar, `F1` help. See the [Korean player guide](docs/PLAYER_GUIDE.ko.md).

## Build and verification

Requirements: Windows, Visual Studio 2022 C++ desktop tools/Windows SDK, PowerShell 7 and Qt **6.8.3 MSVC x64**. Provide Qt using `QT_ROOT_DIR` or `-QtRoot`; no other project is required at runtime.

```powershell
./tools/build-windows.ps1 -QtRoot C:/Qt/6.8.3/msvc2022_64
python tests/test_server.py
./tools/verify-local.ps1 -QtRoot C:/Qt/6.8.3/msvc2022_64
./tools/verify-bootstrap.ps1 -QtRoot C:/Qt/6.8.3/msvc2022_64
$outputRoot = Join-Path $PWD ('.local/releases/' + (Get-Date -Format yyyyMMdd-HHmmss) + '/dist')
./tools/package-windows.ps1 -QtRoot C:/Qt/6.8.3/msvc2022_64 -OutputRoot $outputRoot
```

Cloudflare/JWT and authenticated portal acceptance requires the project server venv and pinned `server/requirements.txt`; see [Cloudflare setup](docs/CLOUDFLARE_ACCESS.ko.md). A crypto-free Python run can skip optional crypto tests and must not be reported as full authentication acceptance.

Build artifacts live under `build/`, release packages should use a fresh `-OutputRoot` under `.local/releases/`, and private test evidence lives under `.local/`. Packaging refuses to overwrite an existing bundle. **Do not move, delete or overwrite the default `dist/` to make packaging succeed:** it may be the operator's live installation and pinned shortcut target. `build-windows.ps1 -Deploy` uses that default, so use the separate packaging command above on an operating host.

Mac native builds use Qt **6.8.3 macOS**, CMake, Ninja and Xcode command-line tools. The `macos-release.yml` workflow builds and tests on Apple Silicon and Intel Mac runners, with a disposable test Keychain. Run `cmake -S . -B build-macos -G Ninja -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/macos -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0`, then `cmake --build build-macos`. `tools/package-macos.py` creates the app and architecture-specific update ZIP, and `tools/verify-macos.py` checks exact bytes, execution permissions, Mach-O dependencies and code signatures. Native tests that exercise Keychain require `TEAMHJD_TEST_KEYCHAIN_READY=1` after preparing a disposable unlocked Keychain. They refuse to use an unprepared personal Keychain.

The Qt DLLs are dynamically linked. Compatible modified libraries can be used by running `launcher/versions/<version>/TeamHJDClient.exe` directly, or by rebuilding this application. The bootstrap's local file receipts protect managed updates; direct client execution permits library replacement and bypasses only this local launcher receipt check. Signed game validation remains active. See [third-party notices](THIRD_PARTY_NOTICES.md) and [Qt license texts](licenses/Qt). Unmodified Qt source is available from the [exact Qt 6.8.3 source distribution](https://download.qt.io/archive/qt/6.8/6.8.3/single/).

## Operators

See [private distribution setup](docs/DISTRIBUTION.ko.md) and [architecture/trust model](docs/ARCHITECTURE.md). `New-Publisher.ps1` stores a current-user DPAPI signing key and a salted password verifier in ignored private storage. `Publish-GameArchive.ps1 -LatestPointer .../Builds/latest.json` validates and reuses the canonical game ZIP byte for byte, then adds a launcher catalog signature. The local game pointer is selection metadata, not an Internet trust signature.

For Cloudflare hosting, automatic recovery/startup, catalog renewal and verified-email reports, see the [Korean hosting guide](docs/PLAYTEST_HOSTING.ko.md). The origin binds to loopback; only the authenticated tunnel is public. Per-user autostart requires Windows logon, an awake PC and Internet access.

The server exposes read-only catalog/package routes. It has no upload, remote execution, game hosting or log collection endpoint. Binding a public interface, certificates, router forwarding and firewall deployment are explicit operator steps; the launcher does not change them.

## Release status

Version 0.1.3 adds native Apple Silicon and Intel launchers and signed per-platform game variants. It retains 0.1.2's explicit game updates, old-version play and maximize crash fix. Operators can open the private local server dashboard with `tools/Open-HostingDashboard.ps1`. No Authenticode or Apple Developer ID certificate is configured. The small stable bootstrap is replaced with a closed portable-bundle update when its own compatibility contract changes; ordinary client/Qt updates use the automatic versioned payload flow. Mac managed versions are stored in the user's Application Support directory so updating does not modify the signed app in Applications. Unselected games are checked when selected. Old versions remain available until game deletion; disk use is shown in version information.
