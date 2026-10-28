# TeamHJD Game Test Launcher

Windows x64 game playtest library, written in C++20 and Qt Quick. Public launcher updates come from this repository's GitHub Releases. Private games are distributed separately through an authenticated HTTPS server and a signed catalog. No game build, music, private log, credential or signing key belongs in this repository or its Releases.

## Players

Download **TeamHJD-GameTestLauncher-Windows-x64.zip** from [Releases](https://github.com/Team-HJD-21/team-hjd-game-test-launcher/releases/latest), extract it to a writable folder and run **TeamHJDLauncher.exe**. Keep the whole extracted folder together. Windows 10/11 x64 is the target; the first release was tested on Windows 11.

Select **이메일로 시작** and verify the code sent to your email in the browser, then return to install and play. The default channel uses Cloudflare Access One-time PIN with no email allowlist. Verified email and authentication/access timestamps are kept in private operator storage. Session tokens are protected with Windows DPAPI. Team-issued invitation files remain available for separately configured channels.

Latest game versions are checked automatically. When a new version is available, choose **업데이트** to download and apply it, or **현재 버전 플레이** to keep playing the installed version. Opening the launcher or exiting a game does not start a game update. An identical verified package is reused rather than installed again. The launcher prepares its own update for the next launch. Games remain playable offline. The game menu includes repair, rollback, version/change information and uninstall. AppData saves and external playtest logs are preserved.

The integrated title bar contains File, View, Help and notifications. Shortcuts: `Ctrl+,` settings, `Ctrl+B` sidebar, `F1` help. See the [Korean player guide](docs/PLAYER_GUIDE.ko.md).

## Build and verification

Requirements: Windows, Visual Studio 2022 C++ desktop tools/Windows SDK, PowerShell 7 and Qt **6.8.3 MSVC x64**. Provide Qt using `QT_ROOT_DIR` or `-QtRoot`; no other project is required at runtime.

```powershell
./tools/build-windows.ps1 -QtRoot C:/Qt/6.8.3/msvc2022_64
python tests/test_server.py
./tools/verify-local.ps1 -QtRoot C:/Qt/6.8.3/msvc2022_64
./tools/verify-bootstrap.ps1 -QtRoot C:/Qt/6.8.3/msvc2022_64
./tools/package-windows.ps1 -QtRoot C:/Qt/6.8.3/msvc2022_64
```

Build artifacts live under `build/`, portable and update ZIPs under `dist/`, private test evidence under `.local/`. Packaging refuses to overwrite an existing bundle; preserve/move the old generated output before packaging another build.

The Qt DLLs are dynamically linked. Compatible modified libraries can be used by running `launcher/versions/<version>/TeamHJDClient.exe` directly, or by rebuilding this application. The bootstrap's local file receipts protect managed updates; direct client execution permits library replacement and bypasses only this local launcher receipt check. Signed game validation remains active. See [third-party notices](THIRD_PARTY_NOTICES.md) and [Qt license texts](licenses/Qt). Unmodified Qt source is available from the [exact Qt 6.8.3 source distribution](https://download.qt.io/archive/qt/6.8/6.8.3/single/).

## Operators

See [private distribution setup](docs/DISTRIBUTION.ko.md) and [architecture/trust model](docs/ARCHITECTURE.md). `New-Publisher.ps1` stores a current-user DPAPI signing key and a salted password verifier in ignored private storage. `Publish-GameArchive.ps1 -LatestPointer .../Builds/latest.json` validates and reuses the canonical game ZIP byte for byte, then adds a launcher catalog signature. The local game pointer is selection metadata, not an Internet trust signature.

For Cloudflare hosting, automatic recovery/startup, catalog renewal and verified-email reports, see the [Korean hosting guide](docs/PLAYTEST_HOSTING.ko.md). The origin binds to loopback; only the authenticated tunnel is public. Per-user autostart requires Windows logon, an awake PC and Internet access.

The server exposes read-only catalog/package routes. It has no upload, remote execution, game hosting or log collection endpoint. Binding a public interface, certificates, router forwarding and firewall deployment are explicit operator steps; the launcher does not change them.

## Release status

Version 0.1.2 makes game updates explicit, keeps the installed version playable, and fixes the native maximize-button crash. Operators can open the private local server dashboard with `tools/Open-HostingDashboard.ps1`. See [release verification](docs/RELEASE_VALIDATION_0.1.2.md). No Authenticode certificate is configured. The small stable bootstrap is replaced with a closed portable-bundle update when its own compatibility contract changes; ordinary client/Qt updates use the automatic versioned payload flow. Unselected games are checked when selected. Old versions remain available until game deletion; disk use is shown in version information.
