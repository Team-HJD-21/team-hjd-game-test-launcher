#include "LauncherUpdate.h"
#include "Platform.h"
#include <QJsonDocument>
#include <QRegularExpression>
#include <QVersionNumber>
#include <cmath>

namespace hjd::updates {
namespace {
qint64 number(const QJsonObject &o,const char *name,qint64 max){
    const auto v=o.value(QLatin1String(name));const double d=v.toDouble(-1);
    if(!v.isDouble()||d<1||d>double(max)||std::floor(d)!=d)fail(QStringLiteral("런처 배포 정보의 순서 또는 용량이 올바르지 않습니다."));
    return qint64(d);
}
QDateTime time(const QJsonObject &o,const char *name){
    const auto text=o[QLatin1String(name)].toString();
    auto value=QDateTime::fromString(text,Qt::ISODateWithMs);
    if(!text.endsWith('Z')||!value.isValid())fail(QStringLiteral("런처 배포 시각이 올바르지 않습니다."));
    return value.toUTC();
}
QJsonObject artifact(const QJsonValue &value,const QString &version,const QString &expected){
    const auto o=value.toObject();
    static const QRegularExpression hash("^[a-f0-9]{64}$");
    if(o["name"].toString()!=expected||!assetUrl(QUrl(o["url"].toString()),version,expected)||
       !hash.match(o["sha256"].toString()).hasMatch())fail(QStringLiteral("이 운영체제의 공식 런처 파일을 확인하지 못했습니다."));
    number(o,"bytes",2ll*1024*1024*1024-1);return o;
}
}
bool releaseVersion(const QString &version){
    static const QRegularExpression pattern("^(0|[1-9][0-9]{0,3})\\.(0|[1-9][0-9]{0,3})\\.(0|[1-9][0-9]{0,3})$");
    return pattern.match(version).hasMatch();
}
bool assetUrl(const QUrl &url,const QString &version,const QString &name){
    return url.isValid()&&url.scheme()=="https"&&url.host()=="github.com"&&url.userInfo().isEmpty()&&
        url.query().isEmpty()&&url.fragment().isEmpty()&&(url.port(-1)==-1||url.port()==443)&&
        url.path()==QStringLiteral("/Team-HJD-21/team-hjd-game-test-launcher/releases/download/v%1/%2").arg(version,name);
}
Release verifyRelease(const QByteArray &bytes,const QString &key,qint64 minimum,const QDateTime &now){
    if(bytes.size()>256*1024||!now.isValid())fail(QStringLiteral("런처 배포 정보의 크기 또는 시스템 시각을 확인하세요."));
    QJsonParseError error;const auto document=QJsonDocument::fromJson(bytes,&error);
    if(error.error!=QJsonParseError::NoError||!document.isObject())fail(QStringLiteral("런처 버전 정보를 읽지 못했습니다."));
    const auto envelope=document.object();
    const auto data=QByteArray::fromBase64Encoding(envelope["payload"].toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
    const auto signature=QByteArray::fromBase64Encoding(envelope["signature"].toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
    if(envelope["keyId"].toString()!=QLatin1String(keyId)||!data||!signature||
       !verifySignature(data.decoded,signature.decoded,key))fail(QStringLiteral("런처 업데이트 서명이 올바르지 않습니다. 공식 설치 파일로 복구해 주세요."));
    const auto payload=QJsonDocument::fromJson(data.decoded,&error);
    if(error.error!=QJsonParseError::NoError||!payload.isObject())fail(QStringLiteral("서명된 런처 버전 정보를 읽지 못했습니다."));
    const auto o=payload.object();Release release;
    if(o["schema"].toInt()!=1)fail(QStringLiteral("지원하지 않는 런처 업데이트 형식입니다."));
    release.sequence=number(o,"sequence",9007199254740991ll);
    if(release.sequence<minimum)fail(QStringLiteral("이전 런처 배포 정보로 되돌릴 수 없습니다."));
    release.version=o["version"].toString();release.minimumVersion=o["minimumVersion"].toString();
    if(!releaseVersion(release.version)||!releaseVersion(release.minimumVersion)||
       QVersionNumber::fromString(release.minimumVersion)>QVersionNumber::fromString(release.version))fail(QStringLiteral("런처 필수 버전 정보가 올바르지 않습니다."));
    release.published=time(o,"publishedAt");release.expires=time(o,"expiresAt");
    if(release.published>now.addSecs(300)||release.expires<=now||release.expires<=release.published||
       release.published.daysTo(release.expires)>366)fail(QStringLiteral("런처 버전 정보가 만료되었거나 시스템 시각이 맞지 않습니다."));
    const auto platforms=o["platforms"].toObject();
    for(const auto &platform:QStringList{"windows-x64","macos-arm64","macos-x64"}){
        auto item=platforms[platform].toObject();
        const auto suffix=platform=="windows-x64"?QStringLiteral("Windows-x64"):QStringLiteral("macOS-")+platform.mid(6);
        const auto payloadName=QStringLiteral("TeamHJD-LauncherPayload-%1.zip").arg(suffix);
        const auto installerName=QStringLiteral("TeamHJD-GameTestLauncher-%1%2").arg(suffix,platform=="windows-x64"?"-Setup.exe":".dmg");
        const auto artifactPayload=artifact(item["payload"],release.version,payloadName);
        const auto installer=artifact(item["installer"],release.version,installerName);
        if(platform==platformId()){release.payload=artifactPayload;release.installer=installer;}
    }
    if(release.payload.isEmpty())fail(QStringLiteral("지원하지 않는 런처 플랫폼입니다."));
    return release;
}
}
