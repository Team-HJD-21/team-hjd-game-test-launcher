#pragma once
#include "core/Core.h"
#include <QDir>
#include <QFileInfo>

// Directly opening a receipted installed client must retain automatic updates.
// Development executables and arbitrary directory layouts are never adopted.
inline QString managedLauncherRoot(const QString &executable){
    try{
        const auto full=QFileInfo(executable).absoluteFilePath();
        QDir versionDirectory=QFileInfo(full).dir();
#ifdef Q_OS_MACOS
        if(!versionDirectory.cdUp()||!versionDirectory.cdUp()||!versionDirectory.cdUp())return {};
        const QString entry="TeamHJDClient.app/Contents/MacOS/TeamHJDClient";
#else
        const QString entry="TeamHJDClient.exe";
#endif
        const auto versionPath=versionDirectory.absolutePath();
        if(QDir::cleanPath(versionDirectory.filePath(entry))!=QDir::cleanPath(full))return {};
        if(!versionDirectory.cdUp()||versionDirectory.dirName()!="versions"||!versionDirectory.cdUp()||versionDirectory.dirName()!="launcher"||!versionDirectory.cdUp())return {};
        const auto root=versionDirectory.absolutePath();hjd::noLinks(root);hjd::noLinks(full);
        if(!QFileInfo::exists(root+"/launcher/active.json"))return {};
        hjd::Store store(root);if(store.state("launcher")["current"].toString().isEmpty())return {};
        auto receipt=hjd::readJson(versionPath+"/._teamhjd-receipt.json");
        if(receipt["executable"].toString()!=entry)return {};
        for(auto value:receipt["files"].toArray()){
            auto file=value.toObject();
            if(file["path"].toString()==entry&&file["bytes"].toInteger()==QFileInfo(full).size()&&file["sha256"].toString()==hjd::fileHash(full))return root;
        }
    }catch(const hjd::Error&){}
    return {};
}
