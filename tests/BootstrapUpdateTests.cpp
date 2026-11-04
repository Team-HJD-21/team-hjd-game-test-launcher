#include "core/BootstrapUpdate.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>
using namespace hjd;
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);int checks=0;
    auto check=[&](bool ok,const char *label){if(!ok)fail(QString::fromUtf8(label));++checks;QTextStream(stdout)<<"PASS "<<label<<'\n';};
    try{
        QTemporaryDir temp;check(temp.isValid(),"isolated bootstrap transaction root");
        const auto root=temp.path()+"/runtime",payload=root+"/launcher/versions/"+QLatin1String(HJD_VERSION)+"-fixture";
#ifdef Q_OS_WIN
        const QString entry="TeamHJDLauncher.exe",dependency="new-runtime.dll";const auto target=root;
#else
        const QString entry="TeamHJDLauncher.app/Contents/MacOS/TeamHJDLauncher",dependency="TeamHJDLauncher.app/Contents/Resources/new-runtime.txt";
        const auto target=temp.path()+"/Applications";
        writeJson(root+"/bootstrap-location.json",{{"executable",target+"/"+entry}});
#endif
        atomicWrite(target+"/"+entry,"old-bootstrap");atomicWrite(root+"/user-kept.txt","keep-me");
        atomicWrite(payload+"/bootstrap/"+entry,"new-bootstrap");atomicWrite(payload+"/bootstrap/"+dependency,"new-runtime");
        QJsonArray files;
        for(const auto &relative:QStringList{entry,dependency}){
            auto path=payload+"/bootstrap/"+relative;
            files.append(QJsonObject{{"path",relative},{"bytes",QFileInfo(path).size()},{"sha256",fileHash(path)}});
        }
        writeJson(payload+"/bootstrap-manifest.json",{{"version",HJD_VERSION},{"files",files}});
        check(!bootstrapCurrent(root,payload),"old stable entry point requires refresh");
        beginBootstrapUpdate(root,payload);
        check(readFile(target+"/"+entry)=="new-bootstrap"&&QFileInfo::exists(root+"/bootstrap-update.json"),"new entry point is journaled before commit");
        recoverBootstrapUpdate(root);
        check(readFile(target+"/"+entry)=="old-bootstrap"&&!QFileInfo::exists(target+"/"+dependency),"interrupted update restores old entry point and removes introduced files");
        check(!QFileInfo::exists(root+"/bootstrap-update.json"),"recovery clears its completed journal");
        beginBootstrapUpdate(root,payload);commitBootstrapUpdate(root);
        check(bootstrapCurrent(root,payload)&&!QFileInfo::exists(root+"/bootstrap-update.json"),"successful commit refreshes the stable entry point");
        check(readFile(root+"/user-kept.txt")=="keep-me","unrelated root file preserved through update and recovery");
        atomicWrite(payload+"/bootstrap/"+entry,"tampered");bool rejected=false;
        try{beginBootstrapUpdate(root,payload);}catch(const Error&){rejected=true;}
        check(rejected&&readFile(target+"/"+entry)=="new-bootstrap","tampered candidate cannot overwrite a valid stable entry point");
        writeJson(root+"/bootstrap-update.json",{{"backup","../outside"}});rejected=false;
        try{recoverBootstrapUpdate(root);}catch(const Error&){rejected=true;}
        check(rejected&&readFile(root+"/user-kept.txt")=="keep-me","invalid recovery path cannot touch unrelated data");
        QTextStream(stdout)<<"RESULT "<<checks<<" bootstrap transaction assertions passed.\n";return 0;
    }catch(const std::exception &error){QTextStream(stderr)<<error.what()<<'\n';return 1;}
}
