#pragma once
#include "core/Core.h"
#include "core/Platform.h"
#include "core/LauncherUpdate.h"
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
    Q_PROPERTY(QString launcherInstallRoot READ launcherInstallRoot CONSTANT)
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
    QString launcherInstallRoot() const;
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
    Q_INVOKABLE void checkLauncherUpdate(bool force=false);
    Q_INVOKABLE void applyLauncherUpdate();
    Q_INVOKABLE void restartForUpdate();
    Q_INVOKABLE void retryFailedUpdate();
    Q_INVOKABLE void showLicenses();
    Q_INVOKABLE QString privacyNotice() const;
    Q_INVOKABLE void acceptPrivacy(bool collection,bool overseas,bool ageConfirmed);
    Q_INVOKABLE void forgetLogin();
    Q_INVOKABLE bool canClose() const { return !m_busy||m_background; }
signals:
    void changed();
    void settingsChanged();
    void requestSettings();
    void requestInvite();
    void requestLogin();
    void inviteAccepted();
    void quitForUpdate();
    void authenticationCompleted();
    void toast(const QString &title,const QString &message,const QString &tone);
private:
    friend class LauncherUpdaterFixture;
    friend class ClientExperienceFixture;
    // Private transport seam for signed, isolated regression fixtures. There
    // is no command-line, settings, QObject or QML access to these callbacks.
    QString m_releaseKey;
    std::function<QByteArray(const QUrl&)> m_updateFetch;
    std::function<void(const QJsonObject&,const QString&)> m_updateDownload;
    struct WorkResult { QString message; hjd::FailureCode code=hjd::FailureCode::General; };
    void work(std::function<void()> job,std::function<void(const QString&)> completed);
    void install(bool force);
    void load();
    void save();
    bool needsLogin() const;
    void updateProgress(qint64 done,qint64 total,const QString &phase);
    bool deferDuringCheck(std::function<void()> action);
    void updateFailed(const QString &error);
    void finishLogin(const hjd::NetConfig &config,const QDateTime &expires,const hjd::Catalog &catalog,const QByteArray &raw,const QString &error);
    hjd::updates::Release latestLauncherRelease(QByteArray *raw,qint64 minimum);
    const hjd::Game *selected() const;
    QString m_dataRoot,m_launcherRoot,m_gamesRoot,m_key,m_selected="hose-for-hire";
    hjd::NetConfig m_config;
    hjd::Catalog m_catalog;
    bool m_busy=false,m_online=false,m_reduceMotion=false,m_background=false,m_deleting=false;
    QString m_status,m_error,m_phase,m_transfer,m_updateMessage;
    double m_progress=0;
    qint64 m_minSequence=0;
    std::atomic_bool m_cancel{false};
    QFuture<WorkResult> m_future;
    QTimer m_runningTimer;
    QTimer m_autoTimer;
    QTimer m_updateRetry;
    QJsonObject m_updateAsset;
    QString m_updateVersion;
    QString m_testUpdateFrom;
    QByteArray m_releaseEnvelope;
    qint64 m_updateSequence=0;
    int m_updateAttempts=0;
    bool m_updateVerified=false;
    bool m_launcherChecking=false;
    bool m_serverUpgradeRequired=false;
    bool m_updateInstaller=false;
    bool m_knownFailedRelease=false,m_retryFailedOnce=false;
    QString m_readyInstaller;
    QString m_readyCandidate;
    qint64 m_progressStart=0;
    qint64 m_lastRefresh=0,m_lastLauncherCheck=0;
    bool m_wasRunning=false;
    QJsonObject m_updateHolds;
    QJsonObject m_updateNotices;
    QByteArray m_pendingCa;
    bool m_hasPendingCa=false;
    QJsonObject m_inviteInfo;
    bool m_cloudflare=false;
    bool m_hadSession=false,m_checkingServer=false,m_authNoticeShown=false;
    bool m_offlineMode=false,m_authenticating=false;
    QString m_updateGate,m_updateError;
    QDateTime m_authExpires;
    QDateTime m_policyAcceptedAt;
#ifdef Q_OS_MACOS
    QByteArray m_pendingPassword,m_pendingSession;
#endif
};
