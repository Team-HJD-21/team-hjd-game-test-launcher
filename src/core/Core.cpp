#include "Core.h"
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStorageInfo>
#include <QSslCertificate>
#include <QUuid>
#include <cmath>
#include <cerrno>
#include <cstdio>
#include <limits>
#ifdef Q_OS_WIN
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#elif defined(Q_OS_MACOS)
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonHMAC.h>
#include <libproc.h>
#include <sys/proc_info.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#endif
#include "miniz.h"

namespace hjd {
void fail(const QString &s) { throw Error(s); }
QByteArray readFile(const QString &p, qint64 limit) {
    QFile f(p);
    if (!f.open(QIODevice::ReadOnly) || f.size() > limit) fail(QStringLiteral("파일을 읽을 수 없습니다: %1").arg(QFileInfo(p).fileName()));
    auto data=f.readAll();
    if (f.error()!=QFile::NoError) fail(QStringLiteral("파일 읽기가 중단되었습니다."));
    return data;
}
QJsonObject readJson(const QString &p) {
    QJsonParseError e; auto d=QJsonDocument::fromJson(readFile(p),&e);
    if(e.error!=QJsonParseError::NoError || !d.isObject()) fail(QStringLiteral("파일 형식이 올바르지 않습니다."));
    return d.object();
}
void atomicWrite(const QString &p,const QByteArray &data) {
    noLinks(QFileInfo(p).absolutePath());
    if(!QDir().mkpath(QFileInfo(p).absolutePath())) fail(QStringLiteral("저장 폴더를 만들 수 없습니다."));
    noLinks(p);
    QSaveFile f(p); f.setDirectWriteFallback(false);
    if(!f.open(QIODevice::WriteOnly) || f.write(data)!=data.size() || !f.commit()) fail(QStringLiteral("변경사항을 안전하게 저장하지 못했습니다."));
}
void writeJson(const QString &p,const QJsonObject &o) { atomicWrite(p,QJsonDocument(o).toJson(QJsonDocument::Compact)); }
QString fileHash(const QString &p,std::atomic_bool *cancel) {
    QFile f(p); if(!f.open(QIODevice::ReadOnly)) fail(QStringLiteral("검사할 파일을 읽지 못했습니다."));
    QCryptographicHash h(QCryptographicHash::Sha256);
    while(!f.atEnd()) { if(cancel && *cancel) fail(QStringLiteral("취소했습니다.")); auto b=f.read(1024*1024); if(b.isEmpty() && f.error()!=QFile::NoError) fail(QStringLiteral("파일 검사에 실패했습니다.")); h.addData(b); }
    return QString::fromLatin1(h.result().toHex());
}
bool safeId(const QString &s) { static QRegularExpression r("^[a-z0-9][a-z0-9-]{0,63}$"); return r.match(s).hasMatch(); }
bool safeVersion(const QString &s) { static QRegularExpression r("^[0-9][A-Za-z0-9._-]{0,79}$"); return r.match(s).hasMatch() && !s.endsWith('.'); }
bool safePath(const QString &p) {
    if(p.isEmpty() || p.size()>220 || p.startsWith('/') || p.contains('\\') || p.contains(':') || p.contains(QChar::Null)) return false;
    static QRegularExpression reserved("^(CON|PRN|AUX|NUL|COM[0-9¹²³]|LPT[0-9¹²³])(?:\\.|$)",QRegularExpression::CaseInsensitiveOption);
    for(const auto &part:p.split('/')) {
        if(part.isEmpty() || part=="." || part==".." || part.endsWith('.') || part.endsWith(' ') || part.startsWith("._teamhjd",Qt::CaseInsensitive) || reserved.match(part).hasMatch()) return false;
        for(const auto c:part) if(c.unicode()<32 || QStringLiteral("<>\"|?*").contains(c)) return false;
    }
    return true;
}
void noLinks(const QString &p) {
    QString current=QDir::cleanPath(QFileInfo(p).absoluteFilePath());
    while(true) {
#ifdef Q_OS_WIN
        auto native=QDir::toNativeSeparators(current);
        DWORD attr=GetFileAttributesW(reinterpret_cast<LPCWSTR>(native.utf16()));
        if(attr!=INVALID_FILE_ATTRIBUTES && (attr&FILE_ATTRIBUTE_REPARSE_POINT)) fail(QStringLiteral("연결 폴더에는 설치할 수 없습니다. 일반 폴더를 선택하세요."));
#elif defined(Q_OS_MACOS)
        struct stat info{};
        const auto encoded=QFile::encodeName(current);
        if(lstat(encoded.constData(),&info)==0) {
            if(S_ISLNK(info.st_mode)) {
                // macOS ships these root-owned aliases. They cannot be changed
                // by the login user; arbitrary user-controlled links stay denied.
                const bool systemAlias=info.st_uid==0 &&
                    (current=="/var"||current=="/tmp"||current=="/etc") &&
                    QFileInfo(current).canonicalFilePath()=="/private"+current;
                if(!systemAlias) fail(QStringLiteral("연결 폴더에는 설치할 수 없습니다. 일반 폴더를 선택하세요."));
            }
        } else if(errno!=ENOENT&&errno!=ENOTDIR) fail(QStringLiteral("설치 경로의 접근 권한을 확인할 수 없습니다."));
#endif
        auto parent=QFileInfo(current).dir().absolutePath(); if(parent==current) break; current=parent;
    }
}
#ifdef Q_OS_MACOS
namespace {
template<typename T> class CfOwner {
public:
    explicit CfOwner(T value=nullptr):value(value){}
    ~CfOwner(){if(value)CFRelease(value);}
    CfOwner(const CfOwner&)=delete;
    CfOwner& operator=(const CfOwner&)=delete;
    T get() const{return value;}
private:T value;
};
class SecretBytes {
public:
    QByteArray bytes;
    ~SecretBytes(){if(!bytes.isEmpty())explicit_bzero(bytes.data(),size_t(bytes.size()));}
};
CFMutableDictionaryRef dictionary(){return CFDictionaryCreateMutable(kCFAllocatorDefault,0,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);}
CFDataRef cfData(const QByteArray &bytes){return CFDataCreate(kCFAllocatorDefault,reinterpret_cast<const UInt8*>(bytes.constData()),CFIndex(bytes.size()));}
void wrappingKey(SecretBytes &key,bool create) {
    CfOwner<CFMutableDictionaryRef> query(dictionary());
    CFDictionarySetValue(query.get(),kSecClass,kSecClassGenericPassword);
    CFDictionarySetValue(query.get(),kSecAttrService,CFSTR("com.teamhjd.launcher.credentials"));
    CFDictionarySetValue(query.get(),kSecAttrAccount,CFSTR("wrap-v1"));
    CFDictionarySetValue(query.get(),kSecAttrSynchronizable,kCFBooleanFalse);
    CFDictionarySetValue(query.get(),kSecReturnData,kCFBooleanTrue);
    CFDictionarySetValue(query.get(),kSecMatchLimit,kSecMatchLimitOne);
    CFTypeRef raw=nullptr;
    auto status=SecItemCopyMatching(query.get(),&raw);
    if(status==errSecItemNotFound&&create) {
        SecretBytes fresh;fresh.bytes.resize(64);
        if(SecRandomCopyBytes(kSecRandomDefault,64,reinterpret_cast<uint8_t*>(fresh.bytes.data()))!=errSecSuccess)
            fail(QStringLiteral("안전한 인증 저장 키를 만들지 못했습니다."));
        CfOwner<CFDataRef> value(cfData(fresh.bytes));
        CFDictionaryRemoveValue(query.get(),kSecReturnData);
        CFDictionaryRemoveValue(query.get(),kSecMatchLimit);
        CFDictionarySetValue(query.get(),kSecValueData,value.get());
        CFDictionarySetValue(query.get(),kSecAttrLabel,CFSTR("TeamHJD Launcher credentials"));
        auto added=SecItemAdd(query.get(),nullptr);
        if(added!=errSecSuccess&&added!=errSecDuplicateItem)
            fail(QStringLiteral("macOS 키체인에 인증 정보를 저장하지 못했습니다. 키체인 접근을 허용해 주세요."));
        wrappingKey(key,false);return;
    }
    CfOwner<CFTypeRef> result(raw);
    if(status!=errSecSuccess||!raw||CFGetTypeID(raw)!=CFDataGetTypeID()||CFDataGetLength(static_cast<CFDataRef>(raw))!=64)
        fail(QStringLiteral("macOS 키체인을 열지 못했습니다. 키체인을 잠금 해제하고 다시 인증해 주세요."));
    key.bytes=QByteArray(reinterpret_cast<const char*>(CFDataGetBytePtr(static_cast<CFDataRef>(raw))),64);
}
QByteArray authenticationTag(const QByteArray &data,const QByteArray &key) {
    QByteArray tag(CC_SHA256_DIGEST_LENGTH,0);
    CCHmac(kCCHmacAlgSHA256,key.constData()+32,32,data.constData(),size_t(data.size()),tag.data());
    return tag;
}
const QByteArray credentialHeader("HJDMAC1",7);
QByteArray derInteger(QByteArray value) {
    while(value.size()>1&&value[0]=='\0')value.remove(0,1);
    if(static_cast<unsigned char>(value[0])&0x80)value.prepend('\0');
    QByteArray result;result.append(char(0x02));result.append(char(value.size()));result+=value;return result;
}
}
#endif
QByteArray protect(const QByteArray &p) {
    if(p.isEmpty()) return {};
#ifdef Q_OS_WIN
    DATA_BLOB in{DWORD(p.size()),reinterpret_cast<BYTE*>(const_cast<char*>(p.data()))},out{};
    if(!CryptProtectData(&in,L"TeamHJD Launcher",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&out)) fail(QStringLiteral("Windows 암호 저장에 실패했습니다."));
    QByteArray result(reinterpret_cast<char*>(out.pbData),out.cbData); SecureZeroMemory(out.pbData,out.cbData); LocalFree(out.pbData); return result;
#elif defined(Q_OS_MACOS)
    if(p.size()>1024*1024)fail(QStringLiteral("인증 정보가 너무 큽니다."));
    SecretBytes key;wrappingKey(key,true);
    QByteArray iv(kCCBlockSizeAES128,0),cipher(p.size()+kCCBlockSizeAES128,0);
    if(SecRandomCopyBytes(kSecRandomDefault,size_t(iv.size()),reinterpret_cast<uint8_t*>(iv.data()))!=errSecSuccess)
        fail(QStringLiteral("인증 정보 보호를 준비하지 못했습니다."));
    size_t length=0;
    if(CCCrypt(kCCEncrypt,kCCAlgorithmAES,kCCOptionPKCS7Padding,key.bytes.constData(),kCCKeySizeAES256,iv.constData(),p.constData(),size_t(p.size()),cipher.data(),size_t(cipher.size()),&length)!=kCCSuccess)
        fail(QStringLiteral("인증 정보를 보호하지 못했습니다."));
    cipher.resize(qsizetype(length));
    auto result=credentialHeader+iv+cipher;result+=authenticationTag(result,key.bytes);return result;
#else
    fail(QStringLiteral("지원하지 않는 운영체제입니다."));
#endif
}
QByteArray unprotect(const QByteArray &p) {
    if(p.isEmpty()) return {};
#ifdef Q_OS_WIN
    DATA_BLOB in{DWORD(p.size()),reinterpret_cast<BYTE*>(const_cast<char*>(p.data()))},out{};
    if(!CryptUnprotectData(&in,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&out)) fail(QStringLiteral("저장된 암호를 열 수 없습니다. 다시 입력하세요."));
    QByteArray result(reinterpret_cast<char*>(out.pbData),out.cbData); SecureZeroMemory(out.pbData,out.cbData); LocalFree(out.pbData); return result;
#elif defined(Q_OS_MACOS)
    const auto headerLength=credentialHeader.size()+kCCBlockSizeAES128;
    const auto cipherLength=p.size()-headerLength-CC_SHA256_DIGEST_LENGTH;
    if(!p.startsWith(credentialHeader)||cipherLength<=0||cipherLength%kCCBlockSizeAES128||p.size()>1024*1024+128)
        fail(QStringLiteral("저장된 인증 정보가 올바르지 않습니다. 다시 인증해 주세요."));
    SecretBytes key;wrappingKey(key,false);
    const auto authenticated=p.left(p.size()-CC_SHA256_DIGEST_LENGTH),tag=authenticationTag(authenticated,key.bytes);
    if(timingsafe_bcmp(tag.constData(),p.constData()+p.size()-CC_SHA256_DIGEST_LENGTH,CC_SHA256_DIGEST_LENGTH)!=0)
        fail(QStringLiteral("저장된 인증 정보를 확인하지 못했습니다. 다시 인증해 주세요."));
    SecretBytes plain;plain.bytes.resize(cipherLength);
    size_t length=0;
    if(CCCrypt(kCCDecrypt,kCCAlgorithmAES,kCCOptionPKCS7Padding,key.bytes.constData(),kCCKeySizeAES256,p.constData()+credentialHeader.size(),p.constData()+headerLength,size_t(cipherLength),plain.bytes.data(),size_t(plain.bytes.size()),&length)!=kCCSuccess)
        fail(QStringLiteral("저장된 인증 정보를 열지 못했습니다. 다시 인증해 주세요."));
    return QByteArray(plain.bytes.constData(),qsizetype(length));
#else
    fail(QStringLiteral("지원하지 않는 운영체제입니다."));
#endif
}
bool verifySignature(const QByteArray &payload,const QByteArray &signature,const QString &key) {
    if(!key.startsWith("p256:")) return false;
    auto decoded=QByteArray::fromBase64Encoding(key.mid(5).toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
    if(!decoded || decoded.decoded.size()!=64 || signature.size()!=64) return false;
#ifdef Q_OS_WIN
    BCRYPT_ALG_HANDLE alg{}; BCRYPT_KEY_HANDLE pub{};
    if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_ECDSA_P256_ALGORITHM,nullptr,0)<0) return false;
    BCRYPT_ECCKEY_BLOB header{BCRYPT_ECDSA_PUBLIC_P256_MAGIC,32};
    QByteArray blob(reinterpret_cast<const char*>(&header),sizeof(header)); blob+=decoded.decoded;
    bool ok=false;
    if(BCryptImportKeyPair(alg,nullptr,BCRYPT_ECCPUBLIC_BLOB,&pub,reinterpret_cast<PUCHAR>(blob.data()),ULONG(blob.size()),0)>=0) {
        auto digest=QCryptographicHash::hash(payload,QCryptographicHash::Sha256);
        ok=BCryptVerifySignature(pub,nullptr,reinterpret_cast<PUCHAR>(digest.data()),ULONG(digest.size()),reinterpret_cast<PUCHAR>(const_cast<char*>(signature.data())),ULONG(signature.size()),0)>=0;
    }
    if(pub) BCryptDestroyKey(pub); BCryptCloseAlgorithmProvider(alg,0); return ok;
#elif defined(Q_OS_MACOS)
    const QByteArray publicBytes=QByteArray(1,char(0x04))+decoded.decoded;
    CfOwner<CFDataRef> keyData(cfData(publicBytes));
    CfOwner<CFMutableDictionaryRef> attributes(dictionary());
    CFDictionarySetValue(attributes.get(),kSecAttrKeyType,kSecAttrKeyTypeECSECPrimeRandom);
    CFDictionarySetValue(attributes.get(),kSecAttrKeyClass,kSecAttrKeyClassPublic);
    int bits=256;CfOwner<CFNumberRef> size(CFNumberCreate(kCFAllocatorDefault,kCFNumberIntType,&bits));
    CFDictionarySetValue(attributes.get(),kSecAttrKeySizeInBits,size.get());
    CfOwner<SecKeyRef> pub(SecKeyCreateWithData(keyData.get(),attributes.get(),nullptr));
    if(!pub.get())return false;
    const auto integers=derInteger(signature.left(32))+derInteger(signature.mid(32));
    QByteArray encoded;encoded.append(char(0x30));encoded.append(char(integers.size()));encoded+=integers;
    CfOwner<CFDataRef> data(cfData(payload)),der(cfData(encoded));
    return SecKeyVerifySignature(pub.get(),kSecKeyAlgorithmECDSASignatureMessageX962SHA256,data.get(),der.get(),nullptr);
#else
    return false;
#endif
}
static qint64 integer(const QJsonObject &o,const char *key,qint64 maximum) {
    auto v=o.value(QLatin1String(key)); const double d=v.toDouble(-1);
    if(!v.isDouble() || d<0 || d>double(maximum) || std::floor(d)!=d) fail(QStringLiteral("배포 정보의 용량 또는 순서가 올바르지 않습니다."));
    return qint64(d);
}
static QString hashValue(const QJsonObject &o) {
    const auto h=o["sha256"].toString(); static QRegularExpression r("^[a-f0-9]{64}$");
    if(!r.match(h).hasMatch()) fail(QStringLiteral("배포 파일의 검증 정보가 없습니다.")); return h;
}
bool sameOrigin(const QUrl &a,const QUrl &b) { return a.scheme()==b.scheme() && a.host()==b.host() && a.port(a.scheme()=="https"?443:80)==b.port(b.scheme()=="https"?443:80); }
bool allowedServer(const QUrl &u,bool test) {
    return u.isValid() && !u.host().isEmpty() && u.userInfo().isEmpty() && u.query().isEmpty() && u.fragment().isEmpty()
        && (u.scheme()=="https" || (test && u.scheme()=="http" && (u.host()=="127.0.0.1" || u.host()=="[::1]" || u.host()=="localhost")));
}
Invite Invite::parse(const QByteArray &json,bool test){
    if(json.size()>256*1024)fail(QStringLiteral("초대 파일이 너무 큽니다."));
    QJsonParseError error;auto document=QJsonDocument::fromJson(json,&error);auto object=document.object();
    if(error.error!=QJsonParseError::NoError||!document.isObject()||object["schema"].toInt()!=1)fail(QStringLiteral("지원하지 않는 초대 파일입니다."));
    Invite result;result.server=QUrl(object["server"].toString());result.publisherKey=object["publisherKey"].toString().trimmed();result.certificate=object["certificate"].toString().toUtf8();result.label=object["recipientLabel"].toString().left(120);
    if(!result.server.path().endsWith('/'))result.server.setPath(result.server.path()+'/');
    if(!allowedServer(result.server,test))fail(QStringLiteral("초대 파일의 HTTPS 서버 주소가 올바르지 않습니다."));
    auto key=QByteArray::fromBase64Encoding(result.publisherKey.mid(5).toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
    if(!result.publisherKey.startsWith("p256:")||!key||key.decoded.size()!=64)fail(QStringLiteral("초대 파일의 배포자 키가 올바르지 않습니다."));
    if(!result.certificate.isEmpty()&&QSslCertificate::fromData(result.certificate).isEmpty())fail(QStringLiteral("초대 파일의 인증서를 읽을 수 없습니다."));
    result.accessToken=object["accessToken"].toString().toLatin1();
    if(!result.accessToken.isEmpty()){
        static QRegularExpression token("^hjd1\\.[a-f0-9]{16}\\.[A-Za-z0-9_-]{43}$");
        if(!token.match(QString::fromLatin1(result.accessToken)).hasMatch())fail(QStringLiteral("초대 파일의 인증 정보가 올바르지 않습니다."));
        result.expires=QDateTime::fromString(object["expiresAt"].toString(),Qt::ISODate);
        auto now=QDateTime::currentDateTimeUtc();if(!result.expires.isValid()||result.expires<=now||result.expires>now.addDays(31))fail(QStringLiteral("초대가 만료되었거나 유효기간이 올바르지 않습니다. 새 초대를 받아 주세요."));
    }
    return result;
}
QJsonObject Game::toJson() const {
    QJsonArray a; for(const auto &f:files) a.append(QJsonObject{{"path",f.path},{"bytes",f.bytes},{"sha256",f.sha256}});
    return {{"id",id},{"name",name},{"description",description},{"version",version},{"executable",executable},{"platform",platform},{"notes",notes},{"url",packageUrl.toString()},{"sha256",sha256},{"bytes",bytes},{"expandedBytes",expandedBytes},{"files",a}};
}
bool Game::supported() const {
#if defined(Q_OS_WIN) && defined(Q_PROCESSOR_X86_64)
    return platform=="windows-x64";
#elif defined(Q_OS_MACOS) && defined(Q_PROCESSOR_ARM_64)
    return platform=="macos-arm64"||platform=="macos-universal";
#elif defined(Q_OS_MACOS) && defined(Q_PROCESSOR_X86_64)
    return platform=="macos-x64"||platform=="macos-universal";
#else
    return false;
#endif
}
Game Game::parse(const QJsonObject &o,const QUrl &origin) {
    Game g; g.id=o["id"].toString(); g.name=o["name"].toString(); g.description=o["description"].toString(); g.version=o["version"].toString(); g.executable=o["executable"].toString(); g.notes=o["notes"].toString();
    if(o.contains("platform"))g.platform=o["platform"].toString();
    const bool windows=g.platform=="windows-x64";
    const bool mac=QStringList{"macos-arm64","macos-x64","macos-universal"}.contains(g.platform);
    static const QRegularExpression macExecutable(QStringLiteral("^[^/]+\\.app/Contents/MacOS/[^/]+$"));
    const bool executableValid=windows?g.executable.endsWith(".exe",Qt::CaseInsensitive):mac&&macExecutable.match(g.executable).hasMatch();
    if(!safeId(g.id)||g.id=="launcher"||!safeVersion(g.version)||!safePath(g.executable)||!executableValid||g.name.isEmpty()||g.name.size()>120||g.description.size()>1000||g.notes.size()>30000) fail(QStringLiteral("게임 배포 정보 형식이 올바르지 않습니다."));
    QUrl relative(o["url"].toString()); g.packageUrl=origin.resolved(relative);
    if(!sameOrigin(g.packageUrl,origin)||!g.packageUrl.userInfo().isEmpty()||!g.packageUrl.query().isEmpty()||!g.packageUrl.fragment().isEmpty()) fail(QStringLiteral("게임 파일 주소가 테스트 서버와 다릅니다."));
    g.sha256=hashValue(o); g.bytes=integer(o,"bytes",10ll*1024*1024*1024); g.expandedBytes=integer(o,"expandedBytes",40ll*1024*1024*1024);
    auto a=o["files"].toArray(); if(a.isEmpty()||a.size()>100000||g.bytes==0) fail(QStringLiteral("배포 파일 목록이 올바르지 않습니다."));
    QSet<QString> seen; qint64 sum=0; bool hasExe=false;
    for(auto value:a) {
        const auto f=value.toObject(); Artifact artifact{f["path"].toString(),hashValue(f),integer(f,"bytes",40ll*1024*1024*1024)};
        const auto folded=artifact.path.toCaseFolded(); if(!safePath(artifact.path)||seen.contains(folded)) fail(QStringLiteral("배포 파일 경로가 중복되었거나 안전하지 않습니다."));
        seen.insert(folded); sum+=artifact.bytes; if(artifact.path==g.executable && artifact.bytes>0) hasExe=true; g.files.append(artifact);
    }
    for(const auto &f:g.files) { QString prefix; auto parts=f.path.split('/'); parts.removeLast(); for(const auto &p:parts) { prefix+=p; if(seen.contains(prefix.toCaseFolded())) fail(QStringLiteral("파일과 폴더 경로가 충돌합니다.")); prefix+='/'; } }
    if(!hasExe||sum!=g.expandedBytes||sum>40ll*1024*1024*1024) fail(QStringLiteral("실행 파일 또는 전체 용량 정보가 일치하지 않습니다.")); return g;
}
Catalog verifyCatalog(const QByteArray &bytes,const QString &key,const QUrl &origin,qint64 minimum,bool time) {
    if(bytes.size()>32*1024*1024) fail(QStringLiteral("배포 목록이 너무 큽니다."));
    QJsonParseError e; auto envelope=QJsonDocument::fromJson(bytes,&e).object();
    auto p=QByteArray::fromBase64Encoding(envelope["payload"].toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
    auto s=QByteArray::fromBase64Encoding(envelope["signature"].toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
    if(e.error!=QJsonParseError::NoError||!p||!s||!verifySignature(p.decoded,s.decoded,key)) fail(QStringLiteral("배포자 서명을 확인할 수 없습니다. 초대 정보를 확인하세요."));
    auto doc=QJsonDocument::fromJson(p.decoded,&e); auto o=doc.object();
    if(e.error!=QJsonParseError::NoError||o["schema"].toInt()!=1) fail(QStringLiteral("지원하지 않는 배포 목록입니다."));
    Catalog c; c.sequence=integer(o,"sequence",9007199254740991ll); c.expires=QDateTime::fromString(o["expiresAt"].toString(),Qt::ISODate);
    auto issued=QDateTime::fromString(o["issuedAt"].toString(),Qt::ISODate),now=QDateTime::currentDateTimeUtc();
    if(c.sequence<minimum) fail(QStringLiteral("이전에 확인한 목록보다 오래된 배포입니다."));
    if(!issued.isValid()||!c.expires.isValid()||issued.secsTo(c.expires)<=0||issued.secsTo(c.expires)>8*86400||(time&&(now>c.expires||issued>now.addSecs(300)))) fail(QStringLiteral("배포 목록의 유효기간이 지났습니다. 배포자에게 갱신을 요청하세요."));
    auto games=o["games"].toArray(); if(games.isEmpty()||games.size()>100) fail(QStringLiteral("게임 목록이 비어 있거나 너무 큽니다."));
    QSet<QString> ids;
    for(auto v:games) {
        const auto object=v.toObject();auto g=Game::parse(object,origin);
        if(ids.contains(g.id)) fail(QStringLiteral("같은 게임이 중복 등록되었습니다."));
        ids.insert(g.id);
        if(object.contains("platforms")) {
            if(!object["platforms"].isObject())fail(QStringLiteral("플랫폼별 배포 정보가 올바르지 않습니다."));
            const auto variants=object["platforms"].toObject();
            if(variants.size()>3)fail(QStringLiteral("플랫폼별 배포 정보가 너무 많습니다."));
            QList<Game> compatible;
            for(auto it=variants.constBegin();it!=variants.constEnd();++it) {
                if(!QStringList{"macos-arm64","macos-x64","macos-universal"}.contains(it.key())||!it.value().isObject())
                    fail(QStringLiteral("지원하지 않는 플랫폼별 배포 정보입니다."));
                auto variantObject=it.value().toObject();
                if(variantObject.contains("platforms"))fail(QStringLiteral("플랫폼별 배포 정보는 중첩할 수 없습니다."));
                if(!variantObject.contains("platform"))variantObject["platform"]=it.key();
                auto variant=Game::parse(variantObject,origin);
                if(variant.id!=g.id||variant.name!=g.name||variant.platform!=it.key())
                    fail(QStringLiteral("플랫폼별 배포의 게임 정보가 일치하지 않습니다."));
                if(variant.supported())compatible.append(variant);
            }
#ifdef Q_OS_MACOS
            // Prefer the exact architecture; a universal package is the fallback.
            for(const auto &variant:compatible)if(variant.platform=="macos-universal")g=variant;
            for(const auto &variant:compatible)if(variant.platform!="macos-universal")g=variant;
#endif
        }
        c.games.append(g);
    }
    return c;
}

struct ZipOwner { mz_zip_archive zip{}; ~ZipOwner(){ mz_zip_reader_end(&zip); } };
struct ExtractContext { QFile *file; QCryptographicHash hash{QCryptographicHash::Sha256}; std::atomic_bool *cancel; qint64 written=0,maximum=0; ExtractContext(QFile *f,std::atomic_bool *c,qint64 n):file(f),cancel(c),maximum(n){} };
static size_t extractWrite(void *opaque,mz_uint64 offset,const void *data,size_t size) {
    auto &c=*static_cast<ExtractContext*>(opaque);
    if(*c.cancel || offset!=mz_uint64(c.written)||size>mz_uint64(c.maximum-c.written)) return 0;
    if(c.file->write(static_cast<const char*>(data),qint64(size))!=qint64(size)) return 0;
    c.hash.addData(QByteArrayView(static_cast<const char*>(data),qsizetype(size))); c.written+=qint64(size); return size;
}
QJsonObject inspectArchive(const QString &archive,const QString &dest,const QList<Artifact> &expected,std::atomic_bool &cancel,const Progress &progress) {
    noLinks(dest); if(QFileInfo::exists(dest)) fail(QStringLiteral("설치 준비 폴더가 이미 있습니다."));
    ZipOwner owner; auto &z=owner.zip;
    // A wide fopen keeps Korean and other Unicode installation paths intact.
    FILE *raw{};
#ifdef Q_OS_WIN
    _wfopen_s(&raw,reinterpret_cast<const wchar_t*>(archive.utf16()),L"rb");
#else
    raw=fopen(QFile::encodeName(archive).constData(),"rb");
#endif
    if(!raw) fail(QStringLiteral("다운로드 파일을 열지 못했습니다."));
    struct FileOwner { FILE *f; ~FileOwner(){fclose(f);} } fileOwner{raw};
    if(!mz_zip_reader_init_cfile(&z,raw,0,0)) fail(QStringLiteral("압축 파일이 손상되었습니다."));
    auto count=mz_zip_reader_get_num_files(&z); if(count==0||count>100000) fail(QStringLiteral("압축 파일 수가 허용 범위를 벗어났습니다."));
    QMap<QString,Artifact> manifest; for(const auto &f:expected) manifest.insert(f.path,f);
    QSet<QString> seen; QList<QPair<mz_uint,Artifact>> entries; qint64 total=0;
    for(mz_uint i=0;i<count;++i) {
        mz_zip_archive_file_stat st{}; if(!mz_zip_reader_file_stat(&z,i,&st)) fail(QStringLiteral("압축 파일 정보를 읽지 못했습니다."));
        auto nameBytes=QByteArray(st.m_filename); auto p=QString::fromUtf8(nameBytes);
        const auto nameLength=mz_zip_reader_get_filename(&z,i,nullptr,0);
        if(nameLength>1024 || int(nameLength)!=nameBytes.size()+1 || p.toUtf8()!=nameBytes) fail(QStringLiteral("압축 파일 이름이 올바르지 않습니다."));
        bool directory=st.m_is_directory; if(directory&&p.endsWith('/')) p.chop(1);
        const auto unixType=(st.m_external_attr>>16)&0170000;
        constexpr mz_uint32 reparsePoint=0x400;
        if(!safePath(p)||seen.contains(p.toCaseFolded())||(unixType && unixType!=0100000 && unixType!=0040000)||(st.m_external_attr&reparsePoint)||st.m_is_encrypted||!st.m_is_supported) fail(QStringLiteral("압축 파일에 안전하지 않은 경로 또는 링크가 있습니다."));
        seen.insert(p.toCaseFolded());
        if(directory) { if(st.m_uncomp_size) fail(QStringLiteral("폴더 항목이 올바르지 않습니다.")); continue; }
        if(st.m_uncomp_size>40ull*1024*1024*1024||st.m_uncomp_size/(st.m_comp_size?st.m_comp_size:1)>1000) fail(QStringLiteral("압축 해제 용량이 허용 범위를 벗어났습니다."));
        Artifact a{p,{},qint64(st.m_uncomp_size)};
        if(!expected.isEmpty()) { if(!manifest.contains(p)||manifest[p].bytes!=a.bytes) fail(QStringLiteral("압축 내용이 서명된 파일 목록과 다릅니다.")); a=manifest.take(p); }
        total+=a.bytes; if(total>40ll*1024*1024*1024) fail(QStringLiteral("압축 해제 용량이 너무 큽니다.")); entries.append({i,a});
    }
    if(!manifest.isEmpty()) fail(QStringLiteral("배포 파일이 누락되었습니다."));
    // Reject a file used as another file's parent, before writing anything.
    QSet<QString> fileNames; for(const auto &entry:entries) fileNames.insert(entry.second.path.toCaseFolded());
    for(const auto &entry:entries) { auto parent=entry.second.path.section('/',0,-2); while(!parent.isEmpty()&&parent!=entry.second.path) { if(fileNames.contains(parent.toCaseFolded())) fail(QStringLiteral("압축 경로가 충돌합니다.")); int slash=parent.lastIndexOf('/'); if(slash<0) break; parent=parent.left(slash); } }
    QStorageInfo disk(QFileInfo(dest).dir().absolutePath()); disk.refresh(); if(disk.isValid()&&disk.bytesAvailable()<total+64*1024*1024) fail(QStringLiteral("설치 공간이 부족합니다."));
    if(!QDir().mkpath(dest)) fail(QStringLiteral("설치 폴더를 만들지 못했습니다."));
    QJsonArray files; qint64 done=0;
    for(const auto &entry:entries) {
        if(cancel) fail(QStringLiteral("취소했습니다."));
        const auto &a=entry.second; auto target=QDir(dest).filePath(a.path); noLinks(target); if(!QDir().mkpath(QFileInfo(target).absolutePath())) fail(QStringLiteral("설치 폴더를 만들지 못했습니다."));
        QFile out(target); if(!out.open(QIODevice::WriteOnly|QIODevice::NewOnly)) fail(QStringLiteral("새 파일을 저장할 수 없습니다."));
        ExtractContext ctx(&out,&cancel,a.bytes);
        if(!mz_zip_reader_extract_to_callback(&z,entry.first,extractWrite,&ctx,0)||ctx.written!=a.bytes||!out.flush()) fail(cancel?QStringLiteral("취소했습니다."):QStringLiteral("압축 해제 또는 파일 검증에 실패했습니다."));
        auto hash=QString::fromLatin1(ctx.hash.result().toHex()); if(!a.sha256.isEmpty()&&hash!=a.sha256) fail(QStringLiteral("파일이 배포자의 원본과 다릅니다."));
        QJsonObject file{{"path",a.path},{"bytes",a.bytes},{"sha256",hash}};
#ifdef Q_OS_MACOS
        mz_zip_archive_file_stat modeInfo{};
        if(!mz_zip_reader_file_stat(&z,entry.first,&modeInfo))fail(QStringLiteral("파일 권한을 확인하지 못했습니다."));
        const auto executeBits=(modeInfo.m_external_attr>>16)&0111;
        // Signed archive content may mark executables, never privileged/writable
        // modes. Receipt verification also checks the resulting execute state.
        const mode_t mode=mode_t(0644|executeBits);
        if(fchmod(out.handle(),mode)!=0)fail(QStringLiteral("설치 파일 권한을 설정하지 못했습니다."));
        file["executableFile"]=executeBits!=0;
#endif
        files.append(file); done+=a.bytes; if(progress) progress(done,total);
    }
    return {{"files",files},{"expandedBytes",total}};
}
bool verifyTree(const QString &dir,const QJsonObject &receipt,std::atomic_bool *cancel) {
    try {
        noLinks(dir); auto files=receipt["files"].toArray(); if(files.isEmpty()) return false;
        QSet<QString> expected;
        for(auto value:files) {
            auto o=value.toObject();auto p=o["path"].toString();
            if(!safePath(p)||expected.contains(p.toCaseFolded()))return false;
            expected.insert(p.toCaseFolded());auto path=QDir(dir).filePath(p);noLinks(path);
            if(!QFileInfo(path).isFile()||QFileInfo(path).size()!=o["bytes"].toInteger(-1)||fileHash(path,cancel)!=o["sha256"].toString())return false;
#ifdef Q_OS_MACOS
            struct stat info{};
            if(lstat(QFile::encodeName(path).constData(),&info)!=0||!S_ISREG(info.st_mode)||(info.st_mode&07022))return false;
            if(o.contains("executableFile")&&(!o["executableFile"].isBool()||bool(info.st_mode&0111)!=o["executableFile"].toBool()))return false;
#endif
        }
        QDirIterator it(dir,QDir::Files|QDir::Hidden|QDir::System,QDirIterator::Subdirectories);
        while(it.hasNext()) { auto p=it.next(); noLinks(p); auto relative=QDir(dir).relativeFilePath(p); if(relative!="._teamhjd-receipt.json"&&!expected.contains(relative.toCaseFolded())) return false; }
        return true;
    } catch(const Error&) { if(cancel&&*cancel) throw; return false; }
}
quint64 processStamp(qint64 pid,const QString &expected) {
#ifdef Q_OS_WIN
    HANDLE h=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,DWORD(pid)); if(!h) return 0;
    wchar_t buffer[32768]; DWORD size=32768; FILETIME created{},exited{},kernel{},user{}; quint64 result=0;
    if(WaitForSingleObject(h,0)==WAIT_TIMEOUT && QueryFullProcessImageNameW(h,0,buffer,&size) && GetProcessTimes(h,&created,&exited,&kernel,&user)) {
        if(QDir::cleanPath(QString::fromWCharArray(buffer,int(size))).compare(QDir::cleanPath(expected),Qt::CaseInsensitive)==0) result=(quint64(created.dwHighDateTime)<<32)|created.dwLowDateTime;
    } CloseHandle(h); return result;
#elif defined(Q_OS_MACOS)
    if(pid<=0||pid>std::numeric_limits<pid_t>::max())return 0;
    proc_bsdinfo before{},after{};
    if(proc_pidinfo(pid_t(pid),PROC_PIDTBSDINFO,0,&before,sizeof(before))!=sizeof(before)||before.pbi_uid!=getuid()||before.pbi_status==SZOMB)return 0;
    char path[PROC_PIDPATHINFO_MAXSIZE]{};
    if(proc_pidpath(pid_t(pid),path,sizeof(path))<=0)return 0;
    const auto actual=QFileInfo(QFile::decodeName(path)).canonicalFilePath(),wanted=QFileInfo(expected).canonicalFilePath();
    if(actual.isEmpty()||wanted.isEmpty()||actual!=wanted)return 0;
    if(proc_pidinfo(pid_t(pid),PROC_PIDTBSDINFO,0,&after,sizeof(after))!=sizeof(after)||after.pbi_uid!=before.pbi_uid||after.pbi_status==SZOMB||after.pbi_start_tvsec!=before.pbi_start_tvsec||after.pbi_start_tvusec!=before.pbi_start_tvusec)return 0;
    return before.pbi_start_tvsec*1000000+before.pbi_start_tvusec;
#else
    return 0;
#endif
}
bool motionReduced(){
#ifdef Q_OS_WIN
    BOOL enabled=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&enabled,0);return !enabled;
#elif defined(Q_OS_MACOS)
    CfOwner<CFPropertyListRef> preference(CFPreferencesCopyAppValue(CFSTR("reduceMotion"),CFSTR("com.apple.universalaccess")));
    return preference.get()&&CFGetTypeID(preference.get())==CFBooleanGetTypeID()&&CFBooleanGetValue(static_cast<CFBooleanRef>(preference.get()));
#else
    return false;
#endif
}
}
