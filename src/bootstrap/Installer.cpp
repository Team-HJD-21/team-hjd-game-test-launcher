#include "Installer.h"
#include "core/Core.h"
#include "core/BootstrapUpdate.h"
#include "core/Platform.h"
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QRegularExpression>
#include <QSet>
#include <QVersionNumber>
#include <QStandardPaths>
#include <QTextStream>
#include <QUuid>
#include <algorithm>
#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#endif

using namespace hjd;
namespace {
const QString product="com.teamhjd.game-test-launcher";
QString ownedRoot(const QString &path){
    if(!QDir::isAbsolutePath(path))fail(QStringLiteral("설치 위치는 전체 경로여야 합니다."));
    auto root=QDir::cleanPath(QFileInfo(path).absoluteFilePath());noLinks(root);
    if(QFileInfo(root).fileName()!="TeamHJD Game Test Launcher")fail(QStringLiteral("전용 TeamHJD Game Test Launcher 폴더에 설치해 주세요."));
    return root;
}
void assertIdle(const QString &root){
#ifdef Q_OS_WIN
    auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snapshot==INVALID_HANDLE_VALUE)fail(QStringLiteral("실행 중인 런처를 확인하지 못했습니다."));
    PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);bool found=false;
    if(Process32FirstW(snapshot,&entry))do{
        auto process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,entry.th32ProcessID);
        if(!process)continue;
        wchar_t buffer[32768];DWORD length=32768;
        if(QueryFullProcessImageNameW(process,0,buffer,&length)){
            auto path=QDir::fromNativeSeparators(QString::fromWCharArray(buffer,int(length)));
            if(path.startsWith(root+"/",Qt::CaseInsensitive))found=true;
        }
        CloseHandle(process);
    }while(Process32NextW(snapshot,&entry));
    CloseHandle(snapshot);
    if(found)fail(QStringLiteral("해당 설치 폴더에서 실행 중인 런처·게임을 종료한 뒤 다시 시도해 주세요. 게임과 설정은 유지됩니다."));
#endif
#ifndef Q_OS_WIN
    if(Store(root).running("launcher"))fail(QStringLiteral("실행 중인 런처를 먼저 종료해 주세요."));
#endif
}
void copy(const QString &from,const QString &to){
    noLinks(from);noLinks(to);atomicWrite(to,readFile(from,128ll*1024*1024));
    if(!QFile::setPermissions(to,QFileInfo(from).permissions())||fileHash(from)!=fileHash(to))fail(QStringLiteral("설치 파일 복사 검증에 실패했습니다."));
}
void install(const QString &root,const QString &profile,bool requireHealth,const InstallHealthCheck &health){
    const auto marker=root+"/teamhjd-install.json";
    QJsonObject oldMarker;
    if(QFileInfo::exists(marker)){
        oldMarker=readJson(marker);
        if(oldMarker["product"].toString()!=product)fail(QStringLiteral("다른 프로그램의 설치 위치입니다."));
    }else if(QFileInfo::exists(root)&&!QDir(root).entryList(QDir::AllEntries|QDir::Hidden|QDir::System|QDir::NoDotAndDotDot).isEmpty())fail(QStringLiteral("이 위치에 기존 파일이 있습니다. 전용 설치 폴더를 사용해 주세요."));
    assertIdle(root);QDir().mkpath(root);QLockFile lock(root+"/bootstrap.lock");
    if(!lock.tryLock(0))fail(QStringLiteral("다른 설치 또는 업데이트가 진행 중입니다."));
    std::unique_ptr<QLockFile> profileLock;
    if(QFileInfo(profile).isDir()){
        profileLock=std::make_unique<QLockFile>(profile+"/client.lock");
        if(!profileLock->tryLock(0))fail(QStringLiteral("기존 런처를 먼저 종료해 주세요. 기존 게임과 설정은 보존됩니다."));
    }
    QString installed;
    try{installed=Store(root).currentReceipt("launcher")["version"].toString();}
    catch(const Error&){
        // A repair must preserve damaged metadata for diagnosis and avoid
        // trusting its pointers. Process/profile ownership was checked above.
        const auto active=root+"/launcher/active.json";
        if(QFileInfo::exists(active)){
            const auto preserved=root+"/launcher/repair-active-"+QUuid::createUuid().toString(QUuid::Id128)+".json";
            noLinks(active);if(!QFile::rename(active,preserved))fail(QStringLiteral("손상된 설치 정보를 보존하지 못했습니다."));
        }
    }
    if(!installed.isEmpty()&&QVersionNumber::fromString(installed)>QVersionNumber::fromString(QLatin1String(HJD_VERSION)))fail(QStringLiteral("더 새로운 런처가 설치되어 있습니다. 최신 설치 프로그램을 사용해 주세요."));
    const auto source=QCoreApplication::applicationDirPath()+"/launcher/versions/"+QLatin1String(HJD_VERSION);
    const auto receipt=readJson(source+"/._teamhjd-receipt.json");
    if(receipt["version"].toString()!=QLatin1String(HJD_VERSION)||receipt["executable"].toString()!=clientExecutable()||!verifyTree(source,receipt))fail(QStringLiteral("설치 패키지 검증에 실패했습니다. 공식 파일을 다시 받아 주세요."));
    if(fileHash(bootstrapExecutable(source))!=fileHash(QCoreApplication::applicationFilePath()))fail(QStringLiteral("설치 실행기와 패키지 버전이 다릅니다."));
    writeJson(marker,{{"schema",1},{"product",product},{"version",HJD_VERSION},{"preserveUserData",true}});
    const auto folder=QString::fromLatin1(HJD_VERSION)+"-install-"+QUuid::createUuid().toString(QUuid::Id128).left(8);
    const auto destination=root+"/launcher/versions/"+folder;
    for(const auto &value:receipt["files"].toArray()){
        const auto file=value.toObject();const auto relative=file["path"].toString();
        if(!safePath(relative))fail(QStringLiteral("설치 파일 경로가 올바르지 않습니다."));
        copy(source+"/"+relative,destination+"/"+relative);
    }
    writeJson(destination+"/._teamhjd-receipt.json",receipt);
    if(!verifyTree(destination,receipt))fail(QStringLiteral("복사한 런처 검증에 실패했습니다."));
    Store store(root);const auto previous=store.state("launcher")["current"].toString();
    recoverBootstrapUpdate(root);beginBootstrapUpdate(root,destination);
    store.activate("launcher",folder);
    if(profileLock)profileLock->unlock();
    if(requireHealth&&!health(root,profile)){
        recoverBootstrapUpdate(root);
        if(!previous.isEmpty())store.rollback("launcher");
        if(!oldMarker.isEmpty())writeJson(marker,oldMarker);
        writeJson(root+"/launcher/failed-release.json",{{"version",HJD_VERSION},{"reason","health"},{"at",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}});
        if(!previous.isEmpty())health(root,profile);
        fail(QStringLiteral("새 런처를 시작하지 못해 이전 버전으로 복원했습니다. 공식 설치 파일로 복구하거나 새 패치를 기다려 주세요."));
    }
    commitBootstrapUpdate(root);QFile::remove(root+"/launcher/failed-release.json");
    writeJson(root+"/bootstrap-location.json",{{"executable",root+"/TeamHJDLauncher.exe"}});
    QTextStream(stdout)<<"PASS launcher installed; existing profile, games, saves and server records preserved.\n";
}
void removeFiles(const QString &root,const QJsonArray &files){
    QSet<QString> directories;
    for(const auto &value:files){const auto relative=value.toObject()["path"].toString();if(!safePath(relative))fail(QStringLiteral("제거할 파일 경로가 올바르지 않습니다."));noLinks(root+"/"+relative);}
    for(const auto &value:files){
        const auto path=root+"/"+value.toObject()["path"].toString();
        if(QFileInfo::exists(path)&&!QFile::remove(path))fail(QStringLiteral("파일을 제거하지 못했습니다. 런처를 종료한 뒤 다시 시도해 주세요."));
        auto parent=QFileInfo(path).absolutePath();while(parent.startsWith(root+"/")){directories.insert(parent);parent=QFileInfo(parent).absolutePath();}
    }
    auto sorted=directories.values();std::sort(sorted.begin(),sorted.end(),[](const auto &a,const auto &b){return a.size()>b.size();});
    for(const auto &directory:sorted)QDir().rmdir(directory);
}
void uninstall(const QString &root){
    const auto marker=root+"/teamhjd-install.json";
    if(readJson(marker)["product"].toString()!=product)fail(QStringLiteral("이 프로그램이 설치한 폴더인지 확인하지 못했습니다."));
    assertIdle(root);QLockFile lock(root+"/bootstrap.lock");if(!lock.tryLock(0))fail(QStringLiteral("다른 설치 또는 업데이트가 진행 중입니다."));
    recoverBootstrapUpdate(root);
    const auto versions=root+"/launcher/versions";
    const auto folders=QDir(versions).entryList(QDir::Dirs|QDir::NoDotAndDotDot);
    QList<QPair<QString,QJsonArray>> owned;
    for(const auto &folder:folders){
        const auto directory=versions+"/"+folder;noLinks(directory);
        if(!safeVersion(folder)||!QFileInfo::exists(directory+"/._teamhjd-receipt.json"))continue;
        QJsonObject receipt;
        try{receipt=readJson(directory+"/._teamhjd-receipt.json");}catch(const Error&){continue;}
        if(receipt["executable"].toString()!=clientExecutable())continue;
        bool valid=true;
        try{for(const auto &value:receipt["files"].toArray()){auto relative=value.toObject()["path"].toString();if(!safePath(relative)){valid=false;break;}noLinks(directory+"/"+relative);}}catch(const Error&){valid=false;}
        if(!valid)continue; // Preserve an untrusted slot; never follow its paths.
        owned.append({directory,receipt["files"].toArray()});
    }
    QJsonArray bootstrap;
    try{bootstrap=readJson(root+"/bootstrap-installed.json")["files"].toArray();}catch(const Error&){}
    if(bootstrap.isEmpty()){
        // Interrupted removals may already have deleted their manifest. Limit
        // fallback cleanup to known program binaries and bundled license names.
        for(const auto &name:QStringList{"TeamHJDLauncher.exe","Qt6Core.dll","concrt140.dll","msvcp140.dll","msvcp140_1.dll","msvcp140_2.dll","msvcp140_atomic_wait.dll","msvcp140_codecvt_ids.dll","vccorlib140.dll","vcruntime140.dll","vcruntime140_1.dll","vcruntime140_threads.dll","THIRD_PARTY_NOTICES.md"})bootstrap.append(QJsonObject{{"path",name}});
        auto licenses=QCoreApplication::applicationDirPath()+"/licenses";
        QDirIterator it(licenses,QDir::Files,QDirIterator::Subdirectories);
        while(it.hasNext()){auto path=it.next();noLinks(path);bootstrap.append(QJsonObject{{"path","licenses/"+QDir(licenses).relativeFilePath(path)}});}
    }
    for(const auto &value:bootstrap){auto relative=value.toObject()["path"].toString();if(!safePath(relative))fail(QStringLiteral("실행기 제거 경로가 올바르지 않습니다."));noLinks(root+"/"+relative);}
    for(const auto &slot:owned){removeFiles(slot.first,slot.second);QFile::remove(slot.first+"/._teamhjd-receipt.json");QDir().rmdir(slot.first);}
    removeFiles(root,bootstrap);
    for(const auto &name:QStringList{"launcher/active.json","launcher/running.json","queued-launcher.json","pending-launcher.json","bootstrap-location.json","bootstrap-installed.json"}){
        noLinks(root+"/"+name);QFile::remove(root+"/"+name);
    }
    // Evidence consists only of publisher-signed public manifests. Unknown
    // files, user data and external game paths are deliberately preserved.
    const auto evidence=root+"/launcher/update-evidence";
    for(const auto &name:QDir(evidence).entryList({"*.json"},QDir::Files)){
        if(!QRegularExpression("^[0-9][A-Za-z0-9._-]{0,160}\\.json$").match(name).hasMatch())continue;
        noLinks(evidence+"/"+name);QFile::remove(evidence+"/"+name);
    }
    QDir().rmdir(evidence);QDir().rmdir(versions);QDir().rmdir(root+"/launcher");noLinks(root+"/health");QDir().rmdir(root+"/health");
    auto ownership=readJson(marker);ownership["state"]="removed";ownership["preserveUserData"]=true;
    writeJson(marker,ownership); // NSIS removes this last, after its registration.
    QTextStream(stdout)<<"PASS launcher removed; profile, games, Unity saves/logs and server records preserved.\n";
}
}
bool handleInstaller(const QStringList &arguments,const InstallHealthCheck &health){
    auto option=[&](const QString &name){auto index=arguments.indexOf(name);if(index<0)return QString();if(index+1>=arguments.size())fail(QStringLiteral("설치 인수가 누락되었습니다."));return ownedRoot(arguments[index+1]);};
    auto target=option("--install-to"),remove=option("--uninstall-from"),wait=option("--wait-client");
    if(!wait.isEmpty()){
#ifdef Q_OS_WIN
        auto index=arguments.indexOf("--wait-pid"),stampIndex=arguments.indexOf("--wait-stamp");
        if(index<0||index+1>=arguments.size()||stampIndex<0||stampIndex+1>=arguments.size())fail(QStringLiteral("업데이트 요청자 정보가 없습니다."));
        bool valid=false;auto pid=arguments[index+1].toULong(&valid);
        if(!valid||pid==0)fail(QStringLiteral("업데이트 프로세스 번호가 올바르지 않습니다."));
        auto parent=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
        if(parent){
            auto expected=Store(wait).currentDirectory("launcher")+"/"+clientExecutable();
            // Keep identity separate from liveness: a terminated process can
            // remain queryable while its creator retains a handle. Validate
            // the exact image and creation time on this one retained handle.
            wchar_t image[32768];DWORD length=32768;
            FILETIME created{},exited{},kernel{},user{};
            const bool imageKnown=QueryFullProcessImageNameW(parent,0,image,&length);
            const bool timeKnown=GetProcessTimes(parent,&created,&exited,&kernel,&user);
            const auto stamp=(quint64(created.dwHighDateTime)<<32)|created.dwLowDateTime;
            bool imageMatches=imageKnown&&QDir::cleanPath(QString::fromWCharArray(image,int(length))).compare(QDir::cleanPath(expected),Qt::CaseInsensitive)==0;
            if(!imageKnown&&timeKnown&&WaitForSingleObject(parent,0)==WAIT_OBJECT_0){
                // Windows can discard image metadata before the last handle
                // closes. Only an already terminated object may use the exact
                // bootstrap record, OS creation time and verified installed
                // tree. This never authorizes an unknown live process.
                const auto recordPath=wait+"/launcher/running.json";
                if(QFileInfo::exists(recordPath)){
                    const auto record=readJson(recordPath);Store installed(wait);
                    imageMatches=record["pid"].toInteger()==pid&&record["stamp"].toString()==arguments[stampIndex+1]&&record["folder"]==installed.state("launcher")["current"]&&record["executable"].toString()==clientExecutable()&&installed.currentReceipt("launcher")["executable"].toString()==clientExecutable()&&installed.verify("launcher");
                }
            }
            if(!timeKnown||!imageMatches||QString::number(stamp)!=arguments[stampIndex+1]){CloseHandle(parent);fail(QStringLiteral("업데이트를 요청한 런처를 확인하지 못했습니다."));}
            auto finished=WaitForSingleObject(parent,120000);CloseHandle(parent);
            if(finished!=WAIT_OBJECT_0)fail(QStringLiteral("실행 중인 런처가 종료되지 않았습니다."));
        }else if(GetLastError()!=ERROR_INVALID_PARAMETER)fail(QStringLiteral("업데이트 요청자를 확인할 수 없습니다."));
        return true;
#else
        fail(QStringLiteral("지원하지 않는 설치 방식입니다."));
#endif
    }
    if(!target.isEmpty()&&!remove.isEmpty())fail(QStringLiteral("설치와 제거를 동시에 실행할 수 없습니다."));
    if(target.isEmpty()&&remove.isEmpty())return false;
#ifdef Q_OS_MACOS
    fail(QStringLiteral("Mac에서는 DMG의 앱을 응용 프로그램 폴더로 옮겨 설치하세요."));
#else
    QString profile=QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/TeamHJD/Launcher";
    auto index=arguments.indexOf("--data-root");
    if(index>=0){if(index+1>=arguments.size()||!QDir::isAbsolutePath(arguments[index+1]))fail(QStringLiteral("데이터 위치가 올바르지 않습니다."));profile=QDir::cleanPath(arguments[index+1]);noLinks(profile);}
    if(!target.isEmpty())install(target,profile,arguments.contains("--require-health"),health);else uninstall(remove);return true;
#endif
}
