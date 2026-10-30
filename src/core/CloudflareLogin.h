#pragma once
#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QUrl>
#include <atomic>
#include <functional>

namespace hjd {
struct CloudflareSession {
    QByteArray jwt;
    QDateTime expires;
};

// Native Access login through the official, hash-pinned helper. Call from a worker
// thread. Status messages contain no token, transfer URL, email or helper output.
class CloudflareLogin {
public:
    static constexpr const char *helperVersion = "2026.10.5";
#if defined(Q_OS_MACOS) && defined(Q_PROCESSOR_ARM_64)
    static constexpr const char *helperSha256 = "2b23964528ddcd641c1449275df8b5b51e3c7193b5257919846331bc5bc13b0f";
#elif defined(Q_OS_MACOS) && defined(Q_PROCESSOR_X86_64)
    static constexpr const char *helperSha256 = "cb09afb9b4d0a4730de477ac710aebb6107568930a8345775629bf264f70c284";
#else
    static constexpr const char *helperSha256 = "db4e4c413612a0916f3e1679d3caaf376f94911952d163028fb726fb2e0516a9";
#endif
#ifdef Q_OS_MACOS
    static constexpr const char *helperName = "cloudflared";
#else
    static constexpr const char *helperName = "cloudflared.exe";
#endif

    static CloudflareSession authenticate(const QUrl &application,
                                         const QString &helperExecutable,
                                         const QString &privateCacheRoot,
                                         const QString &expectedIssuer,
                                         const QString &expectedAudience,
                                         std::atomic_bool &cancel,
                                         const std::function<void(const QString &)> &status = {});

    // Structural and claim guards only. This is NOT a JWT signature verifier and
    // must never authorize a user. The origin must verify signature/issuer/aud.
    static CloudflareSession inspect(const QByteArray &jwt,
                                     const QString &expectedIssuer,
                                     const QString &expectedAudience);
};
}
