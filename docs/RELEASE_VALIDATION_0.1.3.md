# 0.1.3 release verification

## Scope

Native macOS 12+ Apple Silicon and Intel clients, TeamHJD app icons, Keychain-protected credentials, architecture-specific launcher payloads and signed game-platform variants. The Windows manual game-update policy, running-game protections and maximize fix remain in place. The original Windows game stays at the top level of catalog schema 1; Mac metadata is nested under `platforms`.

## Local Windows and production checks

- Final Windows CMake build and CTest **4/4 passed**: core, visible GUI startup/shutdown, native frame regression and manual-update Controller integration. Core has 78 assertions; manual-update integration has 11.
- Real bootstrap activation, failed-start rollback, interrupted activation recovery and next-launch queued activation passed.
- Operator tests **8/8 passed**, including preserving the Windows entry when publishing a Mac variant and retaining that variant on later Windows publication.
- Mac artifact-boundary Python tests **3/3 passed**; private dashboard tests **2/2 passed**, including platform rows, escaping, no secrets and CSP.
- The actual published **v0.1.2** Core parser was built from its exact 13 source/vendor blobs at `bdbdb60117fc4d7e90285b05b1a40965cf3a2628`. It accepted production sequence 3, ignored the nested Mac variant and retained Windows Alpha24, 304 files, `HoseForHire.exe` and the canonical Windows package hash. This did not rely on upgrading existing Windows clients first.
- Hosting origin, Cloudflare Tunnel and recovery monitor were restarted and verified running. Public HTTPS returned the expected Access email-authentication redirect. The local dashboard shows both platform packages.
- After the operator reauthenticated, the public catalog signature was verified and the **entire 188,942,717-byte Mac game ZIP** was downloaded through Cloudflare Access. Its SHA256 matched the canonical ZIP: `96722573bdb49aa52ee474f569036bf37cb957fa1e242045e823ba4f05ff1f22`. All 302 canonical ZIP entries were independently checked before private publication. An authenticated Windows package range still matched its unchanged canonical ZIP.
- Both game packages are Alpha24 / BUILD0024 / HFH-FIELD-v14. Private game content, music and credentials are excluded from every public launcher asset and CI artifact.

Local evidence is kept in ignored `.local/legacy-0.1.2-catalog-check/proof.json`, `.local/macos-production-publication-proof.json`, `.local/windows-0.1.3-final-build.txt` and `.local/bootstrap-20261031-022446/result.json`.

## Native Mac acceptance

The [final native packaging and delivery run](https://github.com/Team-HJD-21/team-hjd-game-test-launcher/actions/runs/41107822889) **passed on both Apple Silicon and Intel**. Its binaries come from the successful native build/core/Keychain/GUI jobs in [run 41103462194](https://github.com/Team-HJD-21/team-hjd-game-test-launcher/actions/runs/41103462194), using Qt 6.8.3 and the Apple SDK. The reuse gate verified successful build jobs, same-repository artifact provenance and identical native source trees at `919860320fb3397c466fba0d7310a12cc0f2348e` and `e67c51f0cce0487f70d3fc7ccd8d90e7e568ddd8`; only packaging, verification, workflow and documentation changed between them.

On each architecture the final package audit checked **145 native Mach-O files**, deep/strict code signatures, architecture, bundled dependencies and runtime search paths. The complete **1,642-file** payload matched the initial app seed and survived native extraction with its hashes and execute permissions intact. Native packaged GUI startup/capture, interrupted initial seed-copy recovery, first-frame health acknowledgment, queued payload activation and failed-health rollback all passed. The synthetic failed-health fixture was independently run first and confirmed to exit with code 4 without acknowledging startup; the original rollback assertion was preserved. The earlier fixture had omitted its failure flag and incorrectly reported healthy startup. This was a test-setup error, corrected before final acceptance.

The earlier `faa9431` run passed compilation and core/GUI checks on both Mac architectures, then its ARM package audit rejected a remaining build-machine library reference. Plugin dylib install names and runtime search paths are now relocated along with framework libraries. The bootstrap deploys its own dependencies without importing the client's complete QML tree. These packaging corrections passed the final native acceptance above. Actions status/log APIs were temporarily inconsistent with downloaded completed logs; final acceptance uses completed successful jobs and their actual artifacts, rather than an `in_progress` or empty-step response.

Both native build jobs in run `41103462194` passed and retained their original outputs at source `919860320fb3397c466fba0d7310a12cc0f2348e`. The package audit identified the exact remaining external dependency: optional `sqldrivers/libqsqlodbc.dylib` referenced the SDK producer's `/usr/local/opt/libiodbc/lib/libiodbc.2.dylib`. The launcher uses JSON storage, so non-SQLite optional SQL drivers are now excluded instead of accepting an external dependency. Packaging-only reruns validate successful upstream build jobs, same-repository artifact provenance and identical native source trees before reusing the compiled binaries.

The authenticated [manual download page](https://playtest.teamhjd.com/downloads) is also operational. Production public checks confirmed anonymous redirection to the exact configured Cloudflare issuer, authenticated HTTP200, both canonical Windows/Mac links, HTTP404 for an unsupported platform query, no token in the HTML and restrictive CSP. The page was also visually inspected in the authenticated browser. Server-venv tests are **20/20 passed**; a crypto-free interpreter preserves the legacy 16 tests and explicitly skips the four optional portal crypto tests. The origin process was deliberately replaced with verified ownership; the existing tunnel and monitor were retained. Immediately before release all three recorded PIDs/start times still matched and the public HTTPS gate returned the expected authentication redirect.

Mac seed copies use unique immutable directories so interrupted copying does not prevent the next attempt. Credentials are restored after the first frame acknowledgment so a legitimate Keychain consent prompt does not cause a false bootstrap rollback. Qt frameworks are deployed as ordinary dylibs with rewritten loader paths and app-relative `qt.conf`; symbolic-link rejection in the archive installer is preserved.

## Limits

The launcher is ad hoc signed on macOS, without Apple Developer ID or notarization; Windows has no Authenticode certificate. The Mac guide documents per-app first-launch approval. CI does not exercise a human's Mac email/browser flow, a changed-code Keychain consent prompt, Unity Metal rendering, audio, text input or Windows↔Mac multiplayer. The full authenticated game-download proof was collected on the operator's Windows PC, separately from the native Mac launcher tests. No private game is sent to a public runner to blur that boundary.
