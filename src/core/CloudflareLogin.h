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
    static constexpr const char *helperSha256 = "db4e4c413612a0916f3e1679d3caaf376f94911952d163028fb726fb2e0516a9";

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
