#include "core/Core.h"
#include "core/Platform.h"
#include "core/BootstrapUpdate.h"
#include "Installer.h"
#include <QRegularExpression>
#include <QVersionNumber>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QProcess>
#include <QThread>
#include <QUuid>
#include <QStandardPaths>
#include <QTextStream>
#include <cerrno>
#include <limits>
#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <CoreFoundation/CoreFoundation.h>
#include <libproc.h>
#include <sys/proc.h>
#include <signal.h>
#endif

using namespace hjd;
namespace {
void showMessage(const QString &message,bool error=false){
#ifdef Q_OS_WIN
    MessageBoxW(nullptr,reinterpret_cast<LPCWSTR>(message.utf16()),L"TeamHJD Game Test Launcher",MB_OK|(error?MB_ICONERROR:MB_ICONINFORMATION));
#else
    auto text=CFStringCreateWithCharacters(nullptr,reinterpret_cast<const UniChar*>(message.utf16()),message.size());
    CFUserNotificationDisplayAlert(0,error?kCFUserNotificationStopAlertLevel:kCFUserNotificationNoteAlertLevel,nullptr,nullptr,nullptr,CFSTR("TeamHJD Game Test Launcher"),text,CFSTR("OK"),nullptr,nullptr,nullptr);
    CFRelease(text);
#endif
}
#ifdef Q_OS_MACOS
void seedRuntime(const QString &root){
    const auto seed=QDir::cleanPath(QCoreApplication::applicationDirPath()+"/../Resources/seed");
    const bool hasActive=QFileInfo::exists(root+"/launcher/active.json");
    if(!QFileInfo::exists(seed+"/._teamhjd-receipt.json")){
        if(hasActive)return; // A verified self-update uses a seed-free wrapper.
        fail(QStringLiteral("초기 런처가 없습니다. 공식 DMG로 설치해 주세요."));
    }
    auto receipt=readJson(seed+"/._teamhjd-receipt.json");
    if(receipt["executable"].toString()!=clientExecutable()||!verifyTree(seed,receipt))fail(QStringLiteral("런처 설치 파일을 확인하지 못했습니다. 공식 DMG를 다시 받아 주세요."));
    bool damagedActive=false;
    if(hasActive){
        try{
            Store store(root);auto current=store.currentReceipt("launcher")["version"].toString();
            if(safeVersion(current)){
                const auto comparison=QVersionNumber::compare(QVersionNumber::fromString(current),QVersionNumber::fromString(receipt["version"].toString()));
                if(comparison>0||(comparison==0&&store.verify("launcher")))return;
            }
        }catch(const Error&){damagedActive=true;}
        if(Store(root).running("launcher"))fail(QStringLiteral("기존 런처를 종료한 뒤 새 앱을 실행해 주세요."));
    }
    auto version=receipt["version"].toString();if(!safeVersion(version))fail(QStringLiteral("설치 버전 정보가 올바르지 않습니다."));
    // Each attempt has its own immutable directory. A crash or full disk may
    // leave an incomplete copy, but the next launch can start a fresh attempt.
    auto folder=version+"-seed-"+QUuid::createUuid().toString(QUuid::Id128).left(8),target=root+"/launcher/versions/"+folder;
    noLinks(target);
    if(QFileInfo::exists(target))fail(QStringLiteral("초기 런처 준비 폴더가 이미 있습니다."));
    {
        if(!QDir().mkpath(target))fail(QStringLiteral("런처 저장 폴더를 만들지 못했습니다."));
        for(const auto &entry:receipt["files"].toArray()){
            const auto relative=entry.toObject()["path"].toString();
            auto source=seed+"/"+relative,destination=target+"/"+relative;
            noLinks(destination);QDir().mkpath(QFileInfo(destination).absolutePath());
            if(!QFile::copy(source,destination)||!QFile::setPermissions(destination,QFileInfo(source).permissions()))fail(QStringLiteral("초기 런처 파일을 복사하지 못했습니다."));
        }
        writeJson(target+"/._teamhjd-receipt.json",receipt);
        if(!verifyTree(target,receipt))fail(QStringLiteral("복사된 런처 파일 검증에 실패했습니다."));
    }
    if(damagedActive){
        const auto preserved=root+"/launcher/repair-active-"+QUuid::createUuid().toString(QUuid::Id128)+".json";
        if(!QFile::rename(root+"/launcher/active.json",preserved))fail(QStringLiteral("손상된 설치 정보를 보존하지 못했습니다."));
    }
    Store(root).activate("launcher",folder);
}
#endif
bool launchHealthy(const QString &root,Store &store,const QString &dataRoot={}) {
    auto directory=store.currentDirectory("launcher");auto receipt=store.currentReceipt("launcher");
    if(receipt["executable"].toString()!=clientExecutable()||!verifyTree(directory,receipt))return false;
    auto health=root+"/health/"+QUuid::createUuid().toString(QUuid::Id128)+".json";
    QDir().mkpath(root+"/health");
    auto exe=directory+"/"+clientExecutable();qint64 pid=0;
    QStringList childArguments{"--launcher-root",root,"--health-file",health};
    if(!dataRoot.isEmpty())childArguments<<"--data-root"<<dataRoot;
    if(!QProcess::startDetached(exe,childArguments,directory,&pid))return false;
#ifdef Q_OS_WIN
    HANDLE child=OpenProcess(SYNCHRONIZE|PROCESS_TERMINATE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,DWORD(pid));
#else
    auto childStamp=processStamp(pid,exe);
#endif
    bool healthy=false;
    for(int i=0;i<400;++i){
        if(QFileInfo::exists(health)){try{auto acknowledgment=readJson(health);healthy=acknowledgment["pid"].toInteger()==pid&&acknowledgment["version"].toString()==receipt["version"].toString();}catch(const Error&){}if(healthy)break;}
#ifdef Q_OS_WIN
        if(child&&WaitForSingleObject(child,0)!=WAIT_TIMEOUT)break;
#else
        if(!processStamp(pid,exe))break;
#endif
        QThread::msleep(50);
    }
    // This handle refers only to the child just started, never a game or unrelated app.
#ifdef Q_OS_WIN
    if(child){if(!healthy&&WaitForSingleObject(child,0)==WAIT_TIMEOUT){TerminateProcess(child,5);WaitForSingleObject(child,3000);}CloseHandle(child);}
#else
    if(!healthy&&childStamp&&processStamp(pid,exe)==childStamp){kill(pid_t(pid),SIGTERM);for(int i=0;i<60&&processStamp(pid,exe)==childStamp;++i)QThread::msleep(50);}
#endif
    if(healthy)writeJson(root+"/launcher/running.json",{{"pid",pid},{"stamp",QString::number(processStamp(pid,exe))},{"folder",store.state("launcher")["current"]},{"executable",clientExecutable()}});
    QFile::remove(health);return healthy;
}
}
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);app.setOrganizationName("TeamHJD");app.setApplicationName("GameTestLauncher");
    auto root=QCoreApplication::applicationDirPath();
#ifdef Q_OS_MACOS
    root=QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/TeamHJD/LauncherRuntime-"+platformId();
#endif
    auto args=app.arguments();QString dataRoot;
    auto option=[&](const QString &name){auto index=args.indexOf(name);if(index<0)return QString();if(index+1>=args.size())fail(QStringLiteral("실행기 인수가 누락되었습니다."));return args[index+1];};
    try{
        if(handleInstaller(args,[](const QString &installation,const QString &profile){Store installed(installation);return launchHealthy(installation,installed,profile);}))return 0;
#ifdef Q_OS_WIN
        if(!args.contains("--runtime-root")){
            auto maintenance=OpenMutexW(SYNCHRONIZE,FALSE,L"Local\\TeamHJDGameTestLauncher-Installer");
            if(maintenance){CloseHandle(maintenance);if(!args.contains("--noninteractive-test"))showMessage(QStringLiteral("설치·업데이트·제거 작업이 진행 중입니다. 완료 후 런처를 실행해 주세요."));return 0;}
        }
#endif
        auto supplied=option("--runtime-root");
        if(!supplied.isEmpty()){
            root=QDir::cleanPath(QFileInfo(supplied).absoluteFilePath());
            const auto executable=QDir::cleanPath(QCoreApplication::applicationFilePath());
            if(!QDir::isAbsolutePath(supplied)||!executable.startsWith(root+"/launcher/versions/",Qt::CaseInsensitive))fail(QStringLiteral("관리되는 실행기 경로를 확인하지 못했습니다."));
        }
        dataRoot=option("--data-root");
        if(!dataRoot.isEmpty()&&(!QDir::isAbsolutePath(dataRoot)||QDir::cleanPath(dataRoot)==root))fail(QStringLiteral("런처 데이터 경로가 올바르지 않습니다."));
        noLinks(root);Store store(root);
        QDir().mkpath(root);
        if(!args.contains("--runtime-root")&&!store.running("launcher")){
            QString candidate;
            if(QFileInfo::exists(root+"/bootstrap-update.json"))candidate=readJson(root+"/bootstrap-update.json")["sourceFolder"].toString();
            else if(QFileInfo::exists(root+"/queued-launcher.json"))candidate=readJson(root+"/queued-launcher.json")["candidate"].toString();
            if(!candidate.isEmpty()){
                if(!QRegularExpression("^[0-9][A-Za-z0-9._-]{0,160}$").match(candidate).hasMatch()||candidate.endsWith('.'))fail(QStringLiteral("예약된 실행기 경로가 올바르지 않습니다."));
                auto payload=root+"/launcher/versions/"+candidate,helper=bootstrapExecutable(payload);
                if(QFileInfo::exists(helper)){
                    if(!verifyTree(payload,readJson(payload+"/._teamhjd-receipt.json")))fail(QStringLiteral("예약된 실행기 검증에 실패했습니다."));
                    QStringList forwarded{"--runtime-root",root};
                    if(!dataRoot.isEmpty())forwarded<<"--data-root"<<dataRoot;
                    if(args.contains("--noninteractive-test"))forwarded<<"--noninteractive-test";
                    if(!QProcess::startDetached(helper,forwarded,QFileInfo(helper).absolutePath()))fail(QStringLiteral("새 실행기를 시작하지 못했습니다."));
                    return 0;
                }
            }
        }
        QLockFile lock(root+"/bootstrap.lock");if(!lock.tryLock(0))return 2;
#ifdef Q_OS_MACOS
        seedRuntime(root);
#endif
        rememberBootstrap(root);
        recoverBootstrapUpdate(root);
        auto pending=root+"/pending-launcher.json";
        if(!args.contains("--activate")&&store.running("launcher"))return 0;
        if(QFileInfo::exists(pending)){
            auto prior=readJson(pending);if(store.state("launcher")["current"]!=prior["previous"])store.rollback("launcher");QFile::remove(pending);
        }
        auto queued=root+"/queued-launcher.json";
        if(args.contains("--activate")||QFileInfo::exists(queued)){
            QString folder;
            if(args.contains("--activate")){
            int index=args.indexOf("--activate");if(index+1>=args.size())fail(QStringLiteral("업데이트 버전이 없습니다."));folder=args[index+1];
            int waitIndex=args.indexOf("--wait-pid");if(waitIndex+1>=args.size()||waitIndex<0)fail(QStringLiteral("업데이트를 요청한 런처 정보가 없습니다."));
            auto pid=args[waitIndex+1].toLongLong();
            if(pid<=0)fail(QStringLiteral("업데이트를 요청한 프로세스 번호가 올바르지 않습니다."));
#ifdef Q_OS_WIN
            if(quint64(pid)>std::numeric_limits<DWORD>::max())fail(QStringLiteral("업데이트 프로세스 번호가 올바르지 않습니다."));
            HANDLE parent=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,DWORD(pid));
            if(parent){auto exe=store.currentDirectory("launcher")+"/"+clientExecutable();auto stamp=processStamp(pid,exe);auto expected=option("--wait-stamp");if(!stamp||(!expected.isEmpty()&&QString::number(stamp)!=expected)){CloseHandle(parent);fail(QStringLiteral("업데이트를 요청한 프로그램을 확인할 수 없습니다."));}auto result=WaitForSingleObject(parent,120000);CloseHandle(parent);if(result!=WAIT_OBJECT_0)fail(QStringLiteral("실행 중인 런처가 종료되지 않았습니다."));}
            else if(GetLastError()!=ERROR_INVALID_PARAMETER)fail(QStringLiteral("실행 중인 런처를 확인할 수 없습니다."));
#else
            const auto exe=store.currentDirectory("launcher")+"/"+clientExecutable();auto stamp=processStamp(pid,exe);
            auto expected=option("--wait-stamp");
            if(pid>std::numeric_limits<pid_t>::max()||(stamp&&!expected.isEmpty()&&QString::number(stamp)!=expected))fail(QStringLiteral("업데이트를 요청한 프로그램을 확인할 수 없습니다."));
            if(!stamp){
                proc_bsdinfo info{};const auto size=proc_pidinfo(pid_t(pid),PROC_PIDTBSDINFO,0,&info,sizeof(info));
                const bool stopped=size==sizeof(info)?info.pbi_status==SZOMB:(kill(pid_t(pid),0)!=0&&errno==ESRCH);
                if(!stopped)fail(QStringLiteral("실행 중인 업데이트 요청자를 확인할 수 없습니다."));
            }
            for(int i=0;stamp&&i<2400&&processStamp(pid,exe)==stamp;++i)QThread::msleep(50);
            if(stamp&&processStamp(pid,exe)==stamp)fail(QStringLiteral("실행 중인 런처가 종료되지 않았습니다."));
#endif
            }else{folder=readJson(queued)["candidate"].toString();}
            auto previous=store.state("launcher")["current"].toString();
            writeJson(pending,{{"previous",previous},{"candidate",folder}});
            auto directory=root+"/launcher/versions/"+folder;
            if(!verifyTree(directory,readJson(directory+"/._teamhjd-receipt.json")))fail(QStringLiteral("새 런처 파일을 확인하지 못했습니다."));
            if(args.contains("--runtime-root"))beginBootstrapUpdate(root,directory);
            const bool changing=folder!=previous;
            if(changing)store.activate("launcher",folder);
            QFile::remove(queued);
            if(launchHealthy(root,store,dataRoot)){commitBootstrapUpdate(root);QFile::remove(pending);QFile::remove(root+"/launcher/failed-release.json");return 0;}
            recoverBootstrapUpdate(root);if(changing)store.rollback("launcher");QFile::remove(pending);
            writeJson(root+"/launcher/failed-release.json",{{"version",readJson(directory+"/._teamhjd-receipt.json")["version"]},{"reason","health"},{"at",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}});
            if(launchHealthy(root,store,dataRoot)){if(!args.contains("--noninteractive-test"))showMessage(QStringLiteral("새 런처를 시작하지 못해 이전 버전으로 복원했습니다."));return 0;}
            fail(QStringLiteral("런처를 시작하지 못했습니다. 설치 폴더를 확인하세요."));
        }
        if(launchHealthy(root,store,dataRoot))return 0;
        if(!store.state("launcher")["previous"].toString().isEmpty()){
            auto failed=store.currentReceipt("launcher")["version"].toString();store.rollback("launcher");
            writeJson(root+"/launcher/failed-release.json",{{"version",failed},{"reason","health"},{"at",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}});
            if(launchHealthy(root,store,dataRoot))return 0;
        }
        fail(QStringLiteral("런처 파일을 확인하지 못했습니다. 공식 배포 ZIP으로 복구하세요."));
    }catch(const std::exception &e){auto message=QString::fromUtf8(e.what());QTextStream(stderr)<<message<<'\n';if(!args.contains("--noninteractive-test"))showMessage(message,true);return 1;}
}
