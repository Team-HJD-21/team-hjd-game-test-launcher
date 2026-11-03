#include "client/ConnectionState.h"
#include "client/Controller.h"
#include "client/DefaultChannel.h"
#include "core/Core.h"
#include <QGuiApplication>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTextStream>

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
        QTemporaryDir empty;check(empty.isValid(),"isolated first-use profile created");
        {Controller controller(empty.path(),{},false);auto view=controller.view();check(view["needsLogin"].toBool()&&view["connectionState"].toString()=="authentication-required"&&!view["hadEmailSession"].toBool(),"real first-use Controller exposes an email action without retrying an unauthenticated catalog");}
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
        QTextStream(stdout)<<"RESULT "<<assertions<<" connection-state assertions passed; no real credentials or network requests used.\n";
        return 0;
    }catch(const std::exception &error){QTextStream(stderr)<<error.what()<<'\n';return 1;}
}
