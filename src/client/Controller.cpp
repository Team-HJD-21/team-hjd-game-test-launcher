#include "Controller.h"
#include "DefaultChannel.h"
#include "Privacy.h"
#include "ConnectionState.h"
#include "core/CloudflareLogin.h"
#include "core/LauncherUpdate.h"
#include "core/BootstrapUpdate.h"
#include <QDateTime>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QProcess>
#include <QSet>
#include <QRegularExpression>
#include <QFile>
#include <QStandardPaths>
#include <QVersionNumber>
#include <QRandomGenerator>
#include <QtConcurrent>

using namespace hjd;
namespace {
QString exceptionText(const std::exception &e){return QString::fromUtf8(e.what());}
QString sizeText(qint64 n){return n>=1024*1024*1024?QString::number(double(n)/(1024*1024*1024),'f',2)+" GB":QString::number(double(n)/(1024*1024),'f',1)+" MB";}
}
Controller::Controller(QString data,QString launcher,bool test,QObject *parent):QObject(parent),m_dataRoot(std::move(data)),m_launcherRoot(std::move(launcher)) {
    m_releaseKey=QString::fromLatin1(updates::publicKey);
    m_config.testLoopback=test; m_gamesRoot=m_dataRoot+"/games"; m_reduceMotion=motionReduced();
    if(!m_launcherRoot.isEmpty())m_updateGate="checking";
    m_updateRetry.setSingleShot(true);
    connect(&m_updateRetry,&QTimer::timeout,this,[this]{if(!m_offlineMode&&!m_busy)checkLauncherUpdate(true);else if(!m_offlineMode)m_updateRetry.start(1000);});
    try { load(); } catch(const std::exception &e) { m_error=exceptionText(e); }
    m_runningTimer.setInterval(1500); connect(&m_runningTimer,&QTimer::timeout,this,[this]{try{bool active=Store(m_gamesRoot).running(m_selected);if(active!=m_wasRunning){if(m_wasRunning)emit toast(QStringLiteral("게임을 종료했어요"),QStringLiteral("새 버전은 업데이트 버튼을 눌러 적용하세요."),"info");m_wasRunning=active;emit changed();if(!active&&!m_busy&&!m_config.server.isEmpty())refresh();}}catch(const Error&){} }); m_runningTimer.start();
    m_autoTimer.setInterval(30000);connect(&m_autoTimer,&QTimer::timeout,this,[this]{if(m_busy||!m_updateGate.isEmpty()||m_offlineMode)return;auto now=QDateTime::currentSecsSinceEpoch();if(now-m_lastLauncherCheck>=900&&!m_launcherRoot.isEmpty())checkLauncherUpdate();else if(!m_config.server.isEmpty()&&now-m_lastRefresh>=(m_online?120:30))refresh();});m_autoTimer.start();
    connect(qGuiApp,&QGuiApplication::applicationStateChanged,this,[this](Qt::ApplicationState state){if(state==Qt::ApplicationActive&&!m_busy&&m_updateGate.isEmpty()&&!m_offlineMode&&!m_config.server.isEmpty()&&QDateTime::currentSecsSinceEpoch()-m_lastRefresh>15)refresh();});
    QTimer::singleShot(400,this,[this]{if(!m_launcherRoot.isEmpty())checkLauncherUpdate(true);else if(!m_config.server.isEmpty())refresh();});
}
Controller::~Controller(){m_cancel=true;if(m_future.isStarted())m_future.waitForFinished();}
void Controller::load() {
    m_config.server=QUrl(channel::server);m_key=channel::publisherKey;m_cloudflare=true;
    if(QFileInfo::exists(m_dataRoot+"/launcher-feed-state.json")){
        auto state=readJson(m_dataRoot+"/launcher-feed-state.json");
        auto sequence=state["sequence"].toInteger(-1);
        if(sequence<1)fail(QStringLiteral("런처 업데이트 기록이 손상되었습니다. 공식 설치 프로그램으로 복구해 주세요."));
        m_updateSequence=sequence;
    }
    if(!QFileInfo::exists(m_dataRoot+"/settings.json")) return;
    auto o=readJson(m_dataRoot+"/settings.json");
    m_config.server=QUrl(o["server"].toString());
#ifdef Q_OS_MACOS
    // Updated ad hoc apps can need Keychain consent. Show the first frame and
    // acknowledge bootstrap health before any potentially interactive lookup.
    m_pendingPassword=QByteArray::fromBase64(o["passwordBlob"].toString().toLatin1());
#else
    m_config.password=unprotect(QByteArray::fromBase64(o["passwordBlob"].toString().toLatin1()));
#endif
    m_config.caPem=QByteArray::fromBase64(o["certificate"].toString().toLatin1());
    m_key=o["publisherKey"].toString(); m_gamesRoot=o["gamesRoot"].toString(m_gamesRoot); m_reduceMotion=o["reduceMotion"].toBool(m_reduceMotion); m_minSequence=o["sequence"].toInteger();
    m_updateHolds=o["updateHolds"].toObject();
    m_inviteInfo=o["inviteInfo"].toObject();
    m_cloudflare=m_config.server==QUrl(channel::server)&&m_key==channel::publisherKey;
    auto accepted=o["privacyAcceptedAt"].toString();auto at=QDateTime::fromString(accepted,Qt::ISODate);
    if(m_cloudflare&&o["privacyVersion"].toString()==privacy::version&&accepted.endsWith('Z')&&at.isValid()&&at<=QDateTime::currentDateTimeUtc().addSecs(30)){
        m_config.policyVersion=privacy::version;m_policyAcceptedAt=at;
    }
    if(m_cloudflare&&!o["cloudflareBlob"].toString().isEmpty()){
        m_hadSession=true;
#ifdef Q_OS_MACOS
        m_pendingSession=QByteArray::fromBase64(o["cloudflareBlob"].toString().toLatin1());
#else
        try{auto session=CloudflareLogin::inspect(unprotect(QByteArray::fromBase64(o["cloudflareBlob"].toString().toLatin1())),channel::issuer,channel::audience);m_config.cloudflareToken=session.jwt;m_authExpires=session.expires;}catch(const Error&){m_config.cloudflareToken.clear();}
#endif
    }
    if(!allowedServer(m_config.server,m_config.testLoopback)) fail(QStringLiteral("저장된 서버 주소가 올바르지 않습니다."));
    if(QFileInfo::exists(m_dataRoot+"/catalog-cache.json")) {
        try {m_catalog=verifyCatalog(readFile(m_dataRoot+"/catalog-cache.json"),m_key,m_config.server,m_minSequence,false); if(!m_catalog.games.isEmpty()) m_selected=m_catalog.games.first().id;} catch(const Error&) {m_catalog={};}
    }
}
void Controller::restoreCredentials(){
#ifdef Q_OS_MACOS
    try{
        m_config.password=unprotect(m_pendingPassword);m_pendingPassword.clear();
        if(!m_pendingSession.isEmpty()){
            auto session=CloudflareLogin::inspect(unprotect(m_pendingSession),channel::issuer,channel::audience);
            m_config.cloudflareToken=session.jwt;m_authExpires=session.expires;
        }
        m_pendingSession.clear();emit changed();
        if(!m_config.server.isEmpty())refresh();
    }catch(const Error &e){m_pendingPassword.clear();m_pendingSession.clear();m_config.cloudflareToken.clear();m_error=exceptionText(e);emit changed();}
#endif
}
void Controller::save(){writeJson(m_dataRoot+"/settings.json",{{"privacyVersion",m_config.policyVersion},{"privacyAcceptedAt",m_policyAcceptedAt.toUTC().toString(Qt::ISODate)},{"server",m_config.server.toString()},{"passwordBlob",QString::fromLatin1(protect(m_config.password).toBase64())},{"cloudflareBlob",m_config.cloudflareToken.isEmpty()?QString():QString::fromLatin1(protect(m_config.cloudflareToken).toBase64())},{"certificate",QString::fromLatin1(m_config.caPem.toBase64())},{"publisherKey",m_key},{"gamesRoot",m_gamesRoot},{"reduceMotion",m_reduceMotion},{"sequence",m_minSequence},{"updateHolds",m_updateHolds},{"inviteInfo",m_inviteInfo}});}
bool Controller::needsLogin() const{return m_cloudflare&&(m_config.cloudflareToken.isEmpty()||m_authExpires<=QDateTime::currentDateTimeUtc().addSecs(60));}
const Game *Controller::selected() const {for(const auto &g:m_catalog.games) if(g.id==m_selected) return &g; return nullptr;}
QVariantList Controller::library() const {
    QVariantList list;QSet<QString> ids; for(const auto &g:m_catalog.games){list.append(QVariantMap{{"id",g.id},{"name",g.name},{"selected",g.id==m_selected}});ids.insert(g.id);}
    for(const auto &id:QDir(m_gamesRoot).entryList(QDir::Dirs|QDir::NoDotAndDotDot))if(safeId(id)&&!ids.contains(id)){try{auto receipt=Store(m_gamesRoot).currentReceipt(id);if(!receipt.isEmpty())list.append(QVariantMap{{"id",id},{"name",receipt["name"].toString(id)},{"selected",id==m_selected}});}catch(const Error&){} }
    if(list.isEmpty()) list.append(QVariantMap{{"id","hose-for-hire"},{"name","Hose for Hire"},{"selected",true}}); return list;
}
QVariantMap Controller::view() const {
    const auto *g=selected(); QString installed,folder,previousVersion; bool running=false,previous=false; QString localError;QJsonObject receipt;qint64 installedBytes=0;int versionCount=0;
    try { Store store(m_gamesRoot); receipt=store.currentReceipt(m_selected); installed=receipt["version"].toString(); folder=store.currentDirectory(m_selected); running=store.running(m_selected);auto prev=store.state(m_selected)["previous"].toString();previous=!prev.isEmpty();if(previous)previousVersion=readJson(m_gamesRoot+"/"+m_selected+"/versions/"+prev+"/._teamhjd-receipt.json")["version"].toString();versionCount=QDir(m_gamesRoot+"/"+m_selected+"/versions").entryList(QDir::Dirs|QDir::NoDotAndDotDot).size();for(const auto &version:QDir(m_gamesRoot+"/"+m_selected+"/versions").entryList(QDir::Dirs|QDir::NoDotAndDotDot)){auto rp=m_gamesRoot+"/"+m_selected+"/versions/"+version+"/._teamhjd-receipt.json";if(QFileInfo::exists(rp)){auto r=readJson(rp);for(auto f:r["files"].toArray())installedBytes+=f.toObject()["bytes"].toInteger();}} } catch(const std::exception &e){localError=exceptionText(e);}
    const bool loginRequired=needsLogin();
    const bool online=m_online&&!loginRequired&&!m_offlineMode;
    const auto connection=connectionState(!m_config.server.isEmpty(),loginRequired,m_hadSession,m_checkingServer,online,m_lastRefresh>0);
    bool compatible=!g||g->supported();
    bool installedOk=!installed.isEmpty(); bool update=g&&compatible&&installedOk&&(g->version!=installed||g->sha256!=receipt["archiveSha256"].toString());
    QString action=running?QStringLiteral("게임 실행 중"):installedOk?update&&online?QStringLiteral("업데이트"):QStringLiteral("플레이"):needsLogin()?QStringLiteral("이메일로 시작"):m_config.server.isEmpty()?QStringLiteral("초대 파일로 연결"):!g?QStringLiteral("자동 연결 대기"):QStringLiteral("설치");
    QString status=m_busy&&!m_background?m_phase:running?QStringLiteral("게임을 실행하고 있어요"):online?update?QStringLiteral("새 버전이 있어요 · 업데이트를 눌러 적용하세요"):installedOk?QStringLiteral("플레이할 준비가 되었어요"):QStringLiteral("게임을 설치하고 테스트에 참여하세요"):installedOk?QStringLiteral("오프라인 · 설치된 버전으로 플레이할 수 있어요"):m_config.server.isEmpty()?QStringLiteral("팀에서 받은 초대 파일을 가져오세요"):QStringLiteral("서버 연결을 확인하세요");
    if(needsLogin()&&!installedOk&&!m_busy)status=QStringLiteral("이메일 인증 후 테스트에 참여할 수 있어요");
    if(!m_status.isEmpty()&&!m_busy&&!running) status=m_status;
    if(g&&!compatible&&!needsLogin()&&!m_busy){action=QStringLiteral("지원 빌드 준비 중");status=QStringLiteral("이 운영체제용 게임 빌드가 아직 배포되지 않았어요.");}
    return {{"needsPrivacy",m_cloudflare&&m_config.policyVersion!=privacy::version},{"failedRelease",m_knownFailedRelease},{"updateGate",m_updateGate},{"updateError",m_updateError},{"updateVersion",m_updateVersion},{"offlineMode",m_offlineMode},{"authenticating",m_authenticating},{"connectionState",connection.id},{"connectionLabel",connection.label},{"connectionDetail",connection.detail},{"hadEmailSession",m_hadSession},{"compatible",compatible},{"gamePlatform",g?g->platform:QString()},{"name",g?g->name:receipt["name"].toString(QStringLiteral("Hose for Hire"))},{"description",g?g->description:receipt["description"].toString(QStringLiteral("함께 출동하고, 함께 해결하세요.\nTeamHJD의 다음 플레이테스트가 여기에서 시작됩니다."))},{"notes",g?g->notes:receipt["notes"].toString(QStringLiteral("이메일로 인증하면 최신 테스트 빌드와 변경사항을 확인할 수 있습니다.\n\n인증은 브라우저에서 진행됩니다. 설치와 업데이트, 실행은 런처에서 한 번에 관리하세요."))},{"latest",g?g->version:QString()},{"installed",installed},{"installedAt",receipt["installedAt"].toString()},{"installedSize",sizeText(installedBytes)},{"versionCount",versionCount},{"previousVersion",previousVersion},{"canUninstall",versionCount>0||QFileInfo::exists(m_gamesRoot+"/"+m_selected+"/uninstall-pending.json")},{"canCancel",!m_deleting},{"folder",folder},{"online",online},{"running",running},{"previous",previous},{"busy",m_busy&&!m_background},{"working",m_busy},{"checking",m_busy&&m_background},{"progress",m_progress},{"transfer",m_transfer},{"action",action},{"status",status},{"error",!m_error.isEmpty()?m_error:localError},{"hasServer",!m_config.server.isEmpty()},{"canPlay",installedOk&&!running&&(!m_busy||m_background)},{"updateAvailable",update},{"updateMessage",m_updateMessage},{"launcherUpdate",!m_updateAsset.isEmpty()},{"cloudflare",m_cloudflare},{"needsLogin",needsLogin()},{"authExpires",m_authExpires.toString(Qt::ISODate)},{"serverHost",m_config.server.host()},{"size",g?sizeText(g->bytes):QString()}};
}
void Controller::selectGame(const QString &id){if(!m_updateGate.isEmpty())return;if(m_busy)return; if(id==m_selected)return; m_selected=id;m_status.clear();m_error.clear();emit changed();if(!m_config.server.isEmpty())refresh();}
bool Controller::deferDuringCheck(std::function<void()> action){if(m_busy&&m_background){m_cancel=true;QTimer::singleShot(125,this,std::move(action));return true;}return false;}
void Controller::work(std::function<void()> job,std::function<void(const QString&)> completed) {
    if(m_busy) return; m_busy=true;m_cancel=false;m_error.clear();m_status.clear();m_progress=0;m_transfer.clear();m_progressStart=QDateTime::currentMSecsSinceEpoch();emit changed();
    auto *watcher=new QFutureWatcher<WorkResult>(this);
    connect(watcher,&QFutureWatcher<WorkResult>::finished,this,[this,watcher,completed]{
        auto result=watcher->result();m_busy=false;m_background=false;m_phase.clear();watcher->deleteLater();
        if(result.code==FailureCode::PrivacyConsentRequired){
            m_authenticating=false;m_checkingServer=false;m_online=false;m_config.policyVersion.clear();m_policyAcceptedAt={};
            try{save();}catch(const Error&){}emit changed();return;
        }
        if(!m_launcherRoot.isEmpty()&&result.code==FailureCode::LauncherUpgradeRequired){
            m_authenticating=false;m_checkingServer=false;m_online=false;m_updateVerified=false;m_serverUpgradeRequired=true;
            checkLauncherUpdate(true);emit changed();return;
        }
        completed(result.message);emit changed();
    });
    m_future=QtConcurrent::run([job=std::move(job)]{try{job();return WorkResult{};}catch(const Error &e){return WorkResult{exceptionText(e),e.code};}catch(const std::exception &e){return WorkResult{exceptionText(e),FailureCode::General};}});watcher->setFuture(m_future);
}
void Controller::updateProgress(qint64 done,qint64 total,const QString &phase){QMetaObject::invokeMethod(this,[this,done,total,phase]{m_phase=phase;m_progress=total?double(done)/double(total):0;auto elapsed=qMax<qint64>(1,QDateTime::currentMSecsSinceEpoch()-m_progressStart);m_transfer=sizeText(done)+" / "+sizeText(total)+QStringLiteral("  ·  ")+sizeText(done*1000/elapsed)+"/s";emit changed();},Qt::QueuedConnection);}
void Controller::refresh() {
    if(m_cloudflare&&m_config.policyVersion!=privacy::version){emit changed();return;}
    if(!m_launcherRoot.isEmpty()&&!m_updateVerified&&m_updateGate.isEmpty()&&!m_offlineMode){checkLauncherUpdate(true);return;}
    if(m_busy||!m_updateGate.isEmpty()||m_offlineMode)return;if(needsLogin()){m_online=false;m_lastRefresh=QDateTime::currentSecsSinceEpoch();if(m_hadSession&&!m_authNoticeShown){m_authNoticeShown=true;emit toast(QStringLiteral("이메일 재인증이 필요해요"),QStringLiteral("이메일 인증이 만료되었거나 확인되지 않습니다. 다시 인증하면 서버에 연결됩니다."),"warning");}emit changed();checkLauncherUpdate();return;}if(m_config.server.isEmpty()){emit requestSettings();return;}
    m_lastRefresh=QDateTime::currentSecsSinceEpoch();m_background=true;m_checkingServer=true;
    auto cfg=m_config;auto key=m_key;auto minimum=m_minSequence;auto result=std::make_shared<Catalog>();auto raw=std::make_shared<QByteArray>();m_phase=QStringLiteral("테스트 서버에 연결하는 중");
    work([this,cfg,key,minimum,result,raw]{*raw=Http::get(cfg.server.resolved(QUrl("catalog.json")),cfg,m_cancel);*result=verifyCatalog(*raw,key,cfg.server,minimum);},[this,result,raw](const QString &error){
        m_checkingServer=false;
        if(error==QStringLiteral("취소했습니다.")){m_error.clear();return;}
        bool wasOnline=m_online;m_online=error.isEmpty();m_error=error;if(m_cloudflare&&error.contains(QStringLiteral("이메일 인증"))){m_config.cloudflareToken.clear();try{save();}catch(const Error&){} }
        if(needsLogin()&&m_hadSession&&!m_authNoticeShown){m_authNoticeShown=true;emit toast(QStringLiteral("이메일 재인증이 필요해요"),QStringLiteral("이메일 인증을 갱신하면 다시 연결됩니다."),"warning");}
        else if(wasOnline&&!m_online)emit toast(QStringLiteral("서버 연결이 끊겼어요"),QStringLiteral("자동으로 다시 연결할게요. 설치된 게임은 계속 플레이할 수 있어요."),"warning");
        if(!wasOnline&&m_online)emit toast(QStringLiteral("테스트 서버 연결됨"),QStringLiteral("최신 테스트 버전을 자동으로 확인하고 있어요."),"success");
        if(error.isEmpty()){m_catalog=*result;m_minSequence=result->sequence;if(!selected()&&!result->games.isEmpty())m_selected=result->games.first().id;try{atomicWrite(m_dataRoot+"/catalog-cache.json",*raw);save();}catch(const std::exception &e){m_error=exceptionText(e);}}
        try{if(m_online&&selected()){auto receipt=Store(m_gamesRoot).currentReceipt(m_selected);auto installed=receipt["version"].toString();if(!installed.isEmpty()&&(installed!=selected()->version||receipt["archiveSha256"].toString()!=selected()->sha256)&&m_updateNotices[m_selected].toString()!=selected()->sha256){m_updateNotices[m_selected]=selected()->sha256;emit toast(QStringLiteral("새 게임 버전이 있어요"),selected()->version+QStringLiteral(" · 업데이트 버튼을 눌러 적용하세요."),"info");}}}catch(const Error&){}
        QTimer::singleShot(600,this,[this]{checkLauncherUpdate();});
    });
}
void Controller::login(){
    if(m_cloudflare&&m_config.policyVersion!=privacy::version){emit changed();return;}
    if(deferDuringCheck([this]{login();}))return;
    if(m_busy||!m_cloudflare||!m_updateGate.isEmpty())return;
    if(!m_launcherRoot.isEmpty()&&!m_updateVerified){m_offlineMode=false;checkLauncherUpdate(true);return;}
    m_offlineMode=false;m_authenticating=true;
    auto helper=QCoreApplication::applicationDirPath()+"/"+CloudflareLogin::helperName;
#ifdef Q_OS_MACOS
    helper=QDir::cleanPath(QCoreApplication::applicationDirPath()+"/../Helpers/cloudflared");
#else
    if(!QFileInfo::exists(helper))helper=QCoreApplication::applicationDirPath()+"/../.deps/cloudflared/2026.10.5/cloudflared.exe";
#endif
    auto config=std::make_shared<NetConfig>(m_config);config->password.clear();config->caPem.clear();
    auto session=std::make_shared<CloudflareSession>();auto catalog=std::make_shared<Catalog>();auto raw=std::make_shared<QByteArray>();
    auto key=m_key;auto minimum=m_minSequence;m_phase=QStringLiteral("브라우저에서 이메일을 인증해 주세요");
    work([this,helper,config,session,catalog,raw,key,minimum]{
        *session=CloudflareLogin::authenticate(config->server,helper,m_dataRoot+"/auth-scratch",channel::issuer,channel::audience,m_cancel,[this](const QString &status){QMetaObject::invokeMethod(this,[this,status]{m_phase=status;emit changed();},Qt::QueuedConnection);});
        config->cloudflareToken=session->jwt;
        // The origin verifies the JWT signature. A signed catalog must also pass
        // publisher verification before a session is saved or the UI says ready.
        *raw=Http::get(config->server.resolved(QUrl("catalog.json")),*config,m_cancel);
        *catalog=verifyCatalog(*raw,key,config->server,minimum);
    },[this,config,session,catalog,raw](const QString &error){
        m_authenticating=false;m_error=error;
        if(!error.isEmpty()){emit toast(QStringLiteral("이메일 연결을 완료하지 못했어요"),error,error.contains(QStringLiteral("취소"))?"info":"error");return;}
        auto previous=m_config;auto previousExpires=m_authExpires;
        try{m_config=*config;m_authExpires=session->expires;save();atomicWrite(m_dataRoot+"/catalog-cache.json",*raw);m_catalog=*catalog;m_minSequence=catalog->sequence;m_online=true;m_hadSession=true;m_authNoticeShown=false;m_lastRefresh=QDateTime::currentSecsSinceEpoch();if(!selected()&&!catalog->games.isEmpty())m_selected=catalog->games.first().id;save();emit settingsChanged();emit toast(QStringLiteral("이메일 인증 완료"),QStringLiteral("게임을 설치하고 테스트에 참여하세요."),"success");}
        catch(const std::exception &e){m_config=previous;m_authExpires=previousExpires;m_online=false;m_error=exceptionText(e);emit toast(QStringLiteral("인증 정보를 저장하지 못했어요"),m_error,"error");}
    });
}
void Controller::primary(){if(!m_updateGate.isEmpty())return;
    if(deferDuringCheck([this]{primary();}))return;
    if(m_busy)return;
    try{
        Store store(m_gamesRoot);if(store.running(m_selected))return;
        auto receipt=store.currentReceipt(m_selected);auto *game=selected();
        if(game&&!game->supported()&&!needsLogin())fail(QStringLiteral("이 운영체제용 게임 빌드를 기다려 주세요."));
        if(receipt["version"].toString().isEmpty()){
            if(needsLogin()){emit requestLogin();return;}
            if(m_config.server.isEmpty()){emit requestInvite();return;}
            if(!game){refresh();return;}
            install(false);return;
        }
        if(game&&m_online&&(receipt["version"].toString()!=game->version||receipt["archiveSha256"].toString()!=game->sha256)){install(false);return;}
        playInstalled();
    }catch(const std::exception &error){m_error=exceptionText(error);emit changed();}
}
void Controller::playInstalled(){if(!m_updateGate.isEmpty())return;
    if(deferDuringCheck([this]{playInstalled();}))return;
    if(m_busy)return;
    try{
        Store store(m_gamesRoot);if(store.running(m_selected))return;
        if(store.currentReceipt(m_selected).isEmpty())fail(QStringLiteral("설치된 게임이 없습니다."));
        auto root=m_gamesRoot,id=m_selected;m_phase=QStringLiteral("게임 파일을 확인하는 중");
        work([root,id]{Store(root).launch(id);},[this](const QString &error){m_error=error;if(!error.isEmpty())emit toast(QStringLiteral("게임 실행을 확인해 주세요"),error,"error");});
    }catch(const std::exception &error){m_error=exceptionText(error);emit changed();}
}
void Controller::install(bool force){if(deferDuringCheck([this,force]{install(force);}))return;if(m_busy)return;if(!selected())return;if(!m_online){m_error=QStringLiteral("설치하려면 테스트 서버에 다시 연결하세요.");emit changed();return;}auto g=*selected();auto cfg=m_config;auto key=m_key;auto root=m_gamesRoot;auto minimum=m_minSequence;auto part=m_dataRoot+"/downloads/"+g.sha256+".part";m_phase=QStringLiteral("다운로드를 준비하는 중");
    work([this,g,cfg,key,root,minimum,part,force]{
        // Re-authenticate the catalog immediately before any installation, including repair.
        auto catalog=verifyCatalog(Http::get(cfg.server.resolved(QUrl("catalog.json")),cfg,m_cancel),key,cfg.server,minimum);bool found=false;
        for(const auto &latest:catalog.games)if(latest.id==g.id&&latest.sha256==g.sha256&&latest.version==g.version)found=true;
        if(!found)fail(QStringLiteral("배포 목록이 바뀌었습니다. 새로고침한 뒤 다시 설치하세요."));
        if(Store(root).running(g.id))fail(QStringLiteral("게임을 종료한 뒤 업데이트하세요."));
        Http::download(g.packageUrl,cfg,part,g.bytes,g.sha256,m_cancel,[this](qint64 a,qint64 b){updateProgress(a,b,QStringLiteral("게임을 다운로드하는 중"));});
        Store(root).install(g,part,m_cancel,[this](qint64 a,qint64 b){updateProgress(a,b,QStringLiteral("설치 파일을 확인하는 중"));},force);
    },[this](const QString &e){m_error=e;if(e.isEmpty()){m_status=QStringLiteral("설치 완료 · 플레이할 준비가 되었어요");emit toast(QStringLiteral("플레이할 준비가 되었어요"),QStringLiteral("게임 설치와 파일 확인을 마쳤어요."),"success");}else emit toast(e.contains(QStringLiteral("취소"))?QStringLiteral("작업을 취소했어요"):QStringLiteral("설치를 완료하지 못했어요"),e,e.contains(QStringLiteral("취소"))?"info":"error");});
}
void Controller::repair(){if(!m_updateGate.isEmpty())return;if(deferDuringCheck([this]{repair();}))return;if(m_busy)return;m_updateHolds.remove(m_selected);install(true);}
void Controller::uninstall(){if(!m_updateGate.isEmpty())return;if(deferDuringCheck([this]{uninstall();}))return;if(m_busy)return;auto root=m_gamesRoot,id=m_selected,downloads=m_dataRoot+"/downloads";QString latestHash=selected()?selected()->sha256:QString();m_deleting=true;m_phase=QStringLiteral("게임 파일을 삭제하는 중");work([root,id,downloads,latestHash]{QSet<QString> hashes;if(!latestHash.isEmpty())hashes.insert(latestHash);auto base=root+"/"+id+"/versions";for(const auto &version:QDir(base).entryList(QDir::Dirs|QDir::NoDotAndDotDot)){auto p=base+"/"+version+"/._teamhjd-receipt.json";if(QFileInfo::exists(p))hashes.insert(readJson(p)["archiveSha256"].toString());}Store(root).uninstall(id);for(const auto &hash:hashes){static QRegularExpression valid("^[a-f0-9]{64}$");if(valid.match(hash).hasMatch()){auto cache=downloads+"/"+hash+".part";noLinks(cache);QFile::remove(cache);}}},[this,id](const QString &e){m_deleting=false;m_error=e;m_updateHolds.remove(id);if(e.isEmpty()){m_status=QStringLiteral("게임을 삭제했어요. 저장 데이터와 테스트 로그는 유지했어요.");emit toast(QStringLiteral("게임 삭제 완료"),QStringLiteral("설치 파일과 보관 버전을 정리했어요. 필요할 때 다시 설치할 수 있어요."),"success");}else emit toast(QStringLiteral("삭제를 마치지 못했어요"),e,"error");});}
void Controller::rollback(){if(!m_updateGate.isEmpty())return;if(deferDuringCheck([this]{rollback();}))return;if(m_busy)return;auto root=m_gamesRoot,id=m_selected;m_phase=QStringLiteral("이전 버전을 확인하는 중");work([root,id]{Store(root).rollback(id);},[this](const QString &e){m_error=e;if(e.isEmpty()){m_status=QStringLiteral("이전 버전으로 복원했어요");if(selected())m_updateHolds[m_selected]=selected()->sha256;try{save();}catch(const Error&){} }emit toast(e.isEmpty()?QStringLiteral("이전 버전으로 복원했어요"):QStringLiteral("복원을 마치지 못했어요"),e,e.isEmpty()?"success":"error");});}
void Controller::cancel(){m_cancel=true;m_phase=QStringLiteral("안전하게 취소하는 중");emit changed();}
void Controller::openFolder(const QString &which){if(!m_updateGate.isEmpty())return;QString p;try{if(which=="game")p=Store(m_gamesRoot).currentDirectory(m_selected);else if(which=="logs"){
#ifdef Q_OS_MACOS
    p=QDir::homePath()+"/Library/Application Support/DefaultCompany/Hose for Hire/PlaytestLogs";
#else
    p=QDir::homePath()+"/AppData/LocalLow/DefaultCompany/Hose for Hire/PlaytestLogs";
#endif
    }else if(which=="downloads")p=m_dataRoot+"/downloads";else p=m_gamesRoot;if(p.isEmpty()||!QFileInfo::exists(p)){m_error=QStringLiteral("아직 폴더가 없습니다.");emit changed();return;}QDesktopServices::openUrl(QUrl::fromLocalFile(p));}catch(const std::exception &e){m_error=exceptionText(e);emit changed();}}
bool Controller::saveSettings(const QString &url,const QString &password,const QString &key,const QString &root,bool reduced){
    if(!m_updateGate.isEmpty())return false;
    if(m_busy)return false;
    try{QUrl server(url.trimmed());if(!server.path().endsWith('/'))server.setPath(server.path()+'/');if(!allowedServer(server,m_config.testLoopback))fail(QStringLiteral("HTTPS 서버 주소를 입력하세요."));
        auto trimmed=key.trimmed();auto decoded=QByteArray::fromBase64Encoding(trimmed.mid(5).toLatin1(),QByteArray::AbortOnBase64DecodingErrors);if(!trimmed.startsWith("p256:")||!decoded||decoded.decoded.size()!=64)fail(QStringLiteral("초대 정보의 배포자 키를 입력하세요."));
        auto destination=QFileInfo(root.trimmed()).absoluteFilePath();if(root.trimmed().isEmpty()||!QDir::isAbsolutePath(root.trimmed()))fail(QStringLiteral("게임 설치 폴더의 전체 경로를 입력하세요."));noLinks(destination);
        bool identityChanged=server!=m_config.server||trimmed!=m_key;
        bool cloudflare=server==QUrl(channel::server)&&trimmed==channel::publisherKey;
        if(!cloudflare&&password.isEmpty()&&(m_config.password.isEmpty()||identityChanged))fail(QStringLiteral("테스트 서버 접근 암호를 입력하세요."));if(password.toUtf8().size()>1024)fail(QStringLiteral("접근 암호가 너무 깁니다."));
        auto previousConfig=m_config;auto previousKey=m_key,previousRoot=m_gamesRoot;auto previousSequence=m_minSequence;bool previousReduced=m_reduceMotion;bool previousCloudflare=m_cloudflare;
        m_cloudflare=cloudflare;if(identityChanged){m_config.cloudflareToken.clear();m_authExpires={};m_config.policyVersion.clear();}m_config.server=server;if(!password.isEmpty())m_config.password=password.toUtf8();if(m_hasPendingCa)m_config.caPem=m_pendingCa;else if(identityChanged)m_config.caPem.clear();m_key=trimmed;m_gamesRoot=destination;m_reduceMotion=reduced;if(identityChanged)m_minSequence=0;
        try{save();}catch(...){m_config=previousConfig;m_key=previousKey;m_gamesRoot=previousRoot;m_minSequence=previousSequence;m_reduceMotion=previousReduced;m_cloudflare=previousCloudflare;throw;}
        m_hasPendingCa=false;m_pendingCa.clear();if(identityChanged){m_catalog={};m_online=false;m_updateHolds={};}m_error.clear();emit settingsChanged();emit changed();refresh();return true;
    }catch(const std::exception &e){m_error=exceptionText(e);emit changed();return false;}
}
QVariantMap Controller::importInvite(const QUrl &file){
    if(!m_updateGate.isEmpty())return {};
    if(deferDuringCheck([this,file]{importInvite(file);}))return {{"pending",true}};
    if(m_busy){m_error=QStringLiteral("진행 중인 작업을 마친 뒤 초대 파일을 열어 주세요.");emit changed();return {};}
    try{
        if(!file.isLocalFile())fail(QStringLiteral("팀에서 받은 로컬 초대 파일을 선택하세요."));
        auto invitation=Invite::parse(readFile(file.toLocalFile(),256*1024),m_config.testLoopback);
        m_pendingCa=invitation.certificate;m_hasPendingCa=true;
        if(invitation.accessToken.isEmpty())return {{"server",invitation.server.toString()},{"key",invitation.publisherKey}};
        auto previous=m_inviteInfo;m_inviteInfo={{"label",invitation.label},{"expiresAt",invitation.expires.toString(Qt::ISODate)}};
        if(!saveSettings(invitation.server.toString(),QString::fromLatin1(invitation.accessToken),invitation.publisherKey,m_gamesRoot,m_reduceMotion)){m_inviteInfo=previous;return {};}
        emit inviteAccepted();emit toast(QStringLiteral("초대 정보를 적용했어요"),QStringLiteral("테스트 서버와 최신 게임을 확인하고 있어요."),"info");return {{"applied",true}};
    }catch(const std::exception &e){m_error=exceptionText(e);emit toast(QStringLiteral("초대 파일을 확인해 주세요"),m_error,"error");emit changed();return {};}
}
void Controller::setTestUpdateFrom(const QString &value){
    qsizetype suffix=0;auto parsed=QVersionNumber::fromString(value,&suffix);
    if(!m_config.testLoopback||m_busy||!safeVersion(value)||parsed.isNull()||suffix!=value.size())
        fail(QStringLiteral("업데이트 비교 검증은 격리된 loopback 테스트에서만 사용할 수 있습니다."));
    m_testUpdateFrom=value;
}
void Controller::checkLauncherUpdate(bool force){
    if(m_busy||m_launcherRoot.isEmpty()||m_updateGate=="ready"||m_updateGate=="restarting"||m_updateGate=="downloading")return;
    if(!force&&QDateTime::currentSecsSinceEpoch()-m_lastLauncherCheck<900)return;
    m_updateRetry.stop();m_lastLauncherCheck=QDateTime::currentSecsSinceEpoch();m_updateVerified=false;
    m_updateGate="checking";m_updateError.clear();m_background=true;m_phase=QStringLiteral("런처 업데이트를 확인하는 중");
    auto raw=std::make_shared<QByteArray>();auto release=std::make_shared<updates::Release>();
    struct Inspection {bool needsBootstrap=false;QString candidate;};
    auto inspection=std::make_shared<Inspection>();const bool inspectBootstrap=m_testUpdateFrom.isEmpty();
    auto minimum=m_updateSequence;
    work([this,raw,release,minimum,inspection,inspectBootstrap]{
        const auto url=QUrl(QString::fromLatin1(updates::feedUrl)+"?check="+QString::number(QDateTime::currentMSecsSinceEpoch()));
        try{*raw=m_updateFetch?m_updateFetch(url):Http::get(url,{},m_cancel,256*1024,true);}
        catch(const Error&){if(m_cancel)throw;*raw=m_updateFetch?m_updateFetch(QUrl(updates::fallbackFeedUrl)):Http::get(QUrl(updates::fallbackFeedUrl),{},m_cancel,256*1024,true);}
        *release=updates::verifyRelease(*raw,m_releaseKey,minimum);
        if(inspectBootstrap){
            Store store(m_launcherRoot);auto directory=store.currentDirectory("launcher");
            if(!directory.isEmpty()&&QFileInfo::exists(directory+"/bootstrap-manifest.json")){
                inspection->needsBootstrap=!bootstrapCurrent(m_launcherRoot,directory);
                const auto receipt=store.currentReceipt("launcher");
                if(inspection->needsBootstrap&&receipt["version"].toString()==release->version&&receipt["archiveSha256"].toString()==release->payload["sha256"].toString()&&verifyTree(directory,receipt))inspection->candidate=store.state("launcher")["current"].toString();
            }
        }
    },[this,raw,release,inspection](const QString &error){
        if(!error.isEmpty()){updateFailed(error);return;}
        try{
            atomicWrite(m_dataRoot+"/launcher-feed.json",*raw);
            writeJson(m_dataRoot+"/launcher-feed-state.json",{{"sequence",release->sequence},{"version",release->version}});
            m_updateSequence=release->sequence;m_releaseEnvelope=*raw;
            m_knownFailedRelease=false;
            const auto failurePath=m_launcherRoot+"/launcher/failed-release.json";
            if(QFileInfo::exists(failurePath)&&readJson(failurePath)["version"].toString()==release->version){
                if(!m_retryFailedOnce){
                    m_updateVerified=false;m_online=false;m_knownFailedRelease=true;m_updateGate="error";
                    m_updateError=QStringLiteral("새 런처의 정상 실행을 확인하지 못해 이전 버전으로 복원했습니다. 새 패치를 자동으로 확인합니다. 복구를 마쳤다면 다시 설치를 시도할 수 있습니다.");
                    m_updateRetry.start(300000);return;
                }
            }
            m_retryFailedOnce=false;
            const auto comparison=m_testUpdateFrom.isEmpty()?version():m_testUpdateFrom;
            const bool needsBootstrap=inspection->needsBootstrap;
            if(!inspection->candidate.isEmpty()){
                        const auto candidate=inspection->candidate;
                        atomicWrite(m_launcherRoot+"/launcher/update-evidence/"+candidate+".json",*raw);
                        writeJson(m_launcherRoot+"/queued-launcher.json",{{"candidate",candidate}});
                        m_updateVersion=release->version;m_updateGate="ready";m_updateMessage=QStringLiteral("실행기를 갱신하고 다시 시작합니다");
                        QTimer::singleShot(1200,this,&Controller::restartForUpdate);return;
            }
            if(needsBootstrap&&QVersionNumber::fromString(release->version)<QVersionNumber::fromString(version()))fail(QStringLiteral("이 실행기 버전에 맞는 공식 설치 파일로 복구해 주세요."));
            if(!needsBootstrap&&QVersionNumber::fromString(release->version)<=QVersionNumber::fromString(comparison)){
                if(m_serverUpgradeRequired)fail(QStringLiteral("서버가 요구하는 최신 버전 정보를 확인하지 못했습니다. 배포 정보가 갱신되면 자동으로 다시 시도합니다."));
                m_updateAttempts=0;m_updateVerified=true;m_updateGate.clear();m_updateMessage=QStringLiteral("최신 런처");
                QTimer::singleShot(0,this,[this]{if(!m_offlineMode&&!m_config.server.isEmpty())refresh();});return;
            }
            m_updateInstaller=false;
#ifdef Q_OS_WIN
            const auto marker=m_launcherRoot+"/teamhjd-install.json";
            if(QFileInfo::exists(marker)){
                if(readJson(marker)["product"].toString()!="com.teamhjd.game-test-launcher"||QFileInfo(m_launcherRoot).fileName()!="TeamHJD Game Test Launcher")fail(QStringLiteral("설치 정보를 확인하지 못했습니다. 공식 설치 프로그램으로 복구해 주세요."));
                m_updateInstaller=true;
            }
#endif
            m_updateAsset=m_updateInstaller?release->installer:release->payload;m_updateVersion=release->version;
            m_updateGate="downloading";m_updateMessage=QStringLiteral("런처 %1 업데이트").arg(release->version);
            QTimer::singleShot(0,this,&Controller::applyLauncherUpdate);
        }catch(const std::exception &failure){updateFailed(exceptionText(failure));}
    });
}
void Controller::updateFailed(const QString &error){
    m_updateVerified=false;m_online=false;m_updateGate="error";m_updateError=error;
    m_updateMessage=QStringLiteral("런처 업데이트 확인 필요");
    const int attempt=qMin(m_updateAttempts,5);m_updateAttempts=qMin(5,m_updateAttempts+1);
    const int seconds=qMin(300,15*(1<<attempt))+int(QRandomGenerator::global()->bounded(6));
    m_updateRetry.start(seconds*1000);
    emit toast(QStringLiteral("업데이트를 다시 시도할게요"),error+QStringLiteral("\n인터넷 연결을 확인해 주세요."),"warning");
}
void Controller::retryFailedUpdate(){
    if(m_busy)return;m_retryFailedOnce=true;m_offlineMode=false;checkLauncherUpdate(true);
}
void Controller::applyLauncherUpdate(){
    if(m_busy||m_updateAsset.isEmpty()||m_launcherRoot.isEmpty())return;
    auto asset=m_updateAsset;auto ver=m_updateVersion;auto root=m_launcherRoot;
    auto archive=m_dataRoot+"/downloads/launcher-"+asset["sha256"].toString()+".part";
    const auto envelope=m_releaseEnvelope;
    if(m_updateInstaller){
        const auto verified=m_dataRoot+"/downloads/launcher-installer-"+asset["sha256"].toString()+".exe";
        m_phase=QStringLiteral("설치 프로그램을 다운로드하고 검증하는 중");m_updateGate="downloading";m_updateError.clear();
        work([this,asset,archive,verified]{
            noLinks(verified);
            if(QFileInfo::exists(verified)&&QFileInfo(verified).size()==asset["bytes"].toInteger()&&fileHash(verified,&m_cancel)==asset["sha256"].toString())return;
            if(m_updateDownload)m_updateDownload(asset,archive);
            else Http::download(QUrl(asset["url"].toString()),{},archive,asset["bytes"].toInteger(),asset["sha256"].toString(),m_cancel,[this](qint64 done,qint64 total){updateProgress(done,total,QStringLiteral("설치 프로그램을 다운로드하고 검증하는 중"));},true);
            if(QFileInfo(archive).size()!=asset["bytes"].toInteger()||fileHash(archive,&m_cancel)!=asset["sha256"].toString())fail(QStringLiteral("설치 프로그램의 크기·해시 검증에 실패했습니다."));
            if(QFileInfo::exists(verified)&&!QFile::remove(verified))fail(QStringLiteral("이전 설치 캐시를 정리하지 못했습니다."));
            if(!QFile::rename(archive,verified))fail(QStringLiteral("검증된 설치 프로그램을 준비하지 못했습니다."));
        },[this,verified](const QString &error){
            if(!error.isEmpty()){updateFailed(error);return;}
            m_readyInstaller=verified;m_updateGate="ready";m_updateMessage=QStringLiteral("설치 프로그램으로 업데이트를 적용합니다");
            QTimer::singleShot(1200,this,&Controller::restartForUpdate);
        });return;
    }
    auto folder=std::make_shared<QString>();m_phase=QStringLiteral("런처 업데이트를 준비하는 중");m_updateGate="downloading";m_updateError.clear();
    work([this,asset,ver,root,archive,folder,envelope]{
        auto hash=asset["sha256"].toString();
        if(m_updateDownload)m_updateDownload(asset,archive);
        else Http::download(QUrl(asset["url"].toString()),{},archive,asset["bytes"].toInteger(),hash,m_cancel,
            [this](qint64 done,qint64 total){updateProgress(done,total,QStringLiteral("런처를 다운로드하고 검증하는 중"));},true);
        if(QFileInfo(archive).size()!=asset["bytes"].toInteger())fail(QStringLiteral("런처 다운로드 크기 검증에 실패했습니다."));
        *folder=Store(root).stagePayload(ver,hash,archive,m_cancel);
        QDir().mkpath(root+"/launcher/update-evidence");
        atomicWrite(root+"/launcher/update-evidence/"+*folder+".json",envelope);
        writeJson(root+"/queued-launcher.json",{{"candidate",*folder}});
    },[this](const QString &error){
        if(!error.isEmpty()){updateFailed(error);return;}
        m_updateAsset={};m_updateGate="ready";m_updateMessage=QStringLiteral("업데이트 적용 후 다시 시작합니다");
        QTimer::singleShot(1200,this,&Controller::restartForUpdate);
    });
}
void Controller::restartForUpdate(){
    if(m_busy||m_updateGate!="ready"||m_launcherRoot.isEmpty())return;
    try{
        if(m_updateInstaller){
#ifdef Q_OS_WIN
            const auto release=updates::verifyRelease(m_releaseEnvelope,m_releaseKey,m_updateSequence);
            noLinks(m_readyInstaller);
            if(m_readyInstaller.isEmpty()||QFileInfo(m_readyInstaller).size()!=release.installer["bytes"].toInteger()||fileHash(m_readyInstaller)!=release.installer["sha256"].toString())fail(QStringLiteral("설치 프로그램을 다시 검증하지 못했습니다."));
            const auto stamp=processStamp(QCoreApplication::applicationPid(),QCoreApplication::applicationFilePath());
            if(!stamp||m_dataRoot.contains('"')||m_launcherRoot.contains('"')||m_dataRoot.contains('\n')||m_launcherRoot.contains('\n'))fail(QStringLiteral("업데이트 요청 경로를 확인하지 못했습니다."));
            QProcess setup;setup.setProgram(m_readyInstaller);setup.setWorkingDirectory(QFileInfo(m_readyInstaller).absolutePath());
            // NSIS /D consumes the rest of the command line and must be last,
            // without surrounding quotes. No shell interprets this string.
            setup.setNativeArguments(QStringLiteral("/UPDATE /WAITPID=%1 /WAITSTAMP=%2 /PROFILE=\"%3\" /D=%4").arg(QString::number(QCoreApplication::applicationPid()),QString::number(stamp),QDir::toNativeSeparators(m_dataRoot),QDir::toNativeSeparators(m_launcherRoot)));
            if(!setup.startDetached())fail(QStringLiteral("업데이트 설치 프로그램을 시작하지 못했습니다."));
            m_updateRetry.stop();m_updateGate="restarting";emit changed();emit quitForUpdate();return;
#endif
        }
        const auto queued=readJson(m_launcherRoot+"/queued-launcher.json");
        const auto candidate=queued["candidate"].toString();
        static const QRegularExpression folder("^[0-9][A-Za-z0-9._-]{0,160}$");
        if(!folder.match(candidate).hasMatch()||candidate.endsWith('.'))fail(QStringLiteral("업데이트 적용 경로가 올바르지 않습니다."));
        const auto directory=m_launcherRoot+"/launcher/versions/"+candidate;
        const auto receipt=readJson(directory+"/._teamhjd-receipt.json");
        if(!verifyTree(directory,receipt))fail(QStringLiteral("재시작 전 런처 파일 검증에 실패했습니다."));
        const auto release=updates::verifyRelease(readFile(m_launcherRoot+"/launcher/update-evidence/"+candidate+".json",256*1024),m_releaseKey,m_updateSequence);
        if(release.version!=receipt["version"].toString()||release.payload["sha256"].toString()!=receipt["archiveSha256"].toString())fail(QStringLiteral("재시작할 런처와 서명된 배포 정보가 다릅니다."));
        const auto executable=bootstrapExecutable(directory);
        noLinks(executable);
        if(!QFileInfo::exists(executable))fail(QStringLiteral("업데이트 실행기가 없습니다. 공식 설치 파일로 복구해 주세요."));
        const auto stamp=processStamp(QCoreApplication::applicationPid(),QCoreApplication::applicationFilePath());
        if(!stamp)fail(QStringLiteral("재시작할 런처 프로세스를 확인하지 못했습니다."));
        QStringList arguments{"--runtime-root",m_launcherRoot,"--activate",candidate,"--wait-pid",QString::number(QCoreApplication::applicationPid()),"--wait-stamp",QString::number(stamp),"--data-root",m_dataRoot};
        if(m_config.testLoopback)arguments<<"--noninteractive-test";
        if(!QProcess::startDetached(executable,arguments,QFileInfo(executable).absolutePath()))fail(QStringLiteral("업데이트 실행기를 시작하지 못했습니다."));
        m_updateRetry.stop();m_updateGate="restarting";emit changed();emit quitForUpdate();
    }catch(const std::exception &error){updateFailed(exceptionText(error));emit changed();}
}

void Controller::continueOffline(){
    if(m_busy||(!needsLogin()&&m_updateGate!="error"&&(!m_cloudflare||m_config.policyVersion==privacy::version)))return;
    try{
        if(Store(m_gamesRoot).currentReceipt(m_selected).isEmpty())return;
        m_updateRetry.stop();m_offlineMode=true;m_online=false;m_updateGate.clear();m_updateError.clear();m_error.clear();emit changed();
    }catch(const std::exception &error){m_error=exceptionText(error);emit changed();}
}
QString Controller::privacyNotice() const{return privacy::notice();}
void Controller::acceptPrivacy(bool collection,bool overseas,bool ageConfirmed){
    if(m_busy||!m_updateGate.isEmpty()||!m_cloudflare||!collection||!overseas||!ageConfirmed)return;
    const auto previous=m_config.policyVersion;const auto previousAt=m_policyAcceptedAt;
    m_config.policyVersion=privacy::version;m_policyAcceptedAt=QDateTime::currentDateTimeUtc();m_offlineMode=false;
    try{save();m_error.clear();emit changed();if(!needsLogin())refresh();}
    catch(const Error &error){m_config.policyVersion=previous;m_policyAcceptedAt=previousAt;m_error=exceptionText(error);emit changed();}
}
void Controller::forgetLogin(){
    if(m_busy||!m_updateGate.isEmpty())return;
    const auto previous=m_config;const auto accepted=m_policyAcceptedAt;
    m_config.cloudflareToken.clear();m_config.password.clear();m_config.policyVersion.clear();m_policyAcceptedAt={};
    try{save();m_authExpires={};m_online=false;m_offlineMode=false;m_hadSession=false;m_error.clear();emit changed();}
    catch(const Error &error){m_config=previous;m_policyAcceptedAt=accepted;m_error=exceptionText(error);emit changed();}
}
void Controller::showLicenses(){
    auto path=QCoreApplication::applicationDirPath()+"/THIRD_PARTY_NOTICES.md";
#ifdef Q_OS_MACOS
    path=QDir::cleanPath(QCoreApplication::applicationDirPath()+"/../../../../THIRD_PARTY_NOTICES.md");
#endif
    if(QFileInfo::exists(path))QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    else QDesktopServices::openUrl(QUrl("https://github.com/Team-HJD-21/team-hjd-game-test-launcher/blob/main/THIRD_PARTY_NOTICES.md"));
}
