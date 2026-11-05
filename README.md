# TeamHJD Game Test Launcher

Windows per-user Setup and ARM/Intel macOS DMG game playtest launcher, written in C++20 and Qt Quick. Public launcher updates come from this repository's GitHub Releases. Private games are distributed separately through an authenticated HTTPS server and a signed catalog. No game build, music, private log, credential or signing key belongs in this repository or its Releases.

## Maintainers: start here

- [Current installer rollout and team handoff (Korean)](docs/DISTRIBUTION_HANDOFF.ko.md): one-time ZIP migration, player flow, server startup, verified-user records and future releases.
- [v0.2.2 release validation](docs/RELEASE_VALIDATION_0.2.2.md): actual 0.2.0 upgrade, Qt security update, installer preservation, Windows/Mac acceptance and untested boundaries. Earlier reports describe their own releases.
- [Release runbook and completion checklist (Korean)](docs/RELEASING.ko.md): fresh output directories, native Mac build/reuse rules, draft upload/digest checks, publication and old-client acceptance.
- [Troubleshooting and verified repairs (Korean)](docs/TROUBLESHOOTING.ko.md): known symptoms, minimum diagnostics, evidence and unresolved limits.
- [Hosting and verified-user reports (Korean)](docs/PLAYTEST_HOSTING.ko.md): Tunnel, logon startup/recovery, local dashboard and email/timestamp reports.
- [v0.1.3 acceptance and final artifact identities](docs/RELEASE_VALIDATION_0.1.3.md). Historical validation reports describe their own release, not the current server state.

Read [AGENTS.md](AGENTS.md) before agent-led maintenance. Ignored local evidence is not the only source of the procedure.

## Players

Open **teamhjd.com → footer → Related sites → Game Test Launcher**, or the [launcher download page](https://testlauncher.teamhjd.com/). It links to the latest Windows Setup and ARM/Intel Mac DMGs, with portable ZIPs available in the FAQ. See the [site's maintenance contract](docs/PUBLIC_DOWNLOADS.ko.md). The old `playtest` web download pages are retired; its authenticated game API remains in use.

Run **TeamHJD-GameTestLauncher-Windows-x64-Setup.exe** to install for the current user. Open the Start menu shortcut and pin that stable launcher. Existing ZIP users should close the old launcher, install once and replace the old taskbar pin. Games and settings in the same user profile are retained. [GitHub Releases](https://github.com/Team-HJD-21/team-hjd-game-test-launcher/releases/latest) remains the source of the files. Windows 10/11 x64 is the target; the first release was tested on Windows 11.

On **macOS 15 or later**, download **TeamHJD-GameTestLauncher-macOS-arm64.dmg** for Apple Silicon or **TeamHJD-GameTestLauncher-macOS-x64.dmg** for Intel. Move **TeamHJDLauncher.app** to Applications and run it. See the [Mac player guide](docs/PLAYER_GUIDE_MACOS.ko.md), including the first-launch security approval for this ad hoc signed, unnotarized playtest distribution. The launcher automatically selects a compatible game package and refuses Windows games on macOS.

Select **이메일로 시작** and verify the code sent to your email in the browser. After successful verification and saving, the launcher restores its window automatically; the OS may show a taskbar/Dock attention indicator instead of granting focus. The default channel uses Cloudflare Access One-time PIN with no email allowlist. From0.2.3, a per-device signing key keeps the login without retaining a long-lived email token. Its private key is encrypted with current-user Windows DPAPI or Mac Keychain-backed authenticated encryption. Logout, operator revocation, key loss or180 days of inactivity require authentication again. Updates, repair and reinstall preserve the user profile; uninstall alone is not logout. Team-issued invitation files remain available for separately configured channels.

Separate unchecked collection/international-processing choices precede online authentication. Verified email, authentication/access timestamps, bounded session counts, consent records and device registration ID/public key/timestamps are kept privately for up to180 days after last access. See [the privacy notice](legal/PRIVACY.ko.txt). Google login is not used. The server stores no device private key, hardware serial number or device fingerprint.

Latest game versions are checked automatically. When a new version is available, choose **업데이트** to download and apply it, or **현재 버전 플레이** to keep playing the installed version. Opening the launcher or exiting a game does not start a game update. An identical verified package is reused rather than installed again. The launcher verifies a signed static feed independently of email authentication, retries failures, and asks for explicit approval before download, then applies its own update with automatic restart and health rollback. Windows installations use Setup so shortcuts, the uninstaller and Installed Apps version stay in sync. Game updates remain explicit. Game features require connectivity and verified current launcher metadata; offline bypass is not provided. The game menu includes repair, rollback, version/change information and uninstall. AppData saves and external playtest logs are preserved.

The integrated title bar contains File, View, Help and notifications. File → **런처 설치 폴더 열기** opens the launcher location, also shown in Help → About. Routine version checks use the bottom-right **버전 확인중** text with animated dots; actual updates and connection failures retain their blocking flows. Shortcuts: `Ctrl+,` settings, `Ctrl+B` sidebar, `F1` help. See the [Korean player guide](docs/PLAYER_GUIDE.ko.md).

## Build and verification

Install the pinned free SDK with `python -m pip install aqtinstall==3.3.0` and `python tools/install-qt-sdk.py --host windows --output .deps/QtSecurity`. The tool verifies official repository/archive checksums.

Requirements: Windows, Visual Studio 2022 C++ desktop tools/Windows SDK, PowerShell 7 and Qt **6.11.3 MSVC x64**. Provide Qt using `QT_ROOT_DIR` or `-QtRoot`; no other project is required at runtime.

```powershell
./tools/build-windows.ps1 -QtRoot ./.deps/QtSecurity/6.11.3/msvc2022_64
python tests/test_server.py
./tools/verify-local.ps1 -QtRoot ./.deps/QtSecurity/6.11.3/msvc2022_64
./tools/verify-bootstrap.ps1 -QtRoot ./.deps/QtSecurity/6.11.3/msvc2022_64
$outputRoot = Join-Path $PWD ('.local/releases/' + (Get-Date -Format yyyyMMdd-HHmmss) + '/dist')
./tools/package-windows.ps1 -QtRoot ./.deps/QtSecurity/6.11.3/msvc2022_64 -OutputRoot $outputRoot
```

Cloudflare/JWT and authenticated portal acceptance requires the project server venv and pinned `server/requirements.txt`; see [Cloudflare setup](docs/CLOUDFLARE_ACCESS.ko.md). A crypto-free Python run can skip optional crypto tests and must not be reported as full authentication acceptance.

Build artifacts live under `build/`, release packages should use a fresh `-OutputRoot` under `.local/releases/`, and private test evidence lives under `.local/`. Packaging refuses to overwrite an existing bundle. **Do not move, delete or overwrite the default `dist/` to make packaging succeed:** it may be the operator's live installation and pinned shortcut target. `build-windows.ps1 -Deploy` uses that default, so use the separate packaging command above on an operating host.

Mac native builds use Qt **6.11.3 macOS**, CMake, Ninja and Xcode command-line tools. The `macos-release.yml` workflow builds and tests on Apple Silicon and Intel Mac runners, with a disposable test Keychain. Run `cmake -S . -B build-macos -G Ninja -DCMAKE_PREFIX_PATH=/path/to/Qt/6.11.3/macos -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0`, then `cmake --build build-macos`. `tools/package-macos.py` creates the app, DMG and architecture-specific update ZIP, and `tools/verify-macos.py` checks exact bytes, execution permissions, Mach-O dependencies and code signatures. Native tests that exercise Keychain require `TEAMHJD_TEST_KEYCHAIN_READY=1` after preparing a disposable unlocked Keychain. They refuse to use an unprepared personal Keychain.

The Qt DLLs are dynamically linked. Compatible modified libraries can be used by running `launcher/versions/<version>/TeamHJDClient.exe` directly, or by rebuilding this application. The bootstrap's local file receipts protect managed updates; direct client execution permits library replacement and bypasses only this local launcher receipt check. Signed game validation remains active. See [third-party notices](THIRD_PARTY_NOTICES.md) and [Qt license texts](licenses/Qt). Unmodified Qt source is available from the [exact Qt 6.11.3 source distribution](https://download.qt.io/official_releases/qt/6.11/6.11.3/single/).

## Operators

See [private distribution setup](docs/DISTRIBUTION.ko.md) and [architecture/trust model](docs/ARCHITECTURE.md). `New-Publisher.ps1` stores a current-user DPAPI signing key and a salted password verifier in ignored private storage. `Publish-GameArchive.ps1 -LatestPointer .../Builds/latest.json` validates and reuses the canonical game ZIP byte for byte, then adds a launcher catalog signature. The local game pointer is selection metadata, not an Internet trust signature.

For Cloudflare hosting, automatic recovery/startup, catalog renewal and verified-email reports, see the [Korean hosting guide](docs/PLAYTEST_HOSTING.ko.md). The origin binds to loopback; only the authenticated tunnel is public. Per-user autostart requires Windows logon, an awake PC and Internet access.

The private **TeamHJD Game Tester 관리** dashboard provides tester statistics, searchable and paginated records, game catalog details and server controls. Local admin tools share the [design and usability rules](design.md) with the newsletter management tool.

The server exposes read-only catalog/package routes and bounded email-authenticated device enrollment, challenges and self-revocation. It has no file upload, remote execution, game hosting or game-log collection endpoint. The existing loopback Cloudflare Tunnel and local management-service boundaries remain; see [device authentication and operator revocation](docs/DEVICE_LOGIN_PLAN.ko.md).

## Release status

Version 0.2.2 adds an explicit launcher update approval before email authentication, a separate progress modal, connection checks before game launch and retained-process-safe installer handoff. It retains the shared icons and separates update and authentication dialogs. Version 0.1.4 introduced the native launcher icons and separates mandatory launcher-update and email-authentication dialogs. Installed games remain available through explicit offline mode, and receipted installed clients opened directly retain update-root discovery. It retains native Apple Silicon/Intel support, explicit game updates, old-version play and the maximize fix. Operators can open the private local server dashboard with `tools/Open-HostingDashboard.ps1`. No Authenticode or Apple Developer ID certificate is configured. The small stable bootstrap is replaced with a closed portable-bundle update when its own compatibility contract changes; ordinary client/Qt updates use the automatic versioned payload flow. Mac managed versions are stored in the user's Application Support directory so updating does not modify the signed app in Applications. Unselected games are checked when selected. Old versions remain available until game deletion; disk use is shown in version information.
