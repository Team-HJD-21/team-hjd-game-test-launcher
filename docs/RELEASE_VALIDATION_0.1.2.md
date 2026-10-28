# 0.1.2 validation — 2026-10-29 KST

Game versions are now checked automatically and applied when the user selects **Update**. An available update also exposes **Play installed version**. Rolling back does not hide the manual path back to the latest version. Launcher self-updates retain the verified next-launch workflow.

## Passed checks

- **78 core assertions, zero failures**: signed catalog and archive validation, hashes, safe paths, DPAPI, install/idempotency/rollback/uninstall, process protection and authentication guards.
- **Native GUI startup/shutdown** completed normally.
- **11 actual Controller integration assertions** passed against a separate signed localhost HTTPS fixture. Startup detected 1.1.0 while keeping installed 1.0.0, emitted one update notice and created no newer archive or staged installation. The installed-version action launched the old verified executable. Exiting that game and refreshing left 1.0.0 active. Only the explicit Update action downloaded, verified and activated 1.1.0, preserving the previous version. Rollback kept Update available, and another explicit action returned to the latest version. The Controller portion took 20.196 seconds; the complete fixture took 26.67 seconds.
- **Window frame regression** passed in 1.83 seconds: null-result native mouse input, valid-result input, unrelated dispatcher messages, and **10 queued maximize/restore cycles**. Checks compare both Qt geometry and actual Win32 client bounds with the monitor work area and original restored bounds; a visibility flag alone is insufficient.
- **Native app interaction** passed two maximize/restore cycles between **1000×700** and **2560×1392** after the fix, without the earlier crash.
- **Pending-update UI** was reviewed at 100%, 125% and 150%, including the minimum logical window size. Update and Play installed version remained visible, with no observed overlap or clipping. Four layout/help review runs exited normally without QML errors; installed 1.0.0 stayed active throughout. These layout renders used existing Windows fonts in the review process and are separate from native frame interaction evidence.
- **Final update ZIP** passed `LauncherCoreTests --verify-payload`, exercising the Store/ZIP code used by automatic launcher updates.
- **Final archive audit** verified exact receipt coverage and every size/hash for all **1,370 client payload files** in the portable ZIP. Both ZIPs include `Qt6QuickControls2.dll`, the pinned official cloudflared helper, its Apache 2.0 license and release provenance. Neither archive contains private operational settings/catalogs, credentials, signing keys, tokens, game packages, music or Unity build assets. The authorized Hose for Hire logo remains an application resource.
- **Public source audit** reviewed 101 tracked/new paths and raw credential patterns. No private operational file or raw credential match outside synthetic tests was found.
- **Separate operator dashboard checks** passed two focused dashboard tests and seven operator tests. The private HTML/data export and hosting monitor were exercised, with 15-second page refresh and 30-second monitor intervals. The operator opened the generated HTML; automated browser visual inspection of that local file is not credited because the browser tool blocked file URLs.

The four test targets are `core`, `gui_startup_shutdown`, `controller_manual_update` and `gui_window_frame`. The final frame regression was run separately from the earlier successful core/GUI/manual-update invocation; this report does not claim a single combined 4/4 invocation.

## Maximize crash: evidence and repair

The earlier native maximize click reproduced an access violation while the launcher wrote through a missing native-event result pointer. Qt 6.8.3's Windows dispatcher passes a null result for queued generic native messages, and the Windows window procedure deliberately avoids repeating those input messages through the global filter. The launcher now consumes maximize-button input without assuming a writable result, ignores geometry queries when no result is available, and queues the Qt window-state change until the native callback returns. The recorded crash belongs to the launcher's callback handling; this is not described as a defect in Qt itself. The nullable call and input filtering are visible in the pinned upstream sources. [Qt dispatcher source](https://github.com/qt/qtbase/blob/v6.8.3/src/corelib/kernel/qeventdispatcher_win.cpp#L496), [Qt Windows context source](https://github.com/qt/qtbase/blob/v6.8.3/src/plugins/platforms/windows/qwindowscontext.cpp#L937).

Qt documents the Windows native message types and result role, while queued invocation schedules the state change through the event loop. The regression checks both null and non-null result paths and verifies that the window state does not change inside the native callback. [Native event filter documentation](https://doc.qt.io/qt-6.8/qabstractnativeeventfilter.html#nativeEventFilter), [queued invocation documentation](https://doc.qt.io/qt-6.8/qmetaobject.html#invokeMethod).

## Final audited archives

These are the final post-frame-fix artifacts under `.local/release-0.1.2-final/dist`, checked at 03:46 KST. Repackaging requires a new identity check.

| Archive | Bytes | ZIP file entries | SHA-256 |
| --- | ---: | ---: | --- |
| `TeamHJD-GameTestLauncher-Windows-x64.zip` | 51,365,701 | 1,395 | `cc240695bf2ee18a357f3d3610b365fda040614e04e2a1fccc5e9816c9f99544` |
| `TeamHJD-LauncherPayload-Windows-x64.zip` | 47,755,104 | 1,370 | `b5bb93bf4d3ce36db4628cdaf8f5591cade842b79f3560354d607e57399d0637` |

The helper remains official cloudflared 2026.10.5 with SHA-256 `db4e4c413612a0916f3e1679d3caaf376f94911952d163028fb726fb2e0516a9`; its included Apache license is 11,357 bytes.

## Evidence and publication boundary

Private evidence: `.local/manual-update-20261029-033109-903/result.json`, `.local/manual-ui-fontqa-20261029-0333/`, `.local/release-artifact-audit-0.1.2/result.json` and `.local/release-payload-0.1.2-20261029-034543/`. Test sources are `tests/ControllerUpdateTests.cpp` and `tests/WindowFrameTests.cpp`; `tools/verify-manual-update.ps1` creates only synthetic loopback infrastructure. The old `verify-auto-update.ps1` entry point now delegates to the manual-update regression, so it no longer expects automatic game installation.

**Still pending after publication:** the user's existing launcher must discover the published 0.1.2 release, download and verify its actual hosted payload, queue it, and activate it on the next launch. That final user-PC upgrade is not credited as passed by this pre-publication report. The real public API/download/staging pipeline and bootstrap recovery were validated earlier, but do not substitute for observing this particular published upgrade.

The prior live email OTP/catalog/canonical-game installation remains recorded in the 0.1.1 validation. No new live email verification, server change, full Windows reboot, second physical PC, Windows 10 run or Authenticode signature is claimed by these 0.1.2 checks.
