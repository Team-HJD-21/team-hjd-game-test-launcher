#pragma once
#include "Core.h"

namespace hjd {
// Only the encrypted native private-key representation is retained between
// requests. It is never exported to QML, URLs, logs, arguments or the server.
class DeviceIdentity {
public:
    static DeviceIdentity create();
    static DeviceIdentity fromJson(const QJsonObject &object,bool allowUnregistered=false);
    QJsonObject toJson() const;
    void registered(const QString &id);
    QString id() const { return m_id; }
    QString publicKey() const { return m_publicKey; }
    QByteArray sign(const QByteArray &message) const;
private:
    QString m_id,m_publicKey;
    QByteArray m_encryptedKey;
};
QByteArray deviceProofMessage(const QString &nonce,const QByteArray &method,const QString &path,
                             const QByteArray &range,const QString &version,const QString &consent);
}
