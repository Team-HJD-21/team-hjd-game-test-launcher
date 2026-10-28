# Third-party components

- Qt 6.8.3, dynamically linked (Qt Core, GUI, Network, QML, Quick, Quick Controls, Concurrent, and deployed QML modules). Copyright The Qt Company Ltd. and other contributors. LGPL v3 / applicable module licenses. License texts are included in `licenses/`. Exact upstream source: https://download.qt.io/archive/qt/6.8/6.8.3/single/ . Library replacement is permitted; this launcher does not prohibit reverse engineering for debugging modifications to these libraries. Build instructions are in README.md. No Qt modifications are made.
- miniz 3.0.2, Copyright Rich Geldreich and other contributors, MIT license, reproduced in vendor/miniz/LICENSE. Source: https://github.com/richgel999/miniz/tree/3.0.2 . Vendored source is unmodified except a static-build `miniz_export.h`.
- cloudflared 2026.10.5, Copyright Cloudflare, Inc. and contributors, Apache License 2.0. The unmodified official Windows x64 executable is bundled for browser-based Cloudflare Access email verification. License and pinned release provenance are included in `licenses/cloudflared-LICENSE.txt` and `licenses/cloudflared-provenance.json`. Source and release: https://github.com/cloudflare/cloudflared/tree/2026.10.5 and https://github.com/cloudflare/cloudflared/releases/tag/2026.10.5 . SHA-256: `db4e4c413612a0916f3e1679d3caaf376f94911952d163028fb726fb2e0516a9`.
- Microsoft Visual C++ runtime, redistributed by the official Qt deployment tool under Microsoft's redistribution terms.
- TeamHJD logos are supplied brand assets and remain the property of TeamHJD. They are not an upstream third-party asset or an AI-generated replacement.

No game, music, private package, password, signing key, test logs, or Unity source is part of the public launcher distribution.
