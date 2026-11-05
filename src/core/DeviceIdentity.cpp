#include "DeviceIdentity.h"
#include <QCryptographicHash>
#include <QRegularExpression>
#include <limits>
#ifdef Q_OS_WIN
#include <windows.h>
#include <bcrypt.h>
#else
#include <Security/Security.h>
#endif

namespace hjd {
namespace {
struct Secret {
    QByteArray bytes;
    ~Secret(){volatile char *p=bytes.data();for(qsizetype i=0;i<bytes.size();++i)p[i]=0;}
};
#ifdef Q_OS_WIN
struct Algorithm {
    BCRYPT_ALG_HANDLE value{};
    Algorithm(){if(BCryptOpenAlgorithmProvider(&value,BCRYPT_ECDSA_P256_ALGORITHM,nullptr,0)<0)fail(QStringLiteral("기기 인증 키를 준비하지 못했습니다."));}
    ~Algorithm(){if(value)BCryptCloseAlgorithmProvider(value,0);}
};
struct Key {BCRYPT_KEY_HANDLE value{};~Key(){if(value)BCryptDestroyKey(value);}};
QByteArray exportKey(BCRYPT_KEY_HANDLE key,LPCWSTR type){
    ULONG size=0;
    if(BCryptExportKey(key,nullptr,type,nullptr,0,&size,0)<0||size>256)fail(QStringLiteral("기기 인증 키를 확인하지 못했습니다."));
    QByteArray result(size,0);
    if(BCryptExportKey(key,nullptr,type,reinterpret_cast<PUCHAR>(result.data()),size,&size,0)<0)fail(QStringLiteral("기기 인증 키를 보관하지 못했습니다."));
    return result;
}
#else
template<class T> struct Owned {
    T value{};
    explicit Owned(T v=nullptr):value(v){}
    ~Owned(){if(value)CFRelease(value);}
    Owned(const Owned&)=delete;
};
CFDataRef data(const QByteArray &bytes){return CFDataCreate(kCFAllocatorDefault,reinterpret_cast<const UInt8*>(bytes.constData()),bytes.size());}
CFDictionaryRef attributes(bool importing){
    int bits=256;Owned<CFNumberRef> size(CFNumberCreate(kCFAllocatorDefault,kCFNumberIntType,&bits));
    const void *keys[]={kSecAttrKeyType,kSecAttrKeySizeInBits,kSecAttrKeyClass};
    const void *values[]={kSecAttrKeyTypeECSECPrimeRandom,size.value,kSecAttrKeyClassPrivate};
    return CFDictionaryCreate(kCFAllocatorDefault,keys,values,importing?3:2,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
}
QByteArray external(SecKeyRef key){
    CFErrorRef error=nullptr;Owned<CFDataRef> raw(SecKeyCopyExternalRepresentation(key,&error));Owned<CFErrorRef> ownedError(error);
    if(!raw.value||CFDataGetLength(raw.value)>256)fail(QStringLiteral("기기 인증 키를 확인하지 못했습니다."));
    return QByteArray(reinterpret_cast<const char*>(CFDataGetBytePtr(raw.value)),CFDataGetLength(raw.value));
}
QByteArray rawSignature(const QByteArray &der){
    int offset=0;auto byte=[&](){if(offset>=der.size())fail(QStringLiteral("기기 서명을 만들지 못했습니다."));return static_cast<unsigned char>(der[offset++]);};
    if(byte()!=0x30||byte()!=der.size()-2)fail(QStringLiteral("기기 서명 형식이 올바르지 않습니다."));
    QByteArray result;
    for(int i=0;i<2;++i){
        if(byte()!=2)fail(QStringLiteral("기기 서명 형식이 올바르지 않습니다."));
        const int size=byte();if(size<1||size>33||offset+size>der.size())fail(QStringLiteral("기기 서명 형식이 올바르지 않습니다."));
        auto integer=der.mid(offset,size);offset+=size;
        if(integer.size()==33){if(integer[0]!=0)fail(QStringLiteral("기기 서명 형식이 올바르지 않습니다."));integer.remove(0,1);}
        result+=QByteArray(32-integer.size(),0)+integer;
    }
    if(offset!=der.size())fail(QStringLiteral("기기 서명 형식이 올바르지 않습니다."));
    return result;
}
#endif
}

DeviceIdentity DeviceIdentity::create(){
    DeviceIdentity device;Secret secret;
#ifdef Q_OS_WIN
    Algorithm algorithm;Key key;
    if(BCryptGenerateKeyPair(algorithm.value,&key.value,256,0)<0||BCryptFinalizeKeyPair(key.value,0)<0)fail(QStringLiteral("기기 인증 키를 만들지 못했습니다."));
    secret.bytes=exportKey(key.value,BCRYPT_ECCPRIVATE_BLOB);
    const auto publicBlob=exportKey(key.value,BCRYPT_ECCPUBLIC_BLOB);
    if(publicBlob.size()!=72||secret.bytes.size()!=104)fail(QStringLiteral("기기 키 형식이 올바르지 않습니다."));
    device.m_publicKey="p256:"+QString::fromLatin1(publicBlob.mid(8).toBase64());
#else
    Owned<CFDictionaryRef> attrs(attributes(false));CFErrorRef error=nullptr;
    Owned<SecKeyRef> key(SecKeyCreateRandomKey(attrs.value,&error));Owned<CFErrorRef> ownedError(error);
    if(!key.value)fail(QStringLiteral("기기 인증 키를 만들지 못했습니다."));
    secret.bytes=external(key.value);Owned<SecKeyRef> pub(SecKeyCopyPublicKey(key.value));
    if(!pub.value)fail(QStringLiteral("기기 공개키를 확인하지 못했습니다."));
    const auto point=external(pub.value);
    if(point.size()!=65||point[0]!=4||secret.bytes.size()!=97)fail(QStringLiteral("기기 키 형식이 올바르지 않습니다."));
    device.m_publicKey="p256:"+QString::fromLatin1(point.mid(1).toBase64());
#endif
    device.m_encryptedKey=protect(secret.bytes);
    return device;
}
void DeviceIdentity::registered(const QString &id){
    static const QRegularExpression valid("^[A-Za-z0-9_-]{32}\\z");
    if(id.size()!=32||!valid.match(id).hasMatch())fail(QStringLiteral("기기 등록 응답이 올바르지 않습니다."));m_id=id;
}
QJsonObject DeviceIdentity::toJson() const{return {{"schema",1},{"id",m_id},{"publicKey",m_publicKey},{"keyBlob",QString::fromLatin1(m_encryptedKey.toBase64())}};}
DeviceIdentity DeviceIdentity::fromJson(const QJsonObject &object,bool allowUnregistered){
    DeviceIdentity device;
    if(object["schema"]!=QJsonValue(1))fail(QStringLiteral("저장된 기기 로그인 형식이 올바르지 않습니다."));
    if(!allowUnregistered||!object["id"].toString().isEmpty())device.registered(object["id"].toString());
    device.m_publicKey=object["publicKey"].toString();
    if(device.m_publicKey.size()!=93||!device.m_publicKey.startsWith("p256:"))fail(QStringLiteral("저장된 기기 공개키가 올바르지 않습니다."));
    const auto encoded=object["keyBlob"].toString();
    if(encoded.size()>4096)fail(QStringLiteral("저장된 기기 인증 정보가 너무 큽니다."));
    auto decoded=QByteArray::fromBase64Encoding(encoded.toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
    if(!decoded||decoded.decoded.isEmpty())fail(QStringLiteral("저장된 기기 인증 정보가 올바르지 않습니다."));
    device.m_encryptedKey=decoded.decoded;
    const QByteArray probe="TeamHJD device key validation v1";
    if(!verifySignature(probe,device.sign(probe),device.m_publicKey))fail(QStringLiteral("이 PC의 기기 인증 키를 확인하지 못했습니다. 이메일로 다시 인증해 주세요."));
    return device;
}
QByteArray DeviceIdentity::sign(const QByteArray &message) const {
    if(message.size()>4096)fail(QStringLiteral("기기 인증 요청이 너무 큽니다."));
    Secret secret;secret.bytes=unprotect(m_encryptedKey);
#ifdef Q_OS_WIN
    if(secret.bytes.size()!=104)fail(QStringLiteral("저장된 기기 키를 확인하지 못했습니다."));
    Algorithm algorithm;Key key;
    if(BCryptImportKeyPair(algorithm.value,nullptr,BCRYPT_ECCPRIVATE_BLOB,&key.value,reinterpret_cast<PUCHAR>(secret.bytes.data()),ULONG(secret.bytes.size()),0)<0)fail(QStringLiteral("기기 인증 키를 열지 못했습니다."));
    const auto digest=QCryptographicHash::hash(message,QCryptographicHash::Sha256);QByteArray signature(64,0);ULONG size=0;
    if(BCryptSignHash(key.value,nullptr,reinterpret_cast<PUCHAR>(const_cast<char*>(digest.constData())),ULONG(digest.size()),reinterpret_cast<PUCHAR>(signature.data()),64,&size,0)<0||size!=64)fail(QStringLiteral("기기 인증 요청에 서명하지 못했습니다."));
    return signature;
#else
    if(secret.bytes.size()!=97)fail(QStringLiteral("저장된 기기 키를 확인하지 못했습니다."));
    Owned<CFDataRef> raw(data(secret.bytes));Owned<CFDictionaryRef> attrs(attributes(true));CFErrorRef keyError=nullptr;
    Owned<SecKeyRef> key(SecKeyCreateWithData(raw.value,attrs.value,&keyError));Owned<CFErrorRef> ownedKeyError(keyError);
    if(!key.value)fail(QStringLiteral("기기 인증 키를 열지 못했습니다."));
    Owned<CFDataRef> payload(data(message));CFErrorRef error=nullptr;
    Owned<CFDataRef> signature(SecKeyCreateSignature(key.value,kSecKeyAlgorithmECDSASignatureMessageX962SHA256,payload.value,&error));Owned<CFErrorRef> ownedError(error);
    if(!signature.value)fail(QStringLiteral("기기 인증 요청에 서명하지 못했습니다."));
    return rawSignature(QByteArray(reinterpret_cast<const char*>(CFDataGetBytePtr(signature.value)),CFDataGetLength(signature.value)));
#endif
}
QByteArray deviceProofMessage(const QString &nonce,const QByteArray &method,const QString &path,const QByteArray &range,const QString &version,const QString &consent){
    static const QRegularExpression valid("^[A-Za-z0-9_-]{43}\\z");
    if(nonce.size()!=43||!valid.match(nonce).hasMatch()||!QList<QByteArray>{"GET","HEAD","POST"}.contains(method)||!path.startsWith("/client-device/")||path.contains('?')||path.contains('#'))fail(QStringLiteral("기기 인증 요청이 올바르지 않습니다."));
    const QStringList fields{nonce,QString::fromLatin1(method),path,QString::fromLatin1(range),version,consent};
    for(const auto &field:fields){if(field.size()>512)fail(QStringLiteral("기기 인증 요청이 너무 큽니다."));for(auto c:field)if(c.unicode()<32||c.unicode()>126)fail(QStringLiteral("기기 인증 요청 형식이 올바르지 않습니다."));}
    return "TeamHJD-Device-v1\n"+fields.join('\n').toLatin1();
}
}
