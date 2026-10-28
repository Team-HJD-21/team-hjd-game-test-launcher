# 0.1.1 validation — 2026-10-29 KST

Environment: Windows 11 build 26200, PowerShell 7.6.5, MSVC 19.38, Qt 6.8.3 MSVC x64 and Python 3.13. Tests use separate profiles and managed installation roots under ignored `.local/` storage. No Unity source/build was changed by these checks.

## Passed before publication

- **78 C++ core assertions, zero failures**, plus the native GUI startup/shutdown CTest: **2/2 tests passed**. Coverage includes signed catalogs, tampering/expiry/rollback rejection, SHA-256, exact archive file coverage, safe Windows paths, DPAPI, same-package no-op, installation/rollback/uninstall, running-process protection, embedded icons, personal invite validation and Cloudflare response claim guards. The client claim guard is not a JWT signature authority; the origin performs cryptographic authorization.
- **36 Python server/operator tests passed**. These exercise legacy and individual expiring/revocable credentials, real HTTP routes/ranges, synthetic RSA/JWKS signature/issuer/audience/time/email checks, fail-closed authentication, bounded private verification records, session deduplication and operator/catalog lifecycle behavior. The final operator-only rerun also passed **7/7**. Synthetic JWT tests do not stand in for a real email delivery/login test.
- **Real Controller automatic game updates**: a signed localhost HTTPS fixture was installed at 1.0.0. Starting the actual app automatically discovered, downloaded, verified and activated 1.1.0. While the app remained open, only the server publication changed to 1.2.0; the app automatically installed it after **149.96 seconds**, with no manual refresh or install call. All installed file sizes/hashes passed and 1.1.0 remained available as the previous version. The app exited normally without QML errors.
- **Real Controller automatic launcher update**: the current app used the unchanged production GitHub release API, TLS/redirect policy and published v0.1.0 payload. A guarded test option supplied only an older comparison version, 0.0.9; the displayed application version stayed 0.1.1. The updater automatically downloaded **27,288,314 bytes**, matched the API SHA-256 and verified all **1,367 payload files**, then wrote the normal next-launch queue after **7.67 seconds**. No API response was replaced and no manual update check was invoked. Four invalid test-option combinations were rejected with exit 64.
- **Actual UI captures** were inspected for automatic connection/update notices, the detected launcher update and the settled game installation. The completed game review showed installed 1.2.0 and an available Play action after the isolated server stopped. Menu/disabled-item/checkbox/logo design reviews also cover 100%, 125% and 150% scaling. The original authorized Hose for Hire logo is preserved in the app resource.
- **Final packaged update payload** passed `LauncherCoreTests --verify-payload`, using the same Store/ZIP implementation as automatic updates.
- **Both final ZIPs** were audited for operational credentials, private catalogs/settings, private keys, tokens, game packages, music and Unity build assets; no forbidden entry was found. The portable client's receipt exactly covered all **1,370 payload files**, and every listed size/hash matched the ZIP contents. The authorized game logo is a compiled application resource; no game build is distributed in these archives.
- **Official cloudflared helper**: both archives contain the unmodified pinned 2026.10.5 executable with SHA-256 `db4e4c413612a0916f3e1679d3caaf376f94911952d163028fb726fb2e0516a9`, its **11,357-byte Apache 2.0 license** and release provenance. No operator tunnel credentials are bundled.
- **Public source audit** reviewed 94 tracked/new source paths and raw credential literal patterns. No private operational path, embedded private key, raw JWT, personal invite secret or GitHub token was found.

## Audited artifact identities

These identities apply to the archives checked at 02:16 KST. Repackaging requires rechecking them.

| Archive | Bytes | ZIP file entries | SHA-256 |
| --- | ---: | ---: | --- |
| `TeamHJD-GameTestLauncher-Windows-x64.zip` | 51,364,131 | 1,395 | `6b7ac25c689784de80de91c85fdda165354aba5839e256c5b815e3fb53fde0ca` |
| `TeamHJD-LauncherPayload-Windows-x64.zip` | 47,754,007 | 1,370 | `5bb3efa48a3d8ec9fd745ba317d0bcbf75f8cbfb4076afa2f30388418d03a886` |

## Evidence and live deployment

Reproducible scripts: `tools/build-windows.ps1`, `tools/verify-auto-update.ps1`, `tools/verify-launcher-auto.ps1`, `LauncherCoreTests --verify-payload` and the Python unittest suite. Private evidence remains in `.local/automatic-update-20261029-020234-198/`, `.local/launcher-automatic-update-20261029-021154-711/` and `.local/release-artifact-audit-0.1.1/`; it is not part of the public release.

**Live Cloudflare OTP passed with the operator.** The native client received the browser authentication, the origin validated the signed identity and recorded the verified email/timestamps privately, and the client verified the publisher catalog. The client then downloaded the canonical 149,532,921-byte game archive through playtest.teamhjd.com and installed 0.1.0-alpha.23 after verifying all 320 file records. The completed receipt matches archive SHA-256 556bf87b85ac40c9d77cbf95f4ff5b02048b66e15c38d4fe932f7badd16b71e6. The operator confirmed the flow worked. No OTP, email address or session token is published in this report. The origin listens only on loopback; the temporary TCP 8443 router mapping was removed. Current-user logon autostart and the hosting wrapper were registered and exercised without duplicate processes. A full Windows reboot was not performed.

The existing bootstrap process checks cover healthy activation, failed-start rollback, interrupted activation recovery and queued next-launch activation. They are documented in the earlier validation report; they were not rerun while the operator's live authentication window was open. Windows 10, a second physical PC and Authenticode signing are not credited by this report.

The online game refresh uses a 120-second threshold sampled by a 30-second timer, so an idle running app can take about 2–2.5 minutes to discover a publication. Launcher publication checks run hourly. Hosting requires the documented user session, network and awake PC; email verification must be renewed when its session expires. Catalog renewal is handled by the hosting lifecycle rather than extending an expired signed catalog in the client.
