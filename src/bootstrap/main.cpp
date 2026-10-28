#include "core/Core.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QProcess>
#include <QThread>
#include <QUuid>
#include <windows.h>

using namespace hjd;
namespace {
bool launchHealthy(const QString &root,Store &store) {
    auto directory=store.currentDirectory("launcher");auto receipt=store.currentReceipt("launcher");
    if(receipt["executable"].toString()!="TeamHJDClient.exe"||!verifyTree(directory,receipt))return false;
    auto health=root+"/health/"+QUuid::createUuid().toString(QUuid::Id128)+".json";
    QDir().mkpath(root+"/health");
    auto exe=directory+"/TeamHJDClient.exe";qint64 pid=0;
    if(!QProcess::startDetached(exe,{"--launcher-root",root,"--health-file",health},directory,&pid))return false;
    HANDLE child=OpenProcess(SYNCHRONIZE|PROCESS_TERMINATE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,DWORD(pid));
    bool healthy=false;
    for(int i=0;i<400;++i){
        if(QFileInfo::exists(health)){try{healthy=readJson(health)["pid"].toInteger()==pid;}catch(const Error&){}if(healthy)break;}
        if(child&&WaitForSingleObject(child,0)!=WAIT_TIMEOUT)break;QThread::msleep(50);
    }
    // This handle refers only to the child just started, never a game or unrelated app.
    if(child){if(!healthy&&WaitForSingleObject(child,0)==WAIT_TIMEOUT){TerminateProcess(child,5);WaitForSingleObject(child,3000);}CloseHandle(child);}
    if(healthy)writeJson(root+"/launcher/running.json",{{"pid",pid},{"stamp",QString::number(processStamp(pid,exe))},{"folder",store.state("launcher")["current"]},{"executable","TeamHJDClient.exe"}});
    QFile::remove(health);return healthy;
}
}
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);auto root=QCoreApplication::applicationDirPath();Store store(root);auto args=app.arguments();
    try{
        QLockFile lock(root+"/bootstrap.lock");if(!lock.tryLock(0))return 2;
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
            auto pid=args[waitIndex+1].toLongLong();HANDLE parent=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,DWORD(pid));
            if(parent){auto exe=store.currentDirectory("launcher")+"/TeamHJDClient.exe";if(!processStamp(pid,exe)){CloseHandle(parent);fail(QStringLiteral("업데이트를 요청한 프로그램을 확인할 수 없습니다."));}auto result=WaitForSingleObject(parent,120000);CloseHandle(parent);if(result!=WAIT_OBJECT_0)fail(QStringLiteral("실행 중인 런처가 종료되지 않았습니다."));}
            }else{folder=readJson(queued)["candidate"].toString();}
            auto previous=store.state("launcher")["current"].toString();
            writeJson(pending,{{"previous",previous},{"candidate",folder}});
            store.activate("launcher",folder);
            QFile::remove(queued);
            if(launchHealthy(root,store)){QFile::remove(pending);return 0;}
            store.rollback("launcher");QFile::remove(pending);
            if(launchHealthy(root,store)){if(!args.contains("--noninteractive-test"))MessageBoxW(nullptr,L"새 런처를 시작하지 못해 이전 버전으로 복원했습니다.",L"TeamHJD 업데이트 복구",MB_OK|MB_ICONINFORMATION);return 0;}
            fail(QStringLiteral("런처를 시작하지 못했습니다. 설치 폴더를 확인하세요."));
        }
        if(launchHealthy(root,store))return 0;
        if(!store.state("launcher")["previous"].toString().isEmpty()){store.rollback("launcher");if(launchHealthy(root,store))return 0;}
        fail(QStringLiteral("런처 파일을 확인하지 못했습니다. 공식 배포 ZIP으로 복구하세요."));
    }catch(const std::exception &e){auto message=QString::fromUtf8(e.what());if(!args.contains("--noninteractive-test"))MessageBoxW(nullptr,reinterpret_cast<LPCWSTR>(message.utf16()),L"TeamHJD Game Test Launcher",MB_OK|MB_ICONERROR);return 1;}
}
