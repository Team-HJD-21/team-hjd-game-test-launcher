#include "client/Controller.h"
#include "client/WindowFrame.h"
#include "core/LauncherUpdate.h"
#include "SigningKey.h"
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSet>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>

using namespace hjd;
class FolderCapture : public QObject {
    Q_OBJECT
public:
    QUrl opened;
public slots:
    void capture(const QUrl &url){opened=url;}
};

class ClientExperienceFixture {
public:
    static void run(){
        QTemporaryDir temp;if(!temp.isValid())fail("temporary profile");
        const auto root=temp.path()+"/runtime",profile=temp.path()+"/profile";
        QDir().mkpath(root);QDir().mkpath(profile+"/games");
        Controller controller(profile,root,true);
        controller.m_autoTimer.stop();controller.m_runningTimer.stop();
        controller.m_cloudflare=false;controller.m_config.server=QUrl();
        int checks=0;
        auto check=[&](bool ok,const char *label){if(!ok)fail(QString::fromUtf8(label));++checks;QTextStream(stdout)<<"PASS "<<label<<'\n';};
        auto until=[&](const std::function<bool()> &condition){
            QElapsedTimer elapsed;elapsed.start();
            while(!condition()){
                if(elapsed.elapsed()>7000)fail("client experience fixture timeout");
                QCoreApplication::processEvents();QThread::msleep(2);
            }
        };
        auto idle=[&](int ms){QElapsedTimer elapsed;elapsed.start();until([&]{return elapsed.elapsed()>=ms;});};
        fixture::SigningKey key;controller.m_releaseKey=key.publicKey;
        auto feed=[&](const QString &version,qint64 sequence){
            QJsonObject platforms;
            for(const auto &platform:QStringList{"windows-x64","macos-arm64","macos-x64"}){
                const auto suffix=platform=="windows-x64"?QString("Windows-x64"):QString("macOS-")+platform.mid(6);
                auto artifact=[&](const QString &name){return QJsonObject{{"name",name},{"url","https://github.com/Team-HJD-21/team-hjd-game-test-launcher/releases/download/v"+version+"/"+name},{"bytes",10},{"sha256",QString(64,'a')}};};
                platforms[platform]=QJsonObject{{"payload",artifact("TeamHJD-LauncherPayload-"+suffix+".zip")},{"installer",artifact("TeamHJD-GameTestLauncher-"+suffix+(platform=="windows-x64"?"-Setup.exe":".dmg"))}};
            }
            const auto now=QDateTime::currentDateTimeUtc();
            auto envelope=QJsonDocument::fromJson(key.envelope({{"schema",1},{"sequence",sequence},{"version",version},{"minimumVersion",version},{"publishedAt",now.addSecs(-60).toString(Qt::ISODate)},{"expiresAt",now.addDays(1).toString(Qt::ISODate)},{"platforms",platforms}})).object();
            envelope["keyId"]=updates::keyId;return QJsonDocument(envelope).toJson();
        };
        QByteArray response=feed(controller.version(),1);
        std::atomic_bool hold=false,entered=false,broken=false;
        controller.m_updateFetch=[&](const QUrl &){
            entered=true;QElapsedTimer bound;bound.start();
            while(hold&&!controller.m_cancel&&bound.elapsed()<6000)QThread::msleep(2);
            if(controller.m_cancel)fail(QStringLiteral("취소했습니다."));
            if(broken)fail("synthetic feed outage");
            return response;
        };
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("launcher",&controller);
        engine.load(QUrl("qrc:/qml/Main.qml"));
        check(!engine.rootObjects().isEmpty(),"production QML loads with the real Controller");
        auto *window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        check(window!=nullptr,"native experience window exists");
        window->setOpacity(0);window->setFlag(Qt::WindowTransparentForInput);
        auto *notice=window->findChild<QObject*>("launcherUpdateNotice");
        auto *status=window->findChild<QObject*>("launcherVersionStatus");
        check(notice&&status&&window->findChild<QObject*>("launcherInstallFolderAction"),"production update modal, footer and launcher-folder action exist");
        hold=true;controller.checkLauncherUpdate(true);
        until([&]{return entered.load()&&notice->property("opened").toBool();});
        check(controller.view()["updateGate"]=="checking","first startup still blocks before update verification");
        hold=false;until([&]{return !controller.m_busy;});idle(30);
        check(controller.m_updateVerified&&!notice->property("opened").toBool(),"current signed release dismisses the initial modal");
        hold=true;entered=false;controller.checkLauncherUpdate(true);until([&]{return entered.load();});
        check(controller.view()["launcherChecking"].toBool()&&controller.view()["updateGate"].toString().isEmpty()&&controller.m_updateVerified,"routine check preserves the verified session and has no blocking gate");
        check(!notice->property("opened").toBool()&&!window->property("blockingFlow").toBool()&&status->property("text")==QStringLiteral("버전 확인중"),"routine checks only update the footer and leave the UI usable");
        QSet<int> dots;
        for(int i=0;i<9;++i){dots.insert(window->property("launcherCheckDots").toInt());idle(210);}
        check(dots==QSet<int>{0,1,2,3},"footer dots animate from zero through three");
        hold=false;until([&]{return !controller.m_busy;});
        check(status->property("text")==QStringLiteral("최신 런처")&&!notice->property("opened").toBool(),"quiet success restores latest status without a popup");
        hold=true;controller.checkLauncherUpdate(true);
        bool deferred=false;check(controller.deferDuringCheck([&]{deferred=true;}),"foreground actions can interrupt a routine check");
        until([&]{return deferred&&!controller.m_busy;});hold=false;
        check(controller.m_updateGate.isEmpty()&&controller.m_updateVerified&&!controller.m_launcherChecking&&controller.m_error.isEmpty(),"intentional background cancellation does not create an offline modal");
        broken=true;controller.checkLauncherUpdate(true);until([&]{return !controller.m_busy;});idle(30);
        check(controller.m_updateGate=="error"&&!controller.m_updateVerified&&notice->property("opened").toBool(),"real feed failure still blocks access and offers retry");
        broken=false;response=feed("9.0.0",2);controller.checkLauncherUpdate(true);until([&]{return !controller.m_busy;});idle(30);
        check(controller.m_updateGate=="available"&&!controller.m_updateVerified&&notice->property("opened").toBool()&&!QFileInfo::exists(root+"/queued-launcher.json"),"a newly discovered release still requires explicit update approval");
        controller.m_updateGate.clear();controller.m_updateVerified=true;emit controller.changed();
        FolderCapture capture;QDesktopServices::setUrlHandler("file",&capture,"capture");
        auto expectedRoot=QDir::toNativeSeparators(root);
#ifdef Q_OS_MACOS
        const auto bundle=temp.path()+"/Applications/TeamHJDLauncher.app";
        QDir().mkpath(bundle+"/Contents/MacOS");
        writeJson(root+"/bootstrap-location.json",{{"executable",bundle+"/Contents/MacOS/TeamHJDLauncher"}});
        expectedRoot=bundle;
#endif
        check(controller.launcherInstallRoot()==expectedRoot,"installation info resolves the stable launcher rather than its version slot");
        controller.openFolder("launcher");
#ifdef Q_OS_MACOS
        check(capture.opened.toLocalFile()==QFileInfo(expectedRoot).absolutePath(),"launcher folder opens Finder containing the app instead of launching another instance");
#else
        check(capture.opened.toLocalFile()==QDir::fromNativeSeparators(expectedRoot),"launcher folder opens the actual installation root");
#endif
        controller.openFolder("root");check(capture.opened.toLocalFile()==profile+"/games","game installation folder stays distinct");
        QDesktopServices::unsetUrlHandler("file");
        int completed=0;QObject::connect(&controller,&Controller::authenticationCompleted,&controller,[&]{++completed;});
        controller.finishLogin(controller.m_config,{}, {},{},QStringLiteral("취소했습니다."));
        check(completed==0,"cancelled authentication cannot return focus or claim completion");
        controller.finishLogin(controller.m_config,{}, {},{},QStringLiteral("인증 검증 실패"));
        check(completed==0,"failed origin authentication cannot activate the launcher");
        auto config=controller.m_config;config.cloudflareToken="isolated-synthetic-session";
        controller.finishLogin(config,QDateTime::currentDateTimeUtc().addSecs(3600),{},"{}",{});
        check(completed==1&&controller.m_online&&QFileInfo::exists(profile+"/catalog-cache.json"),"validated completion requests app return only after credential and catalog persistence");
        const auto priorRoot=controller.m_dataRoot;controller.m_dataRoot=temp.path()+"/not-a-directory";atomicWrite(controller.m_dataRoot,"fixture");
        controller.finishLogin(config,QDateTime::currentDateTimeUtc().addSecs(3600),{},"{}",{});controller.m_dataRoot=priorRoot;
        check(completed==1&&!controller.m_online,"credential storage failure cannot claim completion or activate the window");
        WindowFrame frame(window);
        window->showMaximized();idle(70);window->showMinimized();idle(70);frame.restoreAfterAuthentication();idle(100);
        if(window->visibility()!=QWindow::Maximized)QTextStream(stderr)<<"restore visibility="<<int(window->visibility())<<" states="<<int(window->windowStates())<<'\n';
        check(window->isVisible()&&window->visibility()==QWindow::Maximized,"authentication return restores a minimized maximized window without changing its size mode");
        window->showNormal();idle(70);window->showMinimized();idle(70);frame.restoreAfterAuthentication();idle(100);
        check(window->isVisible()&&window->visibility()==QWindow::Windowed,"authentication return also restores a regular minimized window");
        window->hide();
        QTextStream(stdout)<<"RESULT "<<checks<<" client experience assertions passed; isolated signed transport and authentication completion fixtures, no human OTP.\n";
    }
};
int main(int argc,char **argv){
    QGuiApplication app(argc,argv);app.setOrganizationName("TeamHJDTests");app.setApplicationName("ClientExperienceTests");app.setQuitOnLastWindowClosed(false);QQuickStyle::setStyle("Basic");
    try{ClientExperienceFixture::run();return 0;}catch(const std::exception &error){QTextStream(stderr)<<error.what()<<'\n';return 1;}
}
#include "ClientExperienceTests.moc"
