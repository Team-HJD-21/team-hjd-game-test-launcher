#include "core/Core.h"
#include "core/Platform.h"
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
#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <CoreFoundation/CoreFoundation.h>
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
    if(QFileInfo::exists(root+"/launcher/active.json"))return;
    const auto seed=QDir::cleanPath(QCoreApplication::applicationDirPath()+"/../Resources/seed");
    auto receipt=readJson(seed+"/._teamhjd-receipt.json");
    if(receipt["executable"].toString()!=clientExecutable()||!verifyTree(seed,receipt))fail(QStringLiteral("런처 설치 파일을 확인하지 못했습니다. 공식 ZIP을 다시 받아 주세요."));
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
    Store(root).activate("launcher",folder);
}
#endif
bool launchHealthy(const QString &root,Store &store) {
    auto directory=store.currentDirectory("launcher");auto receipt=store.currentReceipt("launcher");
    if(receipt["executable"].toString()!=clientExecutable()||!verifyTree(directory,receipt))return false;
    auto health=root+"/health/"+QUuid::createUuid().toString(QUuid::Id128)+".json";
    QDir().mkpath(root+"/health");
    auto exe=directory+"/"+clientExecutable();qint64 pid=0;
    if(!QProcess::startDetached(exe,{"--launcher-root",root,"--health-file",health},directory,&pid))return false;
#ifdef Q_OS_WIN
    HANDLE child=OpenProcess(SYNCHRONIZE|PROCESS_TERMINATE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,DWORD(pid));
#else
    auto childStamp=processStamp(pid,exe);
#endif
    bool healthy=false;
    for(int i=0;i<400;++i){
        if(QFileInfo::exists(health)){try{healthy=readJson(health)["pid"].toInteger()==pid;}catch(const Error&){}if(healthy)break;}
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
    Store store(root);auto args=app.arguments();
    try{
        QDir().mkpath(root);
        QLockFile lock(root+"/bootstrap.lock");if(!lock.tryLock(0))return 2;
#ifdef Q_OS_MACOS
        seedRuntime(root);
#endif
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
#ifdef Q_OS_WIN
            HANDLE parent=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,DWORD(pid));
            if(parent){auto exe=store.currentDirectory("launcher")+"/"+clientExecutable();if(!processStamp(pid,exe)){CloseHandle(parent);fail(QStringLiteral("업데이트를 요청한 프로그램을 확인할 수 없습니다."));}auto result=WaitForSingleObject(parent,120000);CloseHandle(parent);if(result!=WAIT_OBJECT_0)fail(QStringLiteral("실행 중인 런처가 종료되지 않았습니다."));}
#else
            const auto exe=store.currentDirectory("launcher")+"/"+clientExecutable();auto stamp=processStamp(pid,exe);
            if(pid<=0||!stamp)fail(QStringLiteral("업데이트를 요청한 프로그램을 확인할 수 없습니다."));
            for(int i=0;i<2400&&processStamp(pid,exe)==stamp;++i)QThread::msleep(50);
            if(processStamp(pid,exe)==stamp)fail(QStringLiteral("실행 중인 런처가 종료되지 않았습니다."));
#endif
            }else{folder=readJson(queued)["candidate"].toString();}
            auto previous=store.state("launcher")["current"].toString();
            writeJson(pending,{{"previous",previous},{"candidate",folder}});
            store.activate("launcher",folder);
            QFile::remove(queued);
            if(launchHealthy(root,store)){QFile::remove(pending);return 0;}
            store.rollback("launcher");QFile::remove(pending);
            if(launchHealthy(root,store)){if(!args.contains("--noninteractive-test"))showMessage(QStringLiteral("새 런처를 시작하지 못해 이전 버전으로 복원했습니다."));return 0;}
            fail(QStringLiteral("런처를 시작하지 못했습니다. 설치 폴더를 확인하세요."));
        }
        if(launchHealthy(root,store))return 0;
        if(!store.state("launcher")["previous"].toString().isEmpty()){store.rollback("launcher");if(launchHealthy(root,store))return 0;}
        fail(QStringLiteral("런처 파일을 확인하지 못했습니다. 공식 배포 ZIP으로 복구하세요."));
    }catch(const std::exception &e){auto message=QString::fromUtf8(e.what());QTextStream(stderr)<<message<<'\n';if(!args.contains("--noninteractive-test"))showMessage(message,true);return 1;}
}
