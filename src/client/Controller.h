#pragma once
#include "core/Core.h"
#include "core/Platform.h"
#include <QFuture>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

class Controller : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap view READ view NOTIFY changed)
    Q_PROPERTY(QVariantList library READ library NOTIFY changed)
    Q_PROPERTY(QString serverUrl READ serverUrl NOTIFY settingsChanged)
    Q_PROPERTY(QString publisherKey READ publisherKey NOTIFY settingsChanged)
    Q_PROPERTY(QString installRoot READ installRoot NOTIFY settingsChanged)
    Q_PROPERTY(bool reduceMotion READ reduceMotion NOTIFY settingsChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString platform READ platform CONSTANT)
public:
    Controller(QString dataRoot,QString launcherRoot,bool testLoopback,QObject *parent=nullptr);
    ~Controller() override;
    QVariantMap view() const;
    QVariantList library() const;
    QString serverUrl() const { return m_config.server.toString(); }
    QString publisherKey() const { return m_key; }
    QString installRoot() const { return m_gamesRoot; }
    bool reduceMotion() const { return m_reduceMotion; }
    QString version() const { return QStringLiteral(HJD_VERSION); }
    QString platform() const { return hjd::platformLabel(); }
    // Only changes the update comparison for an explicitly isolated test run.
    // It never changes the reported version, update source, digest or TLS rules.
    void setTestUpdateFrom(const QString &version);
    void restoreCredentials();
    Q_INVOKABLE void selectGame(const QString &id);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void primary();
    Q_INVOKABLE void playInstalled();
    Q_INVOKABLE void login();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void repair();
    Q_INVOKABLE void rollback();
    Q_INVOKABLE void uninstall();
    Q_INVOKABLE void openFolder(const QString &which);
    Q_INVOKABLE bool saveSettings(const QString &url,const QString &password,const QString &key,const QString &root,bool reduced);
    Q_INVOKABLE QVariantMap importInvite(const QUrl &file);
    Q_INVOKABLE QString localPath(const QUrl &url) const { return url.toLocalFile(); }
    Q_INVOKABLE void checkLauncherUpdate();
    Q_INVOKABLE void applyLauncherUpdate();
    Q_INVOKABLE void showLicenses();
    Q_INVOKABLE bool canClose() const { return !m_busy||m_background; }
signals:
    void changed();
    void settingsChanged();
    void requestSettings();
    void requestInvite();
    void requestLogin();
    void inviteAccepted();
    void quitForUpdate();
    void toast(const QString &title,const QString &message,const QString &tone);
private:
    void work(std::function<void()> job,std::function<void(const QString&)> completed);
    void install(bool force);
    void load();
    void save();
    bool needsLogin() const;
    void updateProgress(qint64 done,qint64 total,const QString &phase);
    bool deferDuringCheck(std::function<void()> action);
    const hjd::Game *selected() const;
    QString m_dataRoot,m_launcherRoot,m_gamesRoot,m_key,m_selected="hose-for-hire";
    hjd::NetConfig m_config;
    hjd::Catalog m_catalog;
    bool m_busy=false,m_online=false,m_reduceMotion=false,m_background=false,m_deleting=false;
    QString m_status,m_error,m_phase,m_transfer,m_updateMessage;
    double m_progress=0;
    qint64 m_minSequence=0;
    std::atomic_bool m_cancel{false};
    QFuture<QString> m_future;
    QTimer m_runningTimer;
    QTimer m_autoTimer;
    QJsonObject m_updateAsset;
    QString m_updateVersion;
    QString m_testUpdateFrom;
    qint64 m_progressStart=0;
    qint64 m_lastRefresh=0,m_lastLauncherCheck=0;
    bool m_wasRunning=false;
    QJsonObject m_updateHolds;
    QJsonObject m_updateNotices;
    QByteArray m_pendingCa;
    bool m_hasPendingCa=false;
    QJsonObject m_inviteInfo;
    bool m_cloudflare=false;
    QDateTime m_authExpires;
#ifdef Q_OS_MACOS
    QByteArray m_pendingPassword,m_pendingSession;
#endif
};
