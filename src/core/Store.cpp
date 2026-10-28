#include "Core.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QProcess>
#include <QRegularExpression>
#include <QUuid>

namespace hjd {
static bool folderName(const QString &s) { static QRegularExpression r("^[0-9][A-Za-z0-9._-]{0,160}$"); return r.match(s).hasMatch()&&!s.endsWith('.'); }
Store::Store(QString root):m_root(QDir::cleanPath(QFileInfo(root).absoluteFilePath())) { noLinks(m_root); }
QString Store::gameRoot(const QString &id) const { if(!safeId(id)) fail(QStringLiteral("게임 식별자가 올바르지 않습니다.")); return QDir(m_root).filePath(id); }
QJsonObject Store::state(const QString &id) const { auto base=gameRoot(id);auto pending=base+"/uninstall-pending.json";if(QFileInfo::exists(pending)){auto trash=readJson(pending)["trash"].toString();static QRegularExpression valid("^removing-[a-f0-9]{32}$");if(!valid.match(trash).hasMatch())fail(QStringLiteral("삭제 복구 정보가 올바르지 않습니다."));if(QFileInfo::exists(base+"/"+trash))return {};}auto p=base+"/active.json"; if(!QFileInfo::exists(p)) return {}; return readJson(p); }
QString Store::currentDirectory(const QString &id) const { auto s=state(id)["current"].toString(); if(s.isEmpty()) return {}; if(!folderName(s)) fail(QStringLiteral("설치 정보가 손상되었습니다.")); return gameRoot(id)+"/versions/"+s; }
QJsonObject Store::currentReceipt(const QString &id) const { auto p=currentDirectory(id); return p.isEmpty()?QJsonObject{}:readJson(p+"/._teamhjd-receipt.json"); }
bool Store::running(const QString &id) const {
    auto p=gameRoot(id)+"/running.json"; if(!QFileInfo::exists(p)) return false;
    auto o=readJson(p); auto dir=o["folder"].toString(); auto exe=o["executable"].toString();
    if(!folderName(dir)||!safePath(exe)) fail(QStringLiteral("실행 정보가 손상되었습니다."));
    auto stamp=processStamp(o["pid"].toInteger(),gameRoot(id)+"/versions/"+dir+"/"+exe);
    return stamp!=0&&QString::number(stamp)==o["stamp"].toString();
}
bool Store::verify(const QString &id,std::atomic_bool *cancel) const { auto d=currentDirectory(id); if(d.isEmpty()) return false; return verifyTree(d,currentReceipt(id),cancel); }
void Store::activate(const QString &id,const QString &folder) {
    if(!folderName(folder)) fail(QStringLiteral("새 버전 정보가 올바르지 않습니다."));
    if(running(id)) fail(QStringLiteral("게임을 종료한 뒤 업데이트하세요."));
    auto dir=gameRoot(id)+"/versions/"+folder; auto receipt=readJson(dir+"/._teamhjd-receipt.json");
    auto exe=receipt["executable"].toString();
    if(!safePath(exe)||!QFileInfo::exists(dir+"/"+exe)) fail(QStringLiteral("실행 파일이 없는 버전으로 전환할 수 없습니다."));
    auto old=state(id); if(old["current"].toString()==folder) return;
    writeJson(gameRoot(id)+"/active.json",{{"current",folder},{"previous",old["current"]}});
}
void Store::install(const Game &g,const QString &archive,std::atomic_bool &cancel,const Progress &progress,bool repair) {
    auto base=gameRoot(g.id); noLinks(base); QDir().mkpath(base+"/versions"); QLockFile lock(base+"/install.lock");
    if(!lock.tryLock(0)) fail(QStringLiteral("다른 설치 작업이 진행 중입니다."));
    if(QFileInfo::exists(base+"/uninstall-pending.json"))fail(QStringLiteral("이전 게임 삭제를 마친 뒤 다시 설치하세요. 게임 관리에서 삭제를 다시 선택할 수 있습니다."));
    if(running(g.id)) fail(QStringLiteral("게임을 종료한 뒤 업데이트하세요."));
    auto current=currentReceipt(g.id);
    QMap<QString,QJsonObject> recorded;for(auto v:current["files"].toArray())recorded.insert(v.toObject()["path"].toString(),v.toObject());bool sameFiles=recorded.size()==g.files.size();
    for(const auto &f:g.files)if(recorded[f.path]["sha256"].toString()!=f.sha256||recorded[f.path]["bytes"].toInteger(-1)!=f.bytes)sameFiles=false;
    if(!repair&&sameFiles&&current["archiveSha256"].toString()==g.sha256&&current["version"].toString()==g.version&&verifyTree(currentDirectory(g.id),current,&cancel))return;
    if(QFileInfo(archive).size()!=g.bytes||fileHash(archive,&cancel)!=g.sha256) fail(QStringLiteral("설치 파일의 검증에 실패했습니다."));
    const auto folder=g.version+"-"+g.sha256.left(12)+"-"+QUuid::createUuid().toString(QUuid::Id128).left(8);
    const auto stage=base+"/versions/"+folder;
    auto receipt=inspectArchive(archive,stage,g.files,cancel,progress);
    receipt["id"]=g.id;receipt["name"]=g.name;receipt["description"]=g.description;receipt["notes"]=g.notes;receipt["installedAt"]=QDateTime::currentDateTimeUtc().toString(Qt::ISODate); receipt["version"]=g.version; receipt["executable"]=g.executable; receipt["archiveSha256"]=g.sha256;
    writeJson(stage+"/._teamhjd-receipt.json",receipt);
    if(cancel) fail(QStringLiteral("취소했습니다."));
    activate(g.id,folder);
}
void Store::uninstall(const QString &id){
    if(id=="launcher")fail(QStringLiteral("런처 자체는 게임 삭제 대상으로 지정할 수 없습니다."));
    auto base=gameRoot(id);noLinks(base);if(!QFileInfo::exists(base))return;
    QLockFile lock(base+"/install.lock");if(!lock.tryLock(0))fail(QStringLiteral("설치 작업을 마친 뒤 삭제하세요."));
    if(running(id))fail(QStringLiteral("게임을 종료한 뒤 삭제하세요."));
    auto marker=base+"/uninstall-pending.json";QString trash;
    if(QFileInfo::exists(marker)){trash=readJson(marker)["trash"].toString();static QRegularExpression valid("^removing-[a-f0-9]{32}$");if(!valid.match(trash).hasMatch())fail(QStringLiteral("삭제 복구 정보가 올바르지 않습니다."));}
    else trash="removing-"+QUuid::createUuid().toString(QUuid::Id128);
    auto versions=base+"/versions",target=base+"/"+trash;
    auto preflight=[](const QString &directory){noLinks(directory);QDirIterator it(directory,QDir::AllEntries|QDir::NoDotAndDotDot|QDir::Hidden|QDir::System,QDirIterator::Subdirectories);while(it.hasNext())noLinks(it.next());};
    if(!QFileInfo::exists(target)&&QFileInfo::exists(versions)){
        preflight(versions);
        static QRegularExpression managed("^[0-9][A-Za-z0-9._-]*-[a-f0-9]{12}-[a-f0-9]{8}$");
        for(const auto &entry:QDir(versions).entryInfoList(QDir::AllEntries|QDir::NoDotAndDotDot|QDir::Hidden|QDir::System))if(!entry.isDir()||!managed.match(entry.fileName()).hasMatch())fail(QStringLiteral("게임 버전 폴더에 별도 파일이 있습니다. 설치 폴더를 확인하세요."));
        writeJson(marker,{{"trash",trash}});
        if(!QDir().rename(versions,target))fail(QStringLiteral("게임 파일을 삭제 준비할 수 없습니다. 실행 중인 프로그램을 확인하세요."));
    }
    writeJson(base+"/active.json",{});
    if(QFileInfo::exists(target)){preflight(target);if(!QDir(target).removeRecursively())fail(QStringLiteral("일부 게임 파일을 삭제하지 못했습니다. 게임 삭제를 다시 선택하세요."));}
    QFile::remove(base+"/running.json");QFile::remove(marker);
}
void Store::rollback(const QString &id) {
    auto base=gameRoot(id); QLockFile lock(base+"/install.lock"); if(!lock.tryLock(0)) fail(QStringLiteral("설치 작업을 마친 뒤 복원하세요."));
    if(running(id)) fail(QStringLiteral("게임을 종료한 뒤 복원하세요."));
    auto old=state(id); auto previous=old["previous"].toString(); if(previous.isEmpty()||!folderName(previous)) fail(QStringLiteral("복원할 이전 버전이 없습니다."));
    auto directory=base+"/versions/"+previous; auto receipt=readJson(directory+"/._teamhjd-receipt.json");
    if(!verifyTree(directory,receipt)) fail(QStringLiteral("이전 버전의 파일 검증에 실패했습니다."));
    activate(id,previous);
}
qint64 Store::launch(const QString &id) {
    auto base=gameRoot(id); QLockFile lock(base+"/install.lock"); if(!lock.tryLock(0)) fail(QStringLiteral("설치 작업이 진행 중입니다."));
    if(running(id)) fail(QStringLiteral("게임이 이미 실행 중입니다."));
    auto dir=currentDirectory(id); auto receipt=currentReceipt(id); auto exe=receipt["executable"].toString();
    if(!safePath(exe)||!verifyTree(dir,receipt)) fail(QStringLiteral("게임 파일을 확인하지 못했습니다. 파일 복구를 실행하세요."));
    auto path=dir+"/"+exe; qint64 pid=0;
    if(!QProcess::startDetached(path,{},dir,&pid)) fail(QStringLiteral("게임을 시작하지 못했습니다."));
    auto stamp=processStamp(pid,path);
    if(!stamp) fail(QStringLiteral("게임이 시작 직후 종료되었습니다. 테스트 로그를 확인하세요."));
    writeJson(base+"/running.json",{{"pid",pid},{"stamp",QString::number(stamp)},{"folder",state(id)["current"]},{"executable",exe}}); return pid;
}
QString Store::stagePayload(const QString &version,const QString &hash,const QString &archive,std::atomic_bool &cancel) {
    if(!safeVersion(version)||fileHash(archive,&cancel)!=hash) fail(QStringLiteral("런처 업데이트 파일을 검증하지 못했습니다."));
    auto base=gameRoot("launcher"); QDir().mkpath(base+"/versions"); QLockFile lock(base+"/install.lock"); if(!lock.tryLock(0)) fail(QStringLiteral("런처 업데이트가 이미 진행 중입니다."));
    auto folder=version+"-"+hash.left(12)+"-"+QUuid::createUuid().toString(QUuid::Id128).left(8);
    auto dest=base+"/versions/"+folder; auto receipt=inspectArchive(archive,dest,{},cancel);
    if(!QFileInfo::exists(dest+"/TeamHJDClient.exe")) fail(QStringLiteral("런처 실행 파일이 누락되었습니다."));
    receipt["version"]=version; receipt["executable"]="TeamHJDClient.exe"; receipt["archiveSha256"]=hash;
    writeJson(dest+"/._teamhjd-receipt.json",receipt); return folder;
}
}
