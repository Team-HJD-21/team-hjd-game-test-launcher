#include "core/Core.h"
#include "core/CloudflareLogin.h"
#include "miniz.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <windows.h>
#include <bcrypt.h>

using namespace hjd;
namespace {
int passed=0,failed=0;
void check(bool condition,const QString &name){QTextStream(stdout)<<(condition?"PASS ":"FAIL ")<<name<<'\n';if(condition)++passed;else++failed;}
void rejects(const QString &name,const std::function<void()> &body){try{body();check(false,name);}catch(const Error&){check(true,name);}}
struct SigningKey {
    BCRYPT_ALG_HANDLE alg{};BCRYPT_KEY_HANDLE key{};QString publicKey;
    SigningKey(){BCryptOpenAlgorithmProvider(&alg,BCRYPT_ECDSA_P256_ALGORITHM,nullptr,0);BCryptGenerateKeyPair(alg,&key,256,0);BCryptFinalizeKeyPair(key,0);ULONG size=0;BCryptExportKey(key,nullptr,BCRYPT_ECCPUBLIC_BLOB,nullptr,0,&size,0);QByteArray blob(size,0);BCryptExportKey(key,nullptr,BCRYPT_ECCPUBLIC_BLOB,reinterpret_cast<PUCHAR>(blob.data()),size,&size,0);publicKey="p256:"+QString::fromLatin1(blob.mid(sizeof(BCRYPT_ECCKEY_BLOB)).toBase64());}
    ~SigningKey(){BCryptDestroyKey(key);BCryptCloseAlgorithmProvider(alg,0);}
    QByteArray envelope(const QJsonObject &o){auto p=QJsonDocument(o).toJson(QJsonDocument::Compact);auto hash=QCryptographicHash::hash(p,QCryptographicHash::Sha256);ULONG size=0;BCryptSignHash(key,nullptr,reinterpret_cast<PUCHAR>(hash.data()),ULONG(hash.size()),nullptr,0,&size,0);QByteArray signature(size,0);BCryptSignHash(key,nullptr,reinterpret_cast<PUCHAR>(hash.data()),ULONG(hash.size()),reinterpret_cast<PUCHAR>(signature.data()),size,&size,0);return QJsonDocument(QJsonObject{{"payload",QString::fromLatin1(p.toBase64())},{"signature",QString::fromLatin1(signature.toBase64())}}).toJson();}
};
void makeZip(const QString &path,const QList<QPair<QString,QByteArray>> &entries){
    FILE *file{};_wfopen_s(&file,reinterpret_cast<const wchar_t*>(path.utf16()),L"wb+");if(!file)fail("test zip open");mz_zip_archive z{};
    if(!mz_zip_writer_init_cfile(&z,file,0))fail("test zip init");
    for(const auto &entry:entries){auto name=entry.first.toUtf8();if(!mz_zip_writer_add_mem(&z,name.constData(),entry.second.data(),size_t(entry.second.size()),MZ_BEST_SPEED))fail("test zip add");}
    mz_zip_writer_finalize_archive(&z);mz_zip_writer_end(&z);fclose(file);
}
Game fixture(const QString &zip,const QByteArray &exe,const QString &version="1.0.0"){
    Game g;g.id="fixture";g.name="Fixture";g.version=version;g.executable="Fixture.exe";g.sha256=fileHash(zip);g.bytes=QFileInfo(zip).size();g.expandedBytes=exe.size();g.files={{"Fixture.exe",QString::fromLatin1(QCryptographicHash::hash(exe,QCryptographicHash::Sha256).toHex()),exe.size()}};g.packageUrl=QUrl("https://localhost/packages/fixture.zip");return g;
}
QJsonObject catalog(const Game &game){return {{"schema",1},{"sequence",5},{"issuedAt",QDateTime::currentDateTimeUtc().addSecs(-60).toString(Qt::ISODate)},{"expiresAt",QDateTime::currentDateTimeUtc().addDays(7).toString(Qt::ISODate)},{"games",QJsonArray{game.toJson()}}};}
int probe(const QStringList &args){
    auto root=args.value(2);auto invite=readJson(args.value(3));NetConfig cfg;cfg.server=QUrl(invite["server"].toString());cfg.password=qgetenv("TEAMHJD_DISTRIBUTION_PASSWORD");cfg.caPem=invite["certificate"].toString().toUtf8();cfg.testLoopback=true;std::atomic_bool cancel=false;
    auto raw=Http::get(cfg.server.resolved(QUrl("catalog.json")),cfg,cancel);auto c=verifyCatalog(raw,invite["publisherKey"].toString(),cfg.server);auto g=c.games.first();
    int gameIndex=args.indexOf("--game-id");if(gameIndex>=0){bool found=false;for(const auto &candidate:c.games)if(candidate.id==args.value(gameIndex+1)){g=candidate;found=true;}if(!found)fail("requested game not in catalog");}
    auto part=root+"/downloads/"+g.sha256+".part";
    Http::download(g.packageUrl,cfg,part,g.bytes,g.sha256,cancel,{});
    if(args.contains("--resume")){QFile f(part);f.open(QIODevice::ReadWrite);f.resize(qMax<qint64>(1,g.bytes/2));f.close();Http::download(g.packageUrl,cfg,part,g.bytes,g.sha256,cancel,{});}
    Store store(root+"/games");store.install(g,part,cancel);if(!store.verify(g.id))fail("probe tree invalid");
    if(args.contains("--launch")){store.launch(g.id);if(!store.running(g.id))fail("probe process not running");rejects("running install blocked",[&]{store.install(g,part,cancel);});QThread::msleep(3000);if(store.running(g.id))fail("fixture did not exit");}
    QTextStream(stdout)<<"PROBE installed "<<g.version<<"; verified files="<<g.files.size()<<"; resume="<<args.contains("--resume")<<'\n';return 0;
}
}
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);
    try{
        if(app.arguments().contains("--verify-payload")){auto args=app.arguments();std::atomic_bool cancel=false;Store store(args.value(3));auto zip=args.value(2);auto folder=store.stagePayload(QStringLiteral(HJD_VERSION),fileHash(zip),zip,cancel);store.activate("launcher",folder);if(!store.verify("launcher"))fail("release payload tree mismatch");QTextStream(stdout)<<"PASS complete release payload extraction and verification\n";return 0;}
        if(app.arguments().contains("--verify-public-release")){auto args=app.arguments();std::atomic_bool cancel=false;auto raw=Http::get(QUrl("https://api.github.com/repos/Team-HJD-21/team-hjd-game-test-launcher/releases/latest"),{},cancel,2*1024*1024,true);auto release=QJsonDocument::fromJson(raw).object();for(auto v:release["assets"].toArray()){auto a=v.toObject();if(a["name"].toString()!="TeamHJD-LauncherPayload-Windows-x64.zip")continue;auto path=args.value(2)+"/public-payload.part";Http::download(QUrl(a["browser_download_url"].toString()),{},path,a["size"].toInteger(),a["digest"].toString().mid(7),cancel,{},true);Store store(args.value(2)+"/install");auto folder=store.stagePayload(release["tag_name"].toString().mid(1),a["digest"].toString().mid(7),path,cancel);store.activate("launcher",folder);if(!store.verify("launcher"))fail("public release payload tree mismatch");QTextStream(stdout)<<"PASS live public GitHub API, HTTPS redirects, digest, extraction and payload verification\n";return 0;}fail("public release payload missing");}
        if(app.arguments().contains("--probe"))return probe(app.arguments());
        QDir().mkpath(QCoreApplication::applicationDirPath()+"/../.local");QTemporaryDir temp(QCoreApplication::applicationDirPath()+"/../.local/core-한글-XXXXXX");if(!temp.isValid())fail("test directory");auto root=temp.path();
        for(const auto &binary:QStringList{"TeamHJDClient.exe","TeamHJDLauncher.exe"}){auto path=QCoreApplication::applicationDirPath()+"/"+binary;auto module=LoadLibraryExW(reinterpret_cast<LPCWSTR>(path.utf16()),nullptr,LOAD_LIBRARY_AS_DATAFILE);check(module&&FindResourceW(module,L"IDI_APP_ICON",RT_GROUP_ICON),"brand icon embedded in "+binary);if(module)FreeLibrary(module);}
        for(const auto &p:QStringList{"../evil.exe","C:/evil","/root","a\\b","a/../x","file.","file ","NUL.txt","COM1.exe","a:b","a//b","._teamhjd-receipt.json","a/<b"})check(!safePath(p),"reject path "+p);
        check(safePath("게임_Data/설정 파일.json"),"Unicode relative paths");
        check(!allowedServer(QUrl("http://example.com"),true),"HTTP internet blocked even in tests");check(!allowedServer(QUrl("https://user:pw@example.com")),"URL credentials blocked");check(allowedServer(QUrl("https://localhost:8443/")),"HTTPS accepted");check(!sameOrigin(QUrl("https://a:8443"),QUrl("https://a:8444")),"Origin includes port");
        auto secret=QByteArray("fixture-secret-한국어");auto encrypted=protect(secret);check(encrypted!=secret&&unprotect(encrypted)==secret,"DPAPI round trip");
        {
            // Synthetic tokens intentionally have no cryptographic signature.
            // inspect() guards response shape/claims; only the server authorizes.
            const QString issuer="https://fixture.cloudflareaccess.com",audience=QString(64,'a');
            const auto now=QDateTime::currentSecsSinceEpoch();
            QJsonObject header{{"alg","RS256"},{"kid","fixture-key"}};
            QJsonObject claims{{"iss",issuer},{"aud",QJsonArray{audience}},{"type","app"},{"exp",now+3600},{"iat",now-60},{"nbf",now-60}};
            auto token=[](const QJsonObject &h,const QJsonObject &c){
                const auto encoding=QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals;
                return QJsonDocument(h).toJson(QJsonDocument::Compact).toBase64(encoding)+'.'+QJsonDocument(c).toJson(QJsonDocument::Compact).toBase64(encoding)+'.'+QByteArray("synthetic-signature").toBase64(encoding);
            };
            const auto validToken=token(header,claims);
            const auto session=CloudflareLogin::inspect(validToken,issuer,audience);
            check(session.jwt==validToken&&session.expires.toSecsSinceEpoch()==now+3600,"Access structural response and expiry accepted without authorizing");
            auto stringAudience=claims;stringAudience["aud"]=audience;
            check(CloudflareLogin::inspect(token(header,stringAudience),issuer,audience).jwt==token(header,stringAudience),"Access string audience accepted");
            rejects("Access wrong audience rejected",[&]{CloudflareLogin::inspect(validToken,issuer,QString(64,'b'));});
            rejects("Access wrong issuer rejected",[&]{CloudflareLogin::inspect(validToken,"https://other.cloudflareaccess.com",audience);});
            auto badAlgorithm=header;badAlgorithm["alg"]="none";
            rejects("Access unexpected algorithm rejected",[&]{CloudflareLogin::inspect(token(badAlgorithm,claims),issuer,audience);});
            auto expiredClaims=claims;expiredClaims["exp"]=now-1;
            rejects("Access expired response rejected",[&]{CloudflareLogin::inspect(token(header,expiredClaims),issuer,audience);});
            auto futureClaims=claims;futureClaims["nbf"]=now+600;
            rejects("Access future response rejected",[&]{CloudflareLogin::inspect(token(header,futureClaims),issuer,audience);});
            auto wrongType=claims;wrongType["type"]="org";
            rejects("Access organization token rejected",[&]{CloudflareLogin::inspect(token(header,wrongType),issuer,audience);});
            rejects("Access malformed token rejected",[&]{CloudflareLogin::inspect("not-a-token",issuer,audience);});
            rejects("Access malformed JSON rejected",[&]{CloudflareLogin::inspect("eA.eA.eA",issuer,audience);});
            rejects("Access oversized token rejected",[&]{CloudflareLogin::inspect(QByteArray(65537,'a'),issuer,audience);});
        }
        auto exe=readFile(QCoreApplication::applicationDirPath()+"/LauncherFixtureGame.exe");auto zip=root+"/fixture.zip";makeZip(zip,{{"Fixture.exe",exe}});auto game=fixture(zip,exe);std::atomic_bool cancel=false;Store store(root+"/games");
        SigningKey key;
        QJsonObject invite{{"schema",1},{"server","https://localhost:8443/"},{"publisherKey",key.publicKey},{"accessToken","hjd1."+QString(16,'a')+"."+QString(43,'b')},{"expiresAt",QDateTime::currentDateTimeUtc().addDays(7).toString(Qt::ISODate)},{"recipientLabel","Fixture"}};
        auto parsedInvite=Invite::parse(QJsonDocument(invite).toJson());check(parsedInvite.label=="Fixture"&&parsedInvite.accessToken.size()==65,"personal invite validated");
        auto expiredInvite=invite;expiredInvite["expiresAt"]=QDateTime::currentDateTimeUtc().addSecs(-1).toString(Qt::ISODate);rejects("expired invite rejected before saving",[&]{Invite::parse(QJsonDocument(expiredInvite).toJson());});
        auto malformedInvite=invite;malformedInvite["accessToken"]="hjd1.bad";rejects("malformed invite credential",[&]{Invite::parse(QJsonDocument(malformedInvite).toJson());});
        auto insecureInvite=invite;insecureInvite["server"]="http://example.com/";rejects("insecure invite endpoint",[&]{Invite::parse(QJsonDocument(insecureInvite).toJson());});
        auto cat=catalog(game);auto envelope=key.envelope(cat);auto verified=verifyCatalog(envelope,key.publicKey,QUrl("https://localhost/"),5);check(verified.games.size()==1&&verified.sequence==5,"signed catalog");
        rejects("wrong publisher key",[&]{SigningKey other;verifyCatalog(envelope,other.publicKey,QUrl("https://localhost/"));});
        auto altered=QJsonDocument::fromJson(envelope).object();auto p=QByteArray::fromBase64(altered["payload"].toString().toLatin1());p.replace("Fixture","Altered");altered["payload"]=QString::fromLatin1(p.toBase64());rejects("tampered catalog",[&]{verifyCatalog(QJsonDocument(altered).toJson(),key.publicKey,QUrl("https://localhost/"));});
        rejects("catalog rollback",[&]{verifyCatalog(envelope,key.publicKey,QUrl("https://localhost/"),6);});
        auto expired=cat;expired["issuedAt"]=QDateTime::currentDateTimeUtc().addDays(-8).toString(Qt::ISODate);expired["expiresAt"]=QDateTime::currentDateTimeUtc().addDays(-1).toString(Qt::ISODate);rejects("expired catalog",[&]{verifyCatalog(key.envelope(expired),key.publicKey,QUrl("https://localhost/"));});
        auto remote=game.toJson();remote["url"]="https://evil.invalid/a.zip";rejects("cross origin package",[&]{Game::parse(remote,QUrl("https://localhost/"));});
        auto duplicate=game.toJson();auto files=duplicate["files"].toArray();files.append(files.first());duplicate["files"]=files;rejects("duplicate manifest path",[&]{Game::parse(duplicate,QUrl("https://localhost/"));});
        auto missing=game.toJson();missing["executable"]="Absent.exe";rejects("missing executable",[&]{Game::parse(missing,QUrl("https://localhost/"));});
        store.install(game,zip,cancel);check(store.verify(game.id),"first install verified");auto initial=store.state(game.id);auto original=store.currentDirectory(game.id);
        store.install(game,zip,cancel);check(store.state(game.id)==initial&&QDir(root+"/games/fixture/versions").entryList(QDir::Dirs|QDir::NoDotAndDotDot).size()==1,"same package install is idempotent");
        auto two=game;two.version="1.1.0";store.install(two,zip,cancel);check(store.currentReceipt(game.id)["version"]=="1.1.0"&&QFileInfo::exists(original+"/Fixture.exe"),"update preserves old version");
        store.rollback(game.id);check(store.currentReceipt(game.id)["version"]=="1.0.0","rollback verified previous");auto stable=store.state(game.id);
        cancel=true;rejects("cancel install",[&]{store.install(two,zip,cancel);});cancel=false;check(store.state(game.id)==stable,"cancel preserves current pointer");
        auto bad=game;bad.sha256=QString(64,'0');rejects("bad package digest",[&]{store.install(bad,zip,cancel);});check(store.state(game.id)==stable,"hash failure preserves current pointer");
        auto badFile=game;badFile.files[0].sha256=QString(64,'0');rejects("bad file digest",[&]{store.install(badFile,zip,cancel);});check(store.state(game.id)==stable,"file failure preserves pointer");
        store.launch(game.id);check(store.running(game.id),"launch tracks process identity");rejects("running update blocked",[&]{store.install(two,zip,cancel);});rejects("running rollback blocked",[&]{store.rollback(game.id);});rejects("running uninstall blocked",[&]{store.uninstall(game.id);});QThread::msleep(3000);check(!store.running(game.id),"process exit detected");
        for(const auto &path:QStringList{"../escape.exe","C:/escape.exe","a\\escape.exe","NUL.exe","foo.","._teamhjd-receipt.json"}){auto evil=root+"/evil.zip";makeZip(evil,{{path,exe}});rejects("archive path "+path,[&]{inspectArchive(evil,root+"/unpacked-"+QString::number(passed),{},cancel);});}
        auto duplicateZip=root+"/duplicate.zip";makeZip(duplicateZip,{{"A.exe",exe},{"a.exe",exe}});rejects("archive case collision",[&]{inspectArchive(duplicateZip,root+"/dup",{},cancel);});
        auto collisionZip=root+"/collision.zip";makeZip(collisionZip,{{"data",exe},{"data/file",exe}});rejects("archive file parent collision",[&]{inspectArchive(collisionZip,root+"/collision",{},cancel);});
        auto extra=root+"/extra.zip";makeZip(extra,{{"Fixture.exe",exe},{"unexpected.dll",exe}});rejects("unlisted archive file",[&]{inspectArchive(extra,root+"/extra",game.files,cancel);});
        atomicWrite(store.currentDirectory(game.id)+"/unexpected.txt","tamper");check(!store.verify(game.id),"extra installed file detected");QFile::remove(store.currentDirectory(game.id)+"/unexpected.txt");atomicWrite(store.currentDirectory(game.id)+"/Fixture.exe","tamper");check(!store.verify(game.id),"corrupt installation detected");rejects("corrupt game cannot launch",[&]{store.launch(game.id);});
        auto payload=root+"/payload.zip";makeZip(payload,{{"TeamHJDClient.exe",exe}});Store launcher(root+"/app");auto folder=launcher.stagePayload("2.0.0",fileHash(payload),payload,cancel);launcher.activate("launcher",folder);check(launcher.verify("launcher"),"launcher payload staged separately");
        atomicWrite(root+"/user-saves/save.json","preserve me");store.uninstall(game.id);check(store.currentDirectory(game.id).isEmpty()&&!QFileInfo::exists(root+"/games/fixture/versions"),"uninstall removes managed versions and state");check(readFile(root+"/user-saves/save.json")=="preserve me","uninstall preserves external saves");store.uninstall(game.id);check(true,"uninstall is idempotent");store.install(game,zip,cancel);check(store.verify(game.id),"reinstall after uninstall");store.install(game,zip,cancel,{},true);check(QDir(root+"/games/fixture/versions").entryList(QDir::Dirs|QDir::NoDotAndDotDot).size()==2,"explicit repair creates verified replacement");
        check(!QFileInfo::exists(root+"/escape.exe"),"no archive escape written");
        QTextStream(stdout)<<"RESULT "<<passed<<" passed; "<<failed<<" failed\n";return failed?1:0;
    }catch(const std::exception &e){QTextStream(stderr)<<"FATAL "<<e.what()<<'\n';return 2;}
}
