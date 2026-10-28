#include "client/Controller.h"
#include "core/Core.h"
#include <QCommandLineParser>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonObject>
#include <QMetaObject>
#include <QTextStream>
#include <QTimer>

using namespace hjd;

namespace {
void require(bool condition,const QString &message){if(!condition)fail(message);}
}

int main(int argc,char **argv){
    QGuiApplication app(argc,argv);
    app.setOrganizationName("TeamHJDTests");
    app.setApplicationName("ControllerUpdateTests");
    QCommandLineParser parser;parser.addHelpOption();
    parser.addOptions({{"profile","Isolated fixture profile","path"},{"catalog","Signed published fixture catalog","path"},{"result","Private evidence JSON","path"}});
    parser.process(app);
    const auto profile=QDir::cleanPath(QFileInfo(parser.value("profile")).absoluteFilePath());
    const auto resultPath=parser.value("result");
    try{
        require(parser.isSet("profile")&&parser.isSet("catalog")&&parser.isSet("result"),"Explicit fixture profile, catalog and result are required.");
        const auto settings=readJson(profile+"/settings.json");
        const auto server=QUrl(settings["server"].toString());
        require(QStringList{"localhost","127.0.0.1","::1"}.contains(server.host())&&server.scheme()=="https","Only an isolated loopback HTTPS fixture is accepted.");
        const auto games=QDir::cleanPath(settings["gamesRoot"].toString());
        require(games==profile+"/games","The fixture game store must be inside the isolated profile.");
        Store store(games);
        auto oldReceipt=store.currentReceipt("fixture");
        require(oldReceipt["version"].toString()=="1.0.0"&&oldReceipt["executable"].toString()=="Fixture.exe"&&store.verify("fixture"),"A verified synthetic 1.0.0 fixture must be seeded.");
        const auto originalFolder=store.state("fixture")["current"].toString();
        const auto catalog=verifyCatalog(readFile(parser.value("catalog")),settings["publisherKey"].toString(),server);
        Game latest;for(const auto &game:catalog.games)if(game.id=="fixture")latest=game;
        require(latest.id=="fixture"&&latest.version=="1.1.0"&&latest.executable=="Fixture.exe","The signed fixture publication must offer 1.1.0.");
        const auto newArchive=profile+"/downloads/"+latest.sha256+".part";
        require(!QFileInfo::exists(newArchive),"The newer archive must not be present before detection.");

        Controller controller(profile,{},true);
        enum class Phase { Detect, Observe, StartOld, OldRunning, AfterOldExit, ExplicitUpdate, Rollback, RollbackRefresh, RollbackObserve, ExplicitAfterRollback };
        Phase phase=Phase::Detect;
        QElapsedTimer timer,total;timer.start();total.start();
        int assertions=0,updateNotices=0;
        bool oldProcessObserved=false,done=false;
        auto checked=[&](bool condition,const QString &message){require(condition,message);++assertions;QTextStream(stdout)<<"PASS "<<message<<'\n';};
        QObject::connect(&controller,&Controller::toast,&app,[&](const QString &title,const QString &message,const QString &){if((title.contains(QStringLiteral("업데이트"))||title.contains(QStringLiteral("버전")))&&message.contains(latest.version))++updateNotices;});
        auto unchanged=[&]{
            require(store.state("fixture")["current"].toString()==originalFolder,"Detection or old-version play silently changed the active installation.");
            require(!QFileInfo::exists(newArchive),"Detection or old-version play downloaded a new archive before an explicit Update action.");
        };
        auto invoke=[&](const char *method){require(QMetaObject::invokeMethod(&controller,method,Qt::DirectConnection),QStringLiteral("The QML action is missing: ")+QString::fromLatin1(method));};
        auto offered=[&]{auto view=controller.view();require(view["updateAvailable"].toBool()&&view["action"].toString()==QStringLiteral("업데이트"),"The explicit Update action is not offered for the available version.");};
        auto finish=[&](const QString &error){
            if(done)return;done=true;
            if(!error.isEmpty()){QTextStream(stderr)<<"FAIL "<<error<<'\n';controller.cancel();app.exit(1);return;}
            writeJson(resultPath,{{"result","PASS"},{"controllerStartupDetection",true},{"noAutomaticGameDownload",true},{"oldVersionPlayable",oldProcessObserved},{"noAutomaticUpdateAfterGameExit",true},{"explicitUpdateVerified",true},{"previousVersionPreserved",true},{"rollbackKeepsManualUpdateAvailable",true},{"explicitUpdateAfterRollback",true},{"assertions",assertions},{"updateNotices",updateNotices},{"elapsedMs",total.elapsed()}});
            QTextStream(stdout)<<"RESULT "<<assertions<<" Controller update assertions passed; no automatic game download or activation.\n";
            app.quit();
        };
        QTimer poll;poll.setInterval(75);
        QObject::connect(&poll,&QTimer::timeout,&app,[&]{
            if(done)return;
            try{
                auto view=controller.view();const bool idle=!view["working"].toBool();
                if(total.elapsed()>90000)fail("The Controller update integration fixture timed out.");
                switch(phase){
                case Phase::Detect:
                    unchanged();
                    if(view["online"].toBool()&&view["latest"].toString()==latest.version&&idle){
                        offered();checked(view["installed"].toString()=="1.0.0","startup discovers the new signed version while preserving installed 1.0.0");
                        phase=Phase::Observe;timer.restart();
                    }
                    break;
                case Phase::Observe:
                    unchanged();
                    if(timer.elapsed()>1800&&idle){
                        offered();checked(updateNotices>0,"automatic detection announces the available update");
                        checked(QDir(games+"/fixture/versions").entryList(QDir::Dirs|QDir::NoDotAndDotDot).size()==1,"detection does not stage or activate another installation");
                        invoke("playInstalled");phase=Phase::StartOld;timer.restart();
                    }
                    break;
                case Phase::StartOld:
                    unchanged();
                    if(store.running("fixture")){
                        const auto running=readJson(games+"/fixture/running.json");
                        checked(running["folder"].toString()==originalFolder&&running["executable"].toString()=="Fixture.exe","the installed-version action launches the old verified executable");
                        oldProcessObserved=true;phase=Phase::OldRunning;
                    }else if(timer.elapsed()>10000)fail("The installed-version action did not launch the fixture.");
                    break;
                case Phase::OldRunning:
                    unchanged();
                    if(!store.running("fixture")){phase=Phase::AfterOldExit;timer.restart();}
                    break;
                case Phase::AfterOldExit:
                    unchanged();
                    if(timer.elapsed()>2200&&idle){
                        offered();checked(store.currentReceipt("fixture")["version"].toString()=="1.0.0","game exit and background refresh preserve the old version");
                        invoke("primary");phase=Phase::ExplicitUpdate;timer.restart();
                    }
                    break;
                case Phase::ExplicitUpdate:
                    if(store.currentReceipt("fixture")["version"].toString()==latest.version&&idle){
                        checked(store.verify("fixture"),"explicit Update downloads and verifies every new file");
                        checked(QFileInfo(newArchive).size()==latest.bytes&&fileHash(newArchive)==latest.sha256,"the explicit download matches the signed archive size and SHA-256");
                        checked(store.state("fixture")["previous"].toString()==originalFolder,"explicit Update retains the previous installation");
                        invoke("rollback");phase=Phase::Rollback;timer.restart();
                    }else if(idle&&!view["error"].toString().isEmpty())fail(view["error"].toString());
                    break;
                case Phase::Rollback:
                    if(store.currentReceipt("fixture")["version"].toString()=="1.0.0"&&idle){
                        offered();checked(true,"rollback still offers the newer version through the manual Update action");
                        invoke("refresh");phase=Phase::RollbackRefresh;timer.restart();
                    }
                    break;
                case Phase::RollbackRefresh:
                    require(store.currentReceipt("fixture")["version"].toString()=="1.0.0","refresh after rollback silently reapplied the newer version.");
                    if(idle&&view["online"].toBool()&&timer.elapsed()>300){phase=Phase::RollbackObserve;timer.restart();}
                    break;
                case Phase::RollbackObserve:
                    require(store.currentReceipt("fixture")["version"].toString()=="1.0.0","rollback was silently undone before another explicit Update action.");
                    if(timer.elapsed()>1500&&idle){offered();invoke("primary");phase=Phase::ExplicitAfterRollback;timer.restart();}
                    break;
                case Phase::ExplicitAfterRollback:
                    if(store.currentReceipt("fixture")["version"].toString()==latest.version&&idle){
                        checked(store.verify("fixture"),"manual Update can return to the new version after rollback");
                        checked(!view["updateAvailable"].toBool()&&view["action"].toString()==QStringLiteral("플레이"),"the updated installation returns to the Play action");
                        finish({});
                    }else if(idle&&!view["error"].toString().isEmpty())fail(view["error"].toString());
                    break;
                }
            }catch(const std::exception &error){finish(QString::fromUtf8(error.what()));}
        });
        poll.start();return app.exec();
    }catch(const std::exception &error){QTextStream(stderr)<<"FAIL "<<error.what()<<'\n';return 1;}
}
