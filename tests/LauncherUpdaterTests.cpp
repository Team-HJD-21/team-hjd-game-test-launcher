#include "client/Controller.h"
#include "core/LauncherUpdate.h"
#include "SigningKey.h"
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QCryptographicHash>
#include <QTextStream>
#include "miniz.h"

using namespace hjd;
class LauncherUpdaterFixture {
public:
    static int run(QGuiApplication &app){
        QTemporaryDir temp;if(!temp.isValid())fail("fixture root");
        const auto root=temp.path()+"/runtime",profile=temp.path()+"/profile",archive=temp.path()+"/fixture.zip";
        const auto binary=readFile(QCoreApplication::applicationDirPath()+"/LauncherFixtureGame.exe");
        const auto digest=QString::fromLatin1(QCryptographicHash::hash(binary,QCryptographicHash::Sha256).toHex());
        QJsonArray bootstrapFiles{QJsonObject{{"path","TeamHJDLauncher.exe"},{"bytes",binary.size()},{"sha256",digest}}};
        const auto manifest=QJsonDocument(QJsonObject{{"version","9.0.0"},{"executable","TeamHJDLauncher.exe"},{"files",bootstrapFiles}}).toJson();
        const QList<QPair<QString,QByteArray>> entries{{"TeamHJDClient.exe",binary},{"bootstrap/TeamHJDLauncher.exe",binary},{"bootstrap-manifest.json",manifest}};
        FILE *file{};_wfopen_s(&file,reinterpret_cast<const wchar_t*>(archive.utf16()),L"wb+");if(!file)fail("fixture archive");
        mz_zip_archive zip{};if(!mz_zip_writer_init_cfile(&zip,file,0))fail("fixture zip init");
        for(const auto &entry:entries)if(!mz_zip_writer_add_mem(&zip,entry.first.toUtf8().constData(),entry.second.data(),size_t(entry.second.size()),MZ_BEST_SPEED))fail("fixture zip add");
        if(!mz_zip_writer_finalize_archive(&zip))fail("fixture zip finalize");mz_zip_writer_end(&zip);fclose(file);
        QJsonObject platforms;
        for(const auto &platform:QStringList{"windows-x64","macos-arm64","macos-x64"}){
            const auto suffix=platform=="windows-x64"?QString("Windows-x64"):QString("macOS-")+platform.mid(6);
            auto artifact=[&](const QString &name){return QJsonObject{{"name",name},{"url","https://github.com/Team-HJD-21/team-hjd-game-test-launcher/releases/download/v9.0.0/"+name},{"bytes",QFileInfo(archive).size()},{"sha256",fileHash(archive)}};};
            platforms[platform]=QJsonObject{{"payload",artifact("TeamHJD-LauncherPayload-"+suffix+".zip")},{"installer",artifact("TeamHJD-GameTestLauncher-"+suffix+(platform=="windows-x64"?"-Setup.exe":".dmg"))}};
        }
        fixture::SigningKey key;const auto now=QDateTime::currentDateTimeUtc();
        auto wire=QJsonDocument::fromJson(key.envelope({{"schema",1},{"sequence",2},{"version","9.0.0"},{"minimumVersion","9.0.0"},{"publishedAt",now.addSecs(-60).toString(Qt::ISODate)},{"expiresAt",now.addDays(1).toString(Qt::ISODate)},{"platforms",platforms}})).object();wire["keyId"]=updates::keyId;
        const auto envelope=QJsonDocument(wire).toJson();
        Controller controller(profile,root,true);controller.m_releaseKey=key.publicKey;controller.m_config.server=QUrl();
        int fetched=0,downloaded=0,checks=0;bool bad=true,done=false,handedOff=false;
        auto check=[&](bool ok,const char *label){if(!ok)fail(QString::fromUtf8(label));++checks;QTextStream(stdout)<<"PASS "<<label<<'\n';};
        controller.m_updateFetch=[&](const QUrl &url){++fetched;if(url.host()!="testlauncher.teamhjd.com"&&url!=QUrl(updates::fallbackFeedUrl))fail("unexpected update endpoint");if(fetched<=2)fail("synthetic temporary connection failure");return envelope;};
        controller.m_updateDownload=[&](const QJsonObject&,const QString &destination){++downloaded;atomicWrite(destination,bad?QByteArray("corrupt transport fixture"):readFile(archive));};
        enum class Phase{NetworkFailure,FailedLatch,DigestFailure,Ready,Wait};Phase phase=Phase::NetworkFailure;
        QElapsedTimer elapsed,afterHandoff;elapsed.start();
        QObject::connect(&controller,&Controller::quitForUpdate,&app,[&]{handedOff=true;afterHandoff.start();});
        QTimer poll;poll.setInterval(20);
        QObject::connect(&poll,&QTimer::timeout,&app,[&]{
            if(done)return;
            try{
                if(elapsed.elapsed()>18000)fail("launcher updater integration timeout");
                const auto view=controller.view();const auto gate=view["updateGate"].toString();
                if(phase==Phase::NetworkFailure&&gate=="error"&&!controller.m_busy){
                    check(fetched==2&&downloaded==0&&!controller.m_updateVerified,"public feed failure blocks online access and schedules retry");
                    check(controller.m_updateRetry.isActive(),"failed update metadata has an automatic retry timer");
                    controller.selectGame("unexpected");check(controller.m_selected=="hose-for-hire","update gate also blocks backend actions");
                    writeJson(root+"/launcher/failed-release.json",{{"version","9.0.0"}});controller.m_updateRetry.start(10);phase=Phase::FailedLatch;
                }else if(phase==Phase::FailedLatch&&gate=="error"&&controller.m_knownFailedRelease&&!controller.m_busy){
                    check(downloaded==0&&controller.m_updateRetry.interval()==300000,"failed-health release waits for a new patch instead of restarting in a loop");
                    controller.retryFailedUpdate();phase=Phase::DigestFailure;
                }else if(phase==Phase::DigestFailure&&gate=="error"&&downloaded==1&&!controller.m_busy){
                    check(!QFileInfo::exists(root+"/queued-launcher.json")&&!controller.m_updateVerified,"corrupt downloaded bytes cannot be queued or enable online access");
                    bad=false;controller.m_retryFailedOnce=true;controller.m_updateRetry.start(10);phase=Phase::Ready;
                }else if(phase==Phase::Ready&&handedOff){
                    auto candidate=readJson(root+"/queued-launcher.json")["candidate"].toString();auto directory=root+"/launcher/versions/"+candidate;
                    check(downloaded==2&&verifyTree(directory,readJson(directory+"/._teamhjd-receipt.json")),"automatic retry stages the complete signed, hashed payload");
                    check(QFileInfo::exists(root+"/launcher/update-evidence/"+candidate+".json")&&!QFileInfo::exists(directory+"/update-evidence.json"),"signed update evidence preserves strict payload tree verification");
                    check(gate=="restarting"&&!controller.m_updateVerified,"verified payload automatically starts its helper and requests client shutdown");
                    phase=Phase::Wait;
                }else if(phase==Phase::Wait&&afterHandoff.elapsed()>3000){done=true;QTextStream(stdout)<<"RESULT "<<checks<<" updater workflow assertions passed; fixture helper handoff, native health tested separately.\n";app.quit();}
            }catch(const std::exception &error){done=true;QTextStream(stderr)<<error.what()<<'\n';app.exit(1);}
        });poll.start();return app.exec();
    }
};
int main(int argc,char **argv){QGuiApplication app(argc,argv);app.setOrganizationName("TeamHJDTests");app.setApplicationName("LauncherUpdaterTests");try{return LauncherUpdaterFixture::run(app);}catch(const std::exception &error){QTextStream(stderr)<<error.what()<<'\n';return 1;}}
