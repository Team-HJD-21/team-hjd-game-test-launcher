# 0.1.0 validation — 2026-10-29 KST

Historical evidence for 0.1.0. Its server/deployment status describes that release's checks. For current maintenance use the [release runbook](RELEASING.ko.md), [hosting guide](PLAYTEST_HOSTING.ko.md) and [latest recorded 0.1.3 acceptance](RELEASE_VALIDATION_0.1.3.md).

Environment: Windows 11 build 26200, PowerShell 7.6.5, MSVC 19.38.33145, CMake 3.31.6, Qt 6.8.3 MSVC x64, Python 3.13.3. This is a Windows 11 validation; Windows 10 and a second physical PC were not separately tested.

## Passed

- **63 core assertions**, including signed catalog verification/tampering/expiry/rollback, origin restrictions, DPAPI, safe Windows/ZIP paths and file coverage, SHA-256 failures, cancellation preserving the current pointer, installation/update/rollback, same-package no-op, running-process protection, uninstall/save preservation/reinstall, explicit repair and embedded brand icons in both EXEs.
- **Native GUI startup/shutdown**: CTest process exit 0. An actual native-frame teardown error (`0xc000041d`) was found in pre-release review. Caching the returned HWND and clearing it on `WM_NCDESTROY`, rather than calling `winId()` inside native callbacks, removed the observed crash. No symbol-level stack diagnosis of Windows internals is claimed. This check now runs with every build.
- **Seven final portable UI reviews**, all process exit 0 and no QML binding errors: home at 100%, minimum-size settings at 125%, help at 150%, toast, notification history, version information and uninstall confirmation. Actual app PNGs were visually inspected. The portable app uses its own deployed Qt/QML paths and app-local VC++ DLLs.
- **HTTPS end to end**: generated localhost CA, request-local client trust, signed publisher catalog, initial synthetic install, validated range resume, real process launch, running-game update block, 1.0.0 → 1.1.0 update and previous-version retention. A missing/untrusted CA was rejected.
- **Canonical private game package**: the current `Builds/latest.json` selected BUILD0023, 0.1.0-alpha.23; the original archive was reused byte for byte. Private HTTPS download/resume, full ZIP extraction and all **320 file size/hash records** passed. Unity source/build files were read only; the game was not launched by this validation. No game assets were uploaded publicly.
- **Bootstrap process tests**: successful payload activation, failed-start rollback, interrupted activation recovery and queued activation on the next launch.
- **Six HTTP server tests**: authentication, inaccessible private routes, full/range downloads, invalid ranges, HEAD and bad-password throttling.
- **Complete release payload**: extracted and verified by the same Store/ZIP code used for automatic client updates.

Evidence scripts: `build-windows.ps1`, `verify-local.ps1`, `verify-bootstrap.ps1`, `verify-ui.ps1`, `LauncherCoreTests --verify-payload`. Raw logs, synthetic credentials and screenshots stay under ignored `.local/` folders, rather than publishing operational data.

## Deployment boundary

The public release contains only the portable launcher, automatic-update payload and checksums. The private game distribution server has not been exposed to the Internet. Production TLS, team invite/password, TCP forwarding/firewall and an off-LAN download check remain operator deployment steps described in DISTRIBUTION.ko.md. No router, firewall, global certificate store or autostart service was changed. Authenticode signing is not configured.

The supplied TeamHJD PNG is preserved in the UI and converted proportionally into a 16–256px multi-resolution ICO embedded in both launch executables. No logo regeneration or game/music artwork is part of the public bundle.
