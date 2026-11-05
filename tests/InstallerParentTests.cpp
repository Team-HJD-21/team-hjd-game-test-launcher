#include "bootstrap/Installer.h"
#include "core/Core.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QProcess>
#include <QThread>
#include <QTextStream>
#include <windows.h>
using namespace hjd;
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);
    try{
        QTemporaryDir temp;const auto root=temp.path()+"/TeamHJD Game Test Launcher";
        const auto slot=root+"/launcher/versions/1.0.0";QDir().mkpath(slot);
        const auto exe=slot+"/TeamHJDClient.exe";
        if(!QFile::copy(QCoreApplication::applicationDirPath()+"/LauncherFixtureGame.exe",exe))fail("parent fixture copy");
        writeJson(root+"/launcher/active.json",{{"current","1.0.0"}});
        QProcess child;child.setProgram(exe);child.setWorkingDirectory(slot);child.start();
        if(!child.waitForStarted())fail("parent fixture start");
        const auto pid=child.processId();const auto stamp=processStamp(pid,exe);
        writeJson(slot+"/._teamhjd-receipt.json",{{"version","1.0.0"},{"executable","TeamHJDClient.exe"},{"files",QJsonArray{QJsonObject{{"path","TeamHJDClient.exe"},{"bytes",QFileInfo(exe).size()},{"sha256",fileHash(exe)}}}}});
        writeJson(root+"/launcher/running.json",{{"pid",pid},{"stamp",QString::number(stamp)},{"folder","1.0.0"},{"executable","TeamHJDClient.exe"}});
        auto retained=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,DWORD(pid));
        if(!retained||!stamp)fail("parent fixture handle");
        child.kill();if(!child.waitForFinished()||WaitForSingleObject(retained,1000)!=WAIT_OBJECT_0)fail("parent fixture exit");
        if(processStamp(pid,exe)!=0)fail("liveness helper accepted a terminated process");
        wchar_t image[32768];DWORD length=32768;FILETIME created{},exited{},kernel{},user{};
        const bool imageKnown=QueryFullProcessImageNameW(retained,0,image,&length);const auto imageError=GetLastError();
        const bool timeKnown=GetProcessTimes(retained,&created,&exited,&kernel,&user);
        QTextStream(stdout)<<"retained image="<<imageKnown<<" imageError="<<imageError<<" pathMatch="<<(imageKnown&&QDir::cleanPath(QString::fromWCharArray(image,int(length))).compare(QDir::cleanPath(exe),Qt::CaseInsensitive)==0)<<" time="<<timeKnown<<" stampMatch="<<(((quint64(created.dwHighDateTime)<<32)|created.dwLowDateTime)==stamp)<<'\n';
        const QStringList args{"--wait-client",root,"--wait-pid",QString::number(pid),"--wait-stamp",QString::number(stamp)};
        if(!handleInstaller(args,{}))fail("terminated retained parent rejected");
        if(!QFile::rename(root+"/launcher/running.json",root+"/launcher/running.saved"))fail("record fixture rename");
        bool missingRejected=false;try{handleInstaller(args,{});}catch(const Error&){missingRejected=true;}
        if(!missingRejected)fail("terminated parent without owned bootstrap record accepted");
        if(!QFile::rename(root+"/launcher/running.saved",root+"/launcher/running.json"))fail("record fixture restore");
        bool rejected=false;auto wrong=args;wrong.last()=QString::number(stamp+1);
        try{handleInstaller(wrong,{});}catch(const Error&){rejected=true;}
        if(!rejected)fail("wrong creation time accepted");
        writeJson(root+"/launcher/active.json",{{"current","9.0.0"}});rejected=false;
        try{handleInstaller(args,{});}catch(const Error&){rejected=true;}
        if(!rejected)fail("different executable path accepted");
        CloseHandle(retained);
        QTextStream(stdout)<<"PASS exited retained parent identity accepted; wrong stamp and image rejected; liveness semantics unchanged.\n";return 0;
    }catch(const std::exception &e){QTextStream(stderr)<<e.what()<<'\n';return 1;}
}
