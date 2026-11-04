#pragma once
#include "core/Core.h"
#include <QJsonDocument>
#include <QCryptographicHash>
#ifdef Q_OS_WIN
#include <windows.h>
#include <bcrypt.h>
#else
#include <Security/Security.h>
#endif
namespace fixture {
using namespace hjd;
#ifdef Q_OS_WIN
struct SigningKey {
    BCRYPT_ALG_HANDLE alg{};BCRYPT_KEY_HANDLE key{};QString publicKey;
    SigningKey(){BCryptOpenAlgorithmProvider(&alg,BCRYPT_ECDSA_P256_ALGORITHM,nullptr,0);BCryptGenerateKeyPair(alg,&key,256,0);BCryptFinalizeKeyPair(key,0);ULONG size=0;BCryptExportKey(key,nullptr,BCRYPT_ECCPUBLIC_BLOB,nullptr,0,&size,0);QByteArray blob(size,0);BCryptExportKey(key,nullptr,BCRYPT_ECCPUBLIC_BLOB,reinterpret_cast<PUCHAR>(blob.data()),size,&size,0);publicKey="p256:"+QString::fromLatin1(blob.mid(sizeof(BCRYPT_ECCKEY_BLOB)).toBase64());}
    ~SigningKey(){BCryptDestroyKey(key);BCryptCloseAlgorithmProvider(alg,0);}
    QByteArray envelope(const QJsonObject &o){auto p=QJsonDocument(o).toJson(QJsonDocument::Compact);auto hash=QCryptographicHash::hash(p,QCryptographicHash::Sha256);ULONG size=0;BCryptSignHash(key,nullptr,reinterpret_cast<PUCHAR>(hash.data()),ULONG(hash.size()),nullptr,0,&size,0);QByteArray signature(size,0);BCryptSignHash(key,nullptr,reinterpret_cast<PUCHAR>(hash.data()),ULONG(hash.size()),reinterpret_cast<PUCHAR>(signature.data()),size,&size,0);return QJsonDocument(QJsonObject{{"payload",QString::fromLatin1(p.toBase64())},{"signature",QString::fromLatin1(signature.toBase64())}}).toJson();}
};

#else
template<class T> class OwnedCF {
public:
    explicit OwnedCF(T value = nullptr): m_value(value) {}
    ~OwnedCF() { if(m_value) CFRelease(m_value); }
    OwnedCF(const OwnedCF &) = delete;
    OwnedCF &operator=(const OwnedCF &) = delete;
    T get() const { return m_value; }
private:
    T m_value;
};

QByteArray rawSignature(const QByteArray &der) {
    // Security returns a short-form X9.62 DER sequence for a 256-bit curve.
    // Decode independently from the product's raw -> DER implementation.
    int offset = 0;
    auto byte = [&]() -> unsigned char {
        if(offset >= der.size()) fail("fixture signature DER truncated");
        return static_cast<unsigned char>(der[offset++]);
    };
    if(byte() != 0x30 || byte() != der.size() - 2) fail("fixture signature sequence");
    QByteArray result;
    for(int i = 0; i != 2; ++i) {
        if(byte() != 2) fail("fixture signature integer");
        const int size = byte();
        if(size < 1 || size > 33 || offset + size > der.size()) fail("fixture signature size");
        auto integer = der.mid(offset, size); offset += size;
        if(integer.size() == 33) {
            if(integer[0] != 0) fail("fixture integer overflow");
            integer.remove(0, 1);
        }
        result += QByteArray(32 - integer.size(), '\0') + integer;
    }
    if(offset != der.size()) fail("fixture signature trailing bytes");
    return result;
}
struct SigningKey {
    OwnedCF<SecKeyRef> key;
    QString publicKey;
    SigningKey(): key(create()) {
        OwnedCF<SecKeyRef> pub(SecKeyCopyPublicKey(key.get()));
        CFErrorRef error = nullptr;
        OwnedCF<CFDataRef> exported(SecKeyCopyExternalRepresentation(pub.get(), &error));
        OwnedCF<CFErrorRef> ownedError(error);
        if(!exported.get() || CFDataGetLength(exported.get()) != 65 || CFDataGetBytePtr(exported.get())[0] != 4)
            fail("fixture public key export");
        QByteArray point(reinterpret_cast<const char *>(CFDataGetBytePtr(exported.get()) + 1), 64);
        publicKey = "p256:" + QString::fromLatin1(point.toBase64());
    }
    static SecKeyRef create() {
        int bits = 256;
        OwnedCF<CFNumberRef> number(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &bits));
        const void *keys[] = {kSecAttrKeyType, kSecAttrKeySizeInBits};
        const void *values[] = {kSecAttrKeyTypeECSECPrimeRandom, number.get()};
        OwnedCF<CFDictionaryRef> attrs(CFDictionaryCreate(kCFAllocatorDefault, keys, values, 2,
                                                        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        CFErrorRef error = nullptr;
        auto key = SecKeyCreateRandomKey(attrs.get(), &error);
        OwnedCF<CFErrorRef> ownedError(error);
        if(!key) fail("fixture signing key creation");
        return key;
    }
    QByteArray sign(const QByteArray &payload) const {
        OwnedCF<CFDataRef> data(CFDataCreate(kCFAllocatorDefault,
            reinterpret_cast<const UInt8 *>(payload.constData()), payload.size()));
        CFErrorRef error = nullptr;
        OwnedCF<CFDataRef> der(SecKeyCreateSignature(key.get(), kSecKeyAlgorithmECDSASignatureMessageX962SHA256,
                                                   data.get(), &error));
        OwnedCF<CFErrorRef> ownedError(error);
        if(!der.get()) fail("fixture signature creation");
        return rawSignature(QByteArray(reinterpret_cast<const char *>(CFDataGetBytePtr(der.get())), CFDataGetLength(der.get())));
    }
    QByteArray envelope(const QJsonObject &object) const {
        const auto payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
        return QJsonDocument(QJsonObject{{"payload", QString::fromLatin1(payload.toBase64())},
            {"signature", QString::fromLatin1(sign(payload).toBase64())}}).toJson(QJsonDocument::Compact);
    }
};


#endif
}
