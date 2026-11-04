#pragma once
#include "Core.h"

namespace hjd::updates {
inline constexpr auto feedUrl="https://testlauncher.teamhjd.com/updates/stable.json";
inline constexpr auto fallbackFeedUrl="https://github.com/Team-HJD-21/team-hjd-game-test-launcher/releases/latest/download/launcher-stable.json";
inline constexpr auto keyId="launcher-p256-2026-11";
inline constexpr auto publicKey="p256:c1bEN1h9jmxaT3QdMpSDH8hqNzs5aU+P0gpxpdX3FEXJxpsdv/pfc40r6SCSFkmK0IEoAo0YkfZ6iKYi9brnYQ==";
struct Release {
    QString version,minimumVersion;
    qint64 sequence=0;
    QDateTime published,expires;
    QJsonObject payload,installer;
};
bool releaseVersion(const QString &version);
bool assetUrl(const QUrl &url,const QString &version,const QString &name);
Release verifyRelease(const QByteArray &envelope,const QString &key=QString::fromLatin1(publicKey),
                      qint64 minimumSequence=0,const QDateTime &now=QDateTime::currentDateTimeUtc());
}
