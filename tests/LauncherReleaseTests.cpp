#include "core/LauncherUpdate.h"
#include "core/Platform.h"
#include "SigningKey.h"
#include <QCoreApplication>
#include <QTextStream>

using namespace hjd;
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);int checks=0;
    auto check=[&](bool ok,const char *name){if(!ok)fail(QString::fromUtf8(name));++checks;QTextStream(stdout)<<"PASS "<<name<<'\n';};
    auto rejects=[&](const char *name,const std::function<void()> &job){bool rejected=false;try{job();}catch(const Error&){rejected=true;}check(rejected,name);};
    try{
        fixture::SigningKey key,other;
        const auto now=QDateTime::fromString("2026-11-05T00:00:00Z",Qt::ISODate);
        QJsonObject platforms;
        for(auto platform:QStringList{"windows-x64","macos-arm64","macos-x64"}){
            const auto suffix=platform=="windows-x64"?QStringLiteral("Windows-x64"):QStringLiteral("macOS-")+platform.mid(6);
            auto file=[&](const QString &name){return QJsonObject{{"name",name},{"url",QStringLiteral("https://github.com/Team-HJD-21/team-hjd-game-test-launcher/releases/download/v0.2.0/")+name},{"bytes",12345},{"sha256",QString(64,'a')}};};
            platforms[platform]=QJsonObject{{"payload",file("TeamHJD-LauncherPayload-"+suffix+".zip")},{"installer",file("TeamHJD-GameTestLauncher-"+suffix+(platform=="windows-x64"?"-Setup.exe":".dmg"))}};
        }
        QJsonObject valid{{"schema",1},{"sequence",2},{"version","0.2.0"},{"minimumVersion","0.2.0"},{"publishedAt","2026-11-04T23:00:00Z"},{"expiresAt","2027-02-01T00:00:00Z"},{"platforms",platforms}};
        auto envelope=[&](const QJsonObject &p){auto o=QJsonDocument::fromJson(key.envelope(p)).object();o["keyId"]=updates::keyId;return QJsonDocument(o).toJson(QJsonDocument::Compact);};
        auto verify=[&](const QJsonObject &p){return updates::verifyRelease(envelope(p),key.publicKey,1,now);};
        auto release=verify(valid);
        check(release.version=="0.2.0"&&release.minimumVersion=="0.2.0"&&release.sequence==2,"signed stable release selects this native platform");
        check(release.payload["name"].toString()==launcherPayloadName(),"platform payload matches the native distribution name");
        rejects("untrusted signing key rejected",[&]{updates::verifyRelease(envelope(valid),other.publicKey,0,now);});
        rejects("sequence rollback rejected",[&]{updates::verifyRelease(envelope(valid),key.publicKey,3,now);});
        auto bad=valid;bad["minimumVersion"]="0.3.0";rejects("impossible mandatory version rejected",[&]{verify(bad);});
        bad=valid;bad["version"]="0.2.0-beta";rejects("prerelease format rejected",[&]{verify(bad);});
        bad=valid;bad["expiresAt"]="2026-11-05T00:00:00Z";rejects("expired at exact boundary rejected",[&]{verify(bad);});
        bad=valid;bad["publishedAt"]="2026-11-05T00:05:01Z";rejects("future publication rejected",[&]{verify(bad);});
        bad=valid;bad["expiresAt"]="2030-01-01T00:00:00Z";rejects("unbounded feed validity rejected",[&]{verify(bad);});
        bad=valid;bad["sequence"]=2.5;rejects("fractional sequence rejected",[&]{verify(bad);});
        bad=valid;auto missing=platforms;missing.remove("macos-x64");bad["platforms"]=missing;rejects("partial platform publication rejected",[&]{verify(bad);});
        for(const auto &url:QStringList{"https://evil.example/payload.zip","https://github.com/other/repo/file.zip","https://user@github.com/Team-HJD-21/team-hjd-game-test-launcher/releases/download/v0.2.0/file.zip"})
            check(!updates::assetUrl(QUrl(url),"0.2.0","file.zip"),"foreign or credential-bearing artifact address rejected");
        auto wire=QJsonDocument::fromJson(envelope(valid)).object();wire["payload"]=QString::fromLatin1(QJsonDocument(bad).toJson().toBase64());
        rejects("tampered payload signature rejected",[&]{updates::verifyRelease(QJsonDocument(wire).toJson(),key.publicKey,0,now);});
        wire=QJsonDocument::fromJson(envelope(valid)).object();wire["keyId"]="different-key";
        rejects("unknown signing key identifier rejected",[&]{updates::verifyRelease(QJsonDocument(wire).toJson(),key.publicKey,0,now);});
        rejects("oversized manifest rejected",[&]{updates::verifyRelease(QByteArray(256*1024+1,' '),key.publicKey,0,now);});
        QTextStream(stdout)<<"RESULT "<<checks<<" signed launcher release assertions passed; ephemeral keys only.\n";return 0;
    }catch(const std::exception &error){QTextStream(stderr)<<error.what()<<'\n';return 1;}
}
