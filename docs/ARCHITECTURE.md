# TeamHJD Game Test Launcher 0.1

## Scope and technology

Windows 10/11 x64, C++20, Qt 6.8.3 Quick/Controls, CMake/Ninja/MSVC. Qt is dynamically linked. The public repository contains the launcher, read-only Python 3.10+ distribution server, operator scripts, synthetic tests and supplied logos. Game builds, signing keys, credentials and user logs stay in private storage. Unity source is outside this project and is never built or modified here.

The Qt installation is supplied through `QT_ROOT_DIR`; no paths to another project are embedded in the executable. Development on this workstation reuses an existing read-only Qt SDK. An Internet-facing port, firewall rule or autostart service is a separate deployment step.

## Private game update

1. A trusted team channel supplies an invite: exact HTTPS root URL, P-256 publisher public key, optionally an explicit private CA certificate. The access password is separate. The app saves it using current-user DPAPI. Credentials are never placed in URLs or logged.
2. Client fetches `/catalog.json` with bearer authorization containing base64-encoded UTF-8 password **over TLS**. The encoding is not encryption; TLS is mandatory outside explicit loopback tests. Private redirects are rejected.
3. The envelope carries the exact signed payload bytes in base64 and an ECDSA P-256/SHA-256 signature in IEEE P1363 format. Windows CNG verifies against the pinned invite key before parsing game records. The payload includes schema, monotonic sequence, issued/expiry timestamps (seven days by default; maximum eight), per-game archive SHA-256/size, full runtime file SHA-256/sizes and executable.
4. Download to a hash-specific `.part`, with bounded streaming, timeout and validated byte ranges. Interrupted data remains for retry. A full SHA-256 comparison is mandatory after resumed downloads. A changed hash uses a different partial file.
5. Parse and preflight the whole ZIP before extraction. Reject unsafe Windows paths, absolute paths, traversal, ADS, device names, trailing dots/spaces, case-fold collisions, directory/file collisions, links, encryption, unsupported compression, too many files and excessive sizes/ratio. Require exact signed file coverage, size and digest. Never invoke an archive command supplied by a server.
6. Extract to a uniquely named version directory on the same filesystem. The receipt is written only after all files pass. `QSaveFile` replaces `active.json` atomically, recording current and previous versions. Incomplete directories are inert; they are preserved for diagnosis and never activated. Current files are never overwritten.
7. Verify installed files before launching via an absolute executable path with an empty argument list. Track PID, executable path and process creation timestamp, including after a launcher restart. Installation and rollback are blocked while that game runs. A per-game lock serializes activation and launch. Save data stays in the game's AppData/LocalLow location.

Identical verified archive/version/file manifests are a no-op, so an already installed package is not installed twice. Explicit repair creates a new verified replacement. Rollback verifies the previous tree and switches the pointer, holding that problematic release from automatic reapplication. Old versions remain available until game deletion. Uninstall blocks a running game, preflights links, renames only the managed version tree to a scoped removal directory, clears active state and removes that tree. A durable marker permits a failed deletion to resume. External AppData saves and playtest logs remain untouched; known archive cache files are removed. The app does not upload logs.

## Public launcher update

The stable `TeamHJDLauncher.exe` boots `launcher/versions/<version>/TeamHJDClient.exe`. The payload holds Qt DLLs and QML dependencies, allowing client and dependency updates without replacing loaded files.

Client checks the official repository's latest stable GitHub Release using HTTPS, no PAT, at most once per hour after successful checks. Only the fixed payload asset name, exact official repository/tag path and SHA-256 `digest` from GitHub's authenticated HTTPS API are accepted. Public redirects are limited to HTTPS GitHub asset hosts. A bare adjacent checksum downloaded over arbitrary HTTP is never treated as authenticity.

The client automatically stages a separate payload and queues it for the next launch, without interrupting the current session. The bootstrap checks that the current client is closed, writes a durable pending record, activates the candidate and requires a fresh per-launch health acknowledgment after a rendered frame. A failed or interrupted activation reverts to the previous verified payload. Only a newly created unresponsive client child may be terminated; games and unrelated programs are never terminated. Bootstrap itself is a small stable compatibility boundary; upgrading that binary requires replacing the portable bundle while it is closed.

Catalog checks run on startup, selection and app reactivation, every two minutes while connected and every thirty seconds while offline. Automatic checks update bindings without hiding/recreating panels. Installed updates wait for the selected game's process to stop. Notifications report meaningful connection/install/restore changes and remain in the session's notification history. GitHub launcher checks are throttled to one per hour.

## Storage

- Portable installation: bootstrap/dependencies, `launcher/active.json`, immutable `launcher/versions/` and transient health/pending records.
- `%LOCALAPPDATA%/TeamHJD/Launcher/`: settings with DPAPI blob, signed catalog cache, downloads, default games. `--data-root` creates isolated test profiles.
- Per game: `games/<id>/active.json`, `running.json`, `versions/<version-hash-random>/` and receipt.
- Operator: `.local/private-distribution/` by default, containing publisher DPAPI key, salted PBKDF2 password verifier, invite, catalog and private ZIPs. The HTTP handler exposes only catalog and hash-named ZIP routes.

## Trust and limits

The threat model covers hostile/corrupt network responses, malformed archives, interruption, stale signed catalogs, concurrent app copies, game process locks and failed client activation. A malicious local process running as the same Windows user can alter both executable and local receipt; this is not a local malware sandbox. Package authenticity comes from the signed catalog, and launcher authenticity from the fixed GitHub repository over valid TLS. Authenticode signing is not configured; SmartScreen reputation is not claimed.

Certificate validation remains enabled. Private CA trust is request-local and provided by the invite; no global root store is modified and no TLS error is ignored. Changing publisher keys or CA certificates requires a new invite over the trusted team channel. Back up the publisher key safely; DPAPI keys are tied to this Windows account/machine. A missing signing key is not recovered by trusting a new unauthenticated key from the distribution server.

## Primary references

- [Qt request redirect policy](https://doc.qt.io/qt-6.8/qnetworkrequest.html)
- [Qt Windows deployment](https://doc.qt.io/qt-6.8/windows-deployment.html)
- [GitHub release API and asset digest](https://docs.github.com/en/rest/releases/releases)
- [GitHub unauthenticated limits (60/hour per originating IP)](https://docs.github.com/en/rest/using-the-rest-api/rate-limits-for-the-rest-api)
- [Windows DPAPI](https://learn.microsoft.com/en-us/windows/win32/api/dpapi/nf-dpapi-cryptprotectdata)
- [miniz source and license](https://github.com/richgel999/miniz/tree/3.0.2)
