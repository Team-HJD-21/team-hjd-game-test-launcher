#pragma once
#include <QByteArray>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QUrl>
#include <atomic>
#include <functional>
#include <memory>
#include <stdexcept>

namespace hjd {
struct Error : std::runtime_error { explicit Error(const QString &s) : std::runtime_error(s.toUtf8().constData()) {} };
[[noreturn]] void fail(const QString &message);
QByteArray readFile(const QString &path, qint64 limit = 32 * 1024 * 1024);
QJsonObject readJson(const QString &path);
void writeJson(const QString &path, const QJsonObject &object);
void atomicWrite(const QString &path, const QByteArray &bytes);
QString fileHash(const QString &path, std::atomic_bool *cancel = nullptr);
bool safePath(const QString &path);
bool safeId(const QString &id);
bool safeVersion(const QString &version);
void noLinks(const QString &path);
QByteArray protect(const QByteArray &plain);
QByteArray unprotect(const QByteArray &encrypted);
bool verifySignature(const QByteArray &payload, const QByteArray &signature, const QString &key);

struct Artifact { QString path, sha256; qint64 bytes = 0; };
struct Game {
    QString id, name, description, version, executable, notes;
    QString platform = QStringLiteral("windows-x64");
    QUrl packageUrl;
    QString sha256;
    qint64 bytes = 0, expandedBytes = 0;
    QList<Artifact> files;
    QJsonObject toJson() const;
    bool supported() const;
    static Game parse(const QJsonObject &object, const QUrl &origin);
};
struct Catalog { QList<Game> games; qint64 sequence = 0; QDateTime expires; };
Catalog verifyCatalog(const QByteArray &envelope, const QString &key, const QUrl &origin,
                      qint64 minimumSequence = 0, bool enforceTime = true);
bool sameOrigin(const QUrl &a, const QUrl &b);
bool allowedServer(const QUrl &url, bool testLoopback = false);
struct Invite {
    QUrl server;
    QString publisherKey, label;
    QByteArray certificate, accessToken;
    QDateTime expires;
    static Invite parse(const QByteArray &json, bool testLoopback = false);
};
QJsonObject inspectArchive(const QString &archive, const QString &destination, const QList<Artifact> &expected,
                          std::atomic_bool &cancel, const std::function<void(qint64,qint64)> &progress = {});
bool verifyTree(const QString &directory, const QJsonObject &receipt, std::atomic_bool *cancel = nullptr);

struct NetConfig { QUrl server; QByteArray password; QByteArray caPem; bool testLoopback = false; QByteArray cloudflareToken; };
using Progress = std::function<void(qint64,qint64)>;
class Http {
public:
    static QByteArray get(const QUrl &url, const NetConfig &config, std::atomic_bool &cancel, qint64 maximum = 32*1024*1024,
                          bool publicDownload = false);
    static void download(const QUrl &url, const NetConfig &config, const QString &path, qint64 bytes,
                         const QString &sha256, std::atomic_bool &cancel, const Progress &progress,
                         bool publicDownload = false);
};

class Store {
public:
    explicit Store(QString root);
    QString root() const { return m_root; }
    QJsonObject state(const QString &id) const;
    QString currentDirectory(const QString &id) const;
    QJsonObject currentReceipt(const QString &id) const;
    bool running(const QString &id) const;
    void install(const Game &game, const QString &archive, std::atomic_bool &cancel, const Progress &progress = {}, bool repair = false);
    void uninstall(const QString &id);
    void rollback(const QString &id);
    bool verify(const QString &id, std::atomic_bool *cancel = nullptr) const;
    qint64 launch(const QString &id);
    void activate(const QString &id, const QString &versionFolder);
    QString stagePayload(const QString &version, const QString &sha256, const QString &archive, std::atomic_bool &cancel);
private:
    QString gameRoot(const QString &id) const;
    QString m_root;
};
quint64 processStamp(qint64 pid, const QString &expectedExecutable);
bool motionReduced();
}
