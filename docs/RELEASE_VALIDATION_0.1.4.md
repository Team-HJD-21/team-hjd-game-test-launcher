# v0.1.4 release verification

2026-11-03 KST. Native source: `08e0470cfe70236bc1a3f15622994599224fe5a4`.

## Changes

The approved TeamHJD launcher rocket/play badge is used by the Windows client/bootstrap executable resources, the running Qt client and both macOS app icons. The 512px PNG is byte-identical to company source `22c596ac6ea86edd5e0f7adee07875c49e0e449a`; ICO/ICNS supply desktop sizes. Original page/header wordmarks remain separate. `assets/brand/app-icon-provenance.json` records the source and generated hashes.

Expired or unavailable saved email credentials are shown as email authentication/reauthentication required, rather than an automatic reconnect. Catalog checks, validated online state and failed connection have separate states. The observed operator issue was an expired saved session while the origin and tunnel were alive; no credential values or personal audit timestamps are included here.

Managed startup checks the public launcher release before email authentication and bypasses the previous one-hour saved-check cache once per launch. Update check/download/verification/ready/error and email authentication have modal flows. Background shortcuts and invitation drops are blocked; browser authentication keeps its dialog open. A verified downloaded payload is queued and becomes active at the next stable launcher execution. The ready dialog requests closing/reopening the launcher. Installed-client direct execution discovers a managed root only with the expected layout, active pointer and matching self-executable receipt/hash. Arbitrary development executables are not adopted.

When authentication is unavailable or update checking fails, an installed game can be selected for offline use. The server is not marked online and game files are still verified. An available launcher update cannot be dismissed into online play. Game updates remain explicit.

## Windows

- Qt6.8.3 MSVC x64, Visual Studio2022, PowerShell7.6.5; CMake/CTest5/5 passed. Connection tests22 assertions include a protected expired session, managed-root recovery/tamper rejection, update-before-auth state and a real installed synthetic game starting/exiting offline. Core78, native frame null-result/10 maximize-restore cycles and manual game-update Controller11 assertions passed.
- Bootstrap activation, failed-health rollback, interrupted recovery and queued next-launch activation passed. Final Windows payload extraction and all file hashes passed.
- Final portable client ran with isolated profiles and its deployed dependencies. Seven capture/scale cases exited0 with no QML binding errors. A forced first-login modal can intentionally cover requested background review surfaces; these captures do not claim those surfaces are usable before authentication.
- Native UI: mandatory authentication survived Escape and Ctrl+,; the explicit offline button removed the modal and exposed offline playback. The real synthetic game generated its owned running receipt. No human email/browser authentication was automated.
- Pre-publication new-client updater check used real public v0.1.3 and only the guarded comparison fixture0.0.9. API/digest/full1370-file payload/queue passed in11.51s; no production API response was replaced. This is distinct from the post-publication v0.1.4 check recorded below after publication.

## macOS

[Full native build and delivery run41583063044](https://github.com/Team-HJD-21/team-hjd-game-test-launcher/actions/runs/41583063044), exact source above, succeeded on both Apple Silicon and Intel. Binaries were rebuilt rather than reused. Native connection/core/GUI checks passed. Each final audit verified145 Mach-O files and1642 payload files with dependencies, strict signatures, permissions and complete native extraction. App files1664. Native seed recovery/frame ACK, queued activation and deliberately failed-health fixture/rollback all passed.

Both original app and seed client ICNS bytes match the reviewed source. The two actual Mac PNG captures were inspected. The ARM capture showed the safe update-error dialog after an anonymous GitHub API rate-limit response; the Intel capture showed update checking. These are native rendering/delivery evidence, not a claim of a successful Mac public-API download. Public update download/digest acceptance is independently exercised on Windows.

An intermediate run41581500188 passed the new connection test but its Mac core test waited at Keychain access and timed out. Different unsigned test binaries had shared the same wrapping item. CI now removes only that item from its explicitly prepared disposable Keychain between the two tests; permissions/assertions were not weakened. The final full native run passed both tests and delivery. Connection tests also refuse an unprepared Mac Keychain.

## Final public files

Six launcher ZIPs and the checksum file only; paths, per-file sizes/hashes, permissions and private-file exclusion passed. No Unity package, BGM, configuration, private keys, user logs or email audit was included. Final Mac delivery artifacts, not native/candidate artifacts, supplied these files.

| Asset | Bytes | SHA256 |
| --- | ---: | --- |
| `TeamHJD-GameTestLauncher-Windows-x64.zip` | 51,618,687 | `3c71830e318d0c252b84f51e56b2e470fde7f5f7485f1144d2f4419ab993e81f` |
| `TeamHJD-LauncherPayload-Windows-x64.zip` | 47,973,152 | `7fef27bff94c4919aa10f34fde6c3caee67885741ae7d381327554b853a5ce1b` |
| `TeamHJD-GameTestLauncher-macOS-arm64.zip` | 61,174,312 | `4f558c0a37ad80f15426444a338d9764ee81d7806ccfdec49ce500902d8477d4` |
| `TeamHJD-LauncherPayload-macOS-arm64.zip` | 49,544,706 | `f65485ff2a7d554baf93425177f25b1715ce0fb2265504bcf854d2d0cb86496a` |
| `TeamHJD-GameTestLauncher-macOS-x64.zip` | 67,210,203 | `689c55b7e544eac0b485cfa75892070ffef0c3260d574e8468a0475e02005c0d` |
| `TeamHJD-LauncherPayload-macOS-x64.zip` | 54,238,146 | `4fbfe06a7a7f4ce7d4102325043a290559ed83716cc0cbd991ec827655dd22be` |
| `SHA256SUMS.txt` | 635 | `63528c651f374703a609597081f86bbbe6136cfc8468f5c08a2c76aaabcee10f` |

## Publication and remaining scope

Draft upload/digest comparison and publication are recorded after the actual GitHub checks. Local proof/logs live in ignored `.local/release-0.1.4-final/`; no signed download URLs or credentials belong in this document.

Windows Authenticode and Apple Developer ID/notarization are absent. Human Mac OTP/changed-code Keychain consent, actual Unity game/cross-play, real reboot/logon and the operator's current installation activation were not verified. The existing actual `dist/` and taskbar target were preserved. Updating the stable EXE/Finder app icon requires the new portable bundle; the automatic payload updates the client icon and UI.
