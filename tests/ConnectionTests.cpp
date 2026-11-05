#include "client/ConnectionState.h"
#include "client/Controller.h"
#include "client/DefaultChannel.h"
#include "client/ManagedLauncherRoot.h"
#include "core/Core.h"
#include <QGuiApplication>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTextStream>
#include <QFile>
#include <QTimer>

int main(int argc,char **argv){
    QGuiApplication app(argc,argv);
    app.setOrganizationName("TeamHJDTests");app.setApplicationName("ConnectionTests");
    int assertions=0;
    auto check=[&](bool condition,const char *message){if(!condition)hjd::fail(QString::fromUtf8(message));++assertions;QTextStream(stdout)<<"PASS "<<message<<'\n';};
    try{
        check(connectionState(true,true,true,false,false,true).id=="authentication-required","expired authentication is not reported as an offline server");
        check(connectionState(true,true,true,true,false,true).label==QStringLiteral("이메일 재인증 필요"),"a public launcher update check cannot hide the need to reauthenticate");
        check(connectionState(true,false,true,false,false,true).label==QStringLiteral("서버 오프라인"),"failed authenticated connection explicitly shows server offline");
        check(connectionState(true,false,true,true,false,true).id=="checking","an actual catalog check has its own state");
        check(connectionState(true,false,true,false,true,true).id=="online","a verified catalog reports online");
        check(connectionState(true,true,false,false,false,false).label==QStringLiteral("이메일 인증 필요"),"first login is distinguished from reauthentication");
        check(connectionState(false,false,false,false,false,false).id=="unconfigured","a missing endpoint is not reported as offline");
        check(connectionState(true,false,false,false,false,false).id=="connecting","initial startup does not claim a completed failed check");
        QTemporaryDir managed;check(managed.isValid(),"isolated managed client root created");
#ifdef Q_OS_MACOS
        const QString entry="TeamHJDClient.app/Contents/MacOS/TeamHJDClient";
#else
        const QString entry="TeamHJDClient.exe";
#endif
        const auto client=managed.path()+"/launcher/versions/0.1.0/"+entry;
        QDir().mkpath(QFileInfo(client).absolutePath());
        {QFile file(client);check(file.open(QIODevice::WriteOnly)&&file.write("synthetic-installed-client")>0,"synthetic managed executable written");}
        hjd::writeJson(managed.path()+"/launcher/active.json",{{"current","0.1.0"}});
        hjd::writeJson(managed.path()+"/launcher/versions/0.1.0/._teamhjd-receipt.json",{{"version","0.1.0"},{"executable",entry},{"files",QJsonArray{QJsonObject{{"path",entry},{"bytes",QFileInfo(client).size()},{"sha256",hjd::fileHash(client)}}}}});
        check(managedLauncherRoot(client)==managed.path(),"directly opened receipted installed client retains its managed update root");
        {QFile file(client);check(file.open(QIODevice::Append)&&file.write("changed")>0,"managed executable alteration fixture written");}
        check(managedLauncherRoot(client).isEmpty(),"unreceipted or altered executable is not adopted for automatic updates");
        QTemporaryDir empty;check(empty.isValid(),"isolated first-use profile created");
        {Controller controller(empty.path(),{},false);auto view=controller.view();check(view["needsLogin"].toBool()&&view["connectionState"].toString()=="authentication-required"&&!view["hadEmailSession"].toBool(),"real first-use Controller exposes an email action without retrying an unauthenticated catalog");}
        {Controller controller(empty.path(),managed.path(),false);check(controller.view()["updateGate"].toString()=="checking","managed startup requires an update check before the authentication flow");}
        {QTemporaryDir consent;Controller controller(consent.path(),{},false);
            check(controller.view()["needsPrivacy"].toBool(),"first online use requires explicit privacy choices");
            controller.acceptPrivacy(true,false,true);check(controller.view()["needsPrivacy"].toBool(),"collection consent alone does not imply overseas consent");
            controller.acceptPrivacy(true,true,false);check(controller.view()["needsPrivacy"].toBool(),"under-age automatic onboarding remains blocked");
            controller.acceptPrivacy(true,true,true);check(!controller.view()["needsPrivacy"].toBool()&&QFileInfo::exists(consent.path()+"/settings.json"),"both explicit consents persist before email authentication");
            controller.forgetLogin();check(controller.view()["needsPrivacy"].toBool()&&!controller.view()["online"].toBool(),"forgetting local credentials clears consent and online access");
        }
        QTemporaryDir expired;check(expired.isValid(),"isolated expired-session profile created");
#ifdef Q_OS_MACOS
        if(qgetenv("TEAMHJD_TEST_KEYCHAIN_READY")!="1")
            hjd::fail("Create and unlock a disposable CI keychain before setting TEAMHJD_TEST_KEYCHAIN_READY=1");
#endif
        const auto encoding=QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals;
        const auto now=QDateTime::currentSecsSinceEpoch();
        const auto header=QJsonDocument(QJsonObject{{"alg","RS256"},{"kid","synthetic-only"}}).toJson(QJsonDocument::Compact).toBase64(encoding);
        const auto payload=QJsonDocument(QJsonObject{{"iss",channel::issuer},{"aud",channel::audience},{"type","app"},{"iat",now-7200},{"nbf",now-7200},{"exp",now-3600}}).toJson(QJsonDocument::Compact).toBase64(encoding);
        const auto token=header+'.'+payload+'.'+QByteArray("synthetic-not-a-signature").toBase64(encoding);
        hjd::writeJson(expired.path()+"/settings.json",{{"server",channel::server},{"publisherKey",channel::publisherKey},{"cloudflareBlob",QString::fromLatin1(hjd::protect(token).toBase64())}});
        {Controller controller(expired.path(),{},false);
#ifdef Q_OS_MACOS
            // Only called in the CI's prepared disposable Keychain. Expired
            // claims fail before refresh() starts any network work.
            controller.restoreCredentials();
#endif
            auto view=controller.view();check(view["needsLogin"].toBool()&&!view["online"].toBool()&&view["hadEmailSession"].toBool()&&view["connectionLabel"].toString()==QStringLiteral("이메일 재인증 필요"),"real Controller restores an expired protected session as reauthentication required, never online or reconnecting");
        }
        const auto gameFolder=expired.path()+"/games/hose-for-hire/versions/1.0.0";
#ifdef Q_OS_MACOS
        const QString gameEntry="Fixture.app/Contents/MacOS/Fixture", fixtureName="LauncherFixtureGame", gamePlatform="macos-universal";
#else
        const QString gameEntry="Fixture.exe", fixtureName="LauncherFixtureGame.exe", gamePlatform="windows-x64";
#endif
        const auto fixture=QCoreApplication::applicationDirPath()+"/"+fixtureName;
        const auto gameFile=gameFolder+"/"+gameEntry;
        QDir().mkpath(QFileInfo(gameFile).absolutePath());check(QFile::copy(fixture,gameFile),"real synthetic game copied for offline play");
        QFile::setPermissions(gameFile,QFileInfo(fixture).permissions());
        const auto bytes=QFileInfo(gameFile).size();
        hjd::writeJson(gameFolder+"/._teamhjd-receipt.json",{{"version","1.0.0"},{"executable",gameEntry},{"platform",gamePlatform},{"expandedBytes",bytes},{"files",QJsonArray{QJsonObject{{"path",gameEntry},{"bytes",bytes},{"sha256",hjd::fileHash(gameFile)},{"executableFile",true}}}}});
        hjd::writeJson(expired.path()+"/games/hose-for-hire/active.json",{{"current","1.0.0"}});
        Controller offline(expired.path(),{},false);
        check(offline.view()["needsLogin"].toBool()&&!offline.view()["canPlay"].toBool(),"expired authentication cannot unlock installed-game playback");
        offline.playInstalled();
        check(!offline.view()["working"].toBool()&&!offline.view()["running"].toBool()&&!offline.view()["error"].toString().isEmpty(),"direct backend playback is blocked without an authenticated connection");
        check(hjd::Store(expired.path()+"/games").verify("hose-for-hire"),"blocking offline play preserves installed game bytes");
        QTextStream(stdout)<<"RESULT "<<assertions<<" connection-state assertions passed; no real credentials or network requests used.\n";
        return 0;
    }catch(const std::exception &error){QTextStream(stderr)<<error.what()<<'\n';return 1;}
}
