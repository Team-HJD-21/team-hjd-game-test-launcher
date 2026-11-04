#include "BootstrapUpdate.h"
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QProcess>
#include <QUuid>

namespace hjd {
namespace {
void copy(const QString &from,const QString &to){
    noLinks(from);noLinks(to);atomicWrite(to,readFile(from,128ll*1024*1024));
    if(!QFile::setPermissions(to,QFileInfo(from).permissions())||fileHash(from)!=fileHash(to))fail(QStringLiteral("실행기 파일을 안전하게 갱신하지 못했습니다."));
}
void removeBackup(const QString &directory){
    noLinks(directory);QDirIterator it(directory,QDir::AllEntries|QDir::Hidden|QDir::System|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);
    while(it.hasNext())noLinks(it.next());
    if(QFileInfo::exists(directory)&&!QDir(directory).removeRecursively())fail(QStringLiteral("실행기 백업 정리를 완료하지 못했습니다."));
}
QString backupPath(const QString &root,const QJsonObject &journal){
    const auto name=journal["backup"].toString();
    if(!QRegularExpression("^bootstrap-backup-[a-f0-9]{32}$").match(name).hasMatch())fail(QStringLiteral("실행기 복구 기록이 올바르지 않습니다."));
    auto path=root+"/"+name;noLinks(path);return path;
}
QJsonObject manifest(const QString &payload){
    auto receipt=readJson(payload+"/bootstrap-manifest.json");
    if(receipt["version"].toString()!=QLatin1String(HJD_VERSION)||!verifyTree(payload+"/bootstrap",receipt))fail(QStringLiteral("새 실행기 파일 검증에 실패했습니다."));
    return receipt;
}
#ifdef Q_OS_MACOS
QString targetBundle(const QString &root){
    auto path=readJson(root+"/bootstrap-location.json")["executable"].toString();
    if(!QDir::isAbsolutePath(path)||!path.endsWith("/TeamHJDLauncher.app/Contents/MacOS/TeamHJDLauncher"))fail(QStringLiteral("응용 프로그램 폴더의 새 런처를 한 번 실행해 주세요."));
    auto bundle=path.left(path.size()-QStringLiteral("/Contents/MacOS/TeamHJDLauncher").size());noLinks(bundle);return bundle;
}
void validateBundle(const QJsonObject &j){
    const auto target=j["bundle"].toString(),backup=j["bundleBackup"].toString(),prepared=j["bundlePrepared"].toString();
    const auto parent=QFileInfo(target).absolutePath();
    if(!QDir::isAbsolutePath(target)||QFileInfo(target).fileName()!="TeamHJDLauncher.app"||
       QFileInfo(backup).absolutePath()!=parent||QFileInfo(prepared).absolutePath()!=parent||
       !QRegularExpression("^\\.TeamHJDLauncher-backup-[a-f0-9]{32}\\.app$").match(QFileInfo(backup).fileName()).hasMatch()||
       !QRegularExpression("^\\.TeamHJDLauncher-update-[a-f0-9]{32}\\.app$").match(QFileInfo(prepared).fileName()).hasMatch())fail(QStringLiteral("앱 복구 경로가 올바르지 않습니다."));
    noLinks(target);noLinks(backup);noLinks(prepared);
}
#endif
}
QString bootstrapExecutable(const QString &payload){
#ifdef Q_OS_WIN
    return payload+"/bootstrap/TeamHJDLauncher.exe";
#else
    return payload+"/bootstrap/TeamHJDLauncher.app/Contents/MacOS/TeamHJDLauncher";
#endif
}
void rememberBootstrap(const QString &root){
    const auto executable=QDir::cleanPath(QCoreApplication::applicationFilePath());
    const auto normalized=QDir::cleanPath(QFileInfo(root).absoluteFilePath());
    if(!executable.startsWith(normalized+"/launcher/versions/",Qt::CaseInsensitive))writeJson(root+"/bootstrap-location.json",{{"executable",executable}});
}
bool bootstrapCurrent(const QString &root,const QString &payload){
    if(!QFileInfo::exists(payload+"/bootstrap-manifest.json"))return true;
    const auto receipt=manifest(payload);
#ifdef Q_OS_WIN
    const auto destination=root;
#else
    if(!QFileInfo::exists(root+"/bootstrap-location.json"))return true;
    const auto destination=QFileInfo(targetBundle(root)).absolutePath();
    bool seedSignatureDiffers=false;
#endif
    for(const auto &value:receipt["files"].toArray()){
        auto file=value.toObject();auto target=destination+"/"+file["path"].toString();noLinks(target);
        if(!QFileInfo::exists(target))return false;
        if(fileHash(target)!=file["sha256"].toString()){
#ifdef Q_OS_MACOS
            if(file["path"].toString()=="TeamHJDLauncher.app/Contents/_CodeSignature/CodeResources")seedSignatureDiffers=true;
            else return false;
#else
            return false;
#endif
        }
    }
#ifdef Q_OS_MACOS
    if(seedSignatureDiffers){
        // Initial DMGs include a seed, so their resource envelope legitimately
        // differs from the seed-free update app. Verify that complete seed and
        // the actual installed bundle's strict signature instead of comparing
        // two different, valid resource envelopes byte-for-byte.
        const auto bundle=targetBundle(root),seed=bundle+"/Contents/Resources/seed";
        if(!QFileInfo::exists(seed+"/._teamhjd-receipt.json")||!verifyTree(seed,readJson(seed+"/._teamhjd-receipt.json")))return false;
        QProcess signature;signature.start("/usr/bin/codesign",{"--verify","--deep","--strict",bundle});
        if(!signature.waitForFinished(15000)){signature.kill();signature.waitForFinished(3000);return false;}
        if(signature.exitStatus()!=QProcess::NormalExit||signature.exitCode()!=0)return false;
    }
#endif
    return true;
}
void recoverBootstrapUpdate(const QString &root){
    auto path=root+"/bootstrap-update.json";if(!QFileInfo::exists(path))return;
    auto journal=readJson(path);auto backup=backupPath(root,journal);
#ifdef Q_OS_WIN
    for(const auto &value:journal["files"].toArray()){
        auto file=value.toObject();auto relative=file["path"].toString();
        if(!safePath(relative))fail(QStringLiteral("실행기 복구 파일명이 올바르지 않습니다."));
        auto target=root+"/"+relative;noLinks(target);
        if(file["existed"].toBool()){
            auto saved=backup+"/"+relative;
            if(fileHash(saved)!=file["oldSha256"].toString())fail(QStringLiteral("실행기 복구 백업 검증에 실패했습니다."));
            copy(saved,target);
        }else if(QFileInfo::exists(target)&&!QFile::remove(target))fail(QStringLiteral("중단된 실행기 파일을 정리하지 못했습니다."));
    }
#else
    validateBundle(journal);auto target=journal["bundle"].toString(),saved=journal["bundleBackup"].toString();
    if(QFileInfo::exists(saved)){if(QFileInfo::exists(target))removeBackup(target);if(!QDir().rename(saved,target))fail(QStringLiteral("이전 앱 복원에 실패했습니다."));}
    removeBackup(journal["bundlePrepared"].toString());
#endif
    if(journal["hadInstalled"].toBool())writeJson(root+"/bootstrap-installed.json",journal["priorInstalled"].toObject());
    else QFile::remove(root+"/bootstrap-installed.json");
    if(!QFile::remove(path))fail(QStringLiteral("실행기 복구 기록 정리에 실패했습니다."));
    removeBackup(backup);
}
void beginBootstrapUpdate(const QString &root,const QString &payload){
    if(bootstrapCurrent(root,payload))return;
    const auto receipt=manifest(payload);const auto source=payload+"/bootstrap";
    auto id=QUuid::createUuid().toString(QUuid::Id128),name="bootstrap-backup-"+id,backup=root+"/"+name;
    noLinks(root);noLinks(backup);QDir().mkpath(backup);
    QJsonObject journal{{"schema",1},{"backup",name},{"sourceFolder",QFileInfo(payload).fileName()},{"manifest",receipt},{"hadInstalled",QFileInfo::exists(root+"/bootstrap-installed.json")}};
    if(journal["hadInstalled"].toBool())journal["priorInstalled"]=readJson(root+"/bootstrap-installed.json");
#ifdef Q_OS_WIN
    QJsonArray files;
    for(const auto &value:receipt["files"].toArray()){
        auto file=value.toObject();auto relative=file["path"].toString(),target=root+"/"+relative;
        noLinks(target);file["existed"]=QFileInfo::exists(target);
        if(file["existed"].toBool()){file["oldSha256"]=fileHash(target);copy(target,backup+"/"+relative);}files.append(file);
    }
    journal["files"]=files;writeJson(root+"/bootstrap-update.json",journal);
    try{
        for(const auto &value:files){auto file=value.toObject();if(file["path"].toString()=="TeamHJDLauncher.exe")continue;copy(source+"/"+file["path"].toString(),root+"/"+file["path"].toString());}
        copy(source+"/TeamHJDLauncher.exe",root+"/TeamHJDLauncher.exe");
    }catch(...){recoverBootstrapUpdate(root);throw;}
#else
    auto target=targetBundle(root),parent=QFileInfo(target).absolutePath();
    auto prepared=parent+"/.TeamHJDLauncher-update-"+id+".app",saved=parent+"/.TeamHJDLauncher-backup-"+id+".app";
    journal["bundle"]=target;journal["bundlePrepared"]=prepared;journal["bundleBackup"]=saved;validateBundle(journal);
    try{
        for(const auto &value:receipt["files"].toArray()){
            auto file=value.toObject();auto relative=file["path"].toString();
            if(!relative.startsWith("TeamHJDLauncher.app/"))fail(QStringLiteral("실행기 앱 경로가 올바르지 않습니다."));
            copy(source+"/"+relative,prepared+"/"+relative.mid(QStringLiteral("TeamHJDLauncher.app/").size()));
        }
        writeJson(root+"/bootstrap-update.json",journal);
        if(!QDir().rename(target,saved)||!QDir().rename(prepared,target))fail(QStringLiteral("앱 갱신 권한이 없습니다. 새 DMG로 설치해 주세요."));
    }catch(...){if(QFileInfo::exists(root+"/bootstrap-update.json"))recoverBootstrapUpdate(root);else removeBackup(prepared);throw;}
#endif
}
void commitBootstrapUpdate(const QString &root){
    auto path=root+"/bootstrap-update.json";if(!QFileInfo::exists(path))return;
    auto journal=readJson(path);auto backup=backupPath(root,journal);
    writeJson(root+"/bootstrap-installed.json",journal["manifest"].toObject());
    if(!QFile::remove(path))fail(QStringLiteral("실행기 갱신 완료를 저장하지 못했습니다."));
#ifdef Q_OS_MACOS
    validateBundle(journal);removeBackup(journal["bundleBackup"].toString());removeBackup(journal["bundlePrepared"].toString());
#endif
    removeBackup(backup);
}
}
