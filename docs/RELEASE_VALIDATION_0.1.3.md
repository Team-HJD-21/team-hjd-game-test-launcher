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

The [native CI run](https://github.com/Team-HJD-21/team-hjd-game-test-launcher/actions/runs/41099042374) builds the actual Apple Silicon and Intel binaries using Qt 6.8.3 and the Apple SDK. Acceptance requires native core/Keychain tests, visible GUI startup, complete payload extraction with execute permissions, dependency and signature audit, unpacked GUI capture, first-use bootstrap health, queued activation and failed-health rollback. Public assets are withheld until both platform jobs pass.

Mac seed copies use unique immutable directories so interrupted copying does not prevent the next attempt. Credentials are restored after the first frame acknowledgment so a legitimate Keychain consent prompt does not cause a false bootstrap rollback. Qt frameworks are deployed as ordinary dylibs with rewritten loader paths and app-relative `qt.conf`; symbolic-link rejection in the archive installer is preserved.

## Limits

The launcher is ad hoc signed on macOS, without Apple Developer ID or notarization; Windows has no Authenticode certificate. The Mac guide documents per-app first-launch approval. CI does not exercise a human's Mac email/browser flow, a changed-code Keychain consent prompt, Unity Metal rendering, audio, text input or Windows↔Mac multiplayer. The full authenticated game-download proof was collected on the operator's Windows PC, separately from the native Mac launcher tests. No private game is sent to a public runner to blur that boundary.
