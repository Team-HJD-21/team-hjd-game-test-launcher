#include "Core.h"
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QTimer>

namespace hjd {
namespace {
bool githubHost(const QUrl &u) {
    return u.scheme()=="https" && u.userInfo().isEmpty() && u.fragment().isEmpty() && (u.port(-1)==-1||u.port()==443)
        && QStringList{"api.github.com","github.com","release-assets.githubusercontent.com","objects.githubusercontent.com"}.contains(u.host());
}
struct Response { QByteArray body; int status=0; QUrl redirect; };
Response request(const QUrl &url,const NetConfig &cfg,std::atomic_bool &cancel,qint64 maximum,bool publicFile,QFile *file=nullptr,qint64 expected=0,qint64 offset=0,const Progress &progress={}) {
    if(cancel) fail(QStringLiteral("취소했습니다."));
    if(publicFile?!githubHost(url):(!allowedServer(url,cfg.testLoopback)||!sameOrigin(url,cfg.server))) fail(QStringLiteral("허용되지 않은 다운로드 주소입니다."));
    QNetworkAccessManager manager;
    QNetworkRequest req(url); req.setTransferTimeout(30000);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    req.setRawHeader("User-Agent","TeamHJD-GameTestLauncher/" HJD_VERSION);
    req.setRawHeader("Accept-Encoding","identity");
    if(!publicFile) {
        if(!cfg.cloudflareToken.isEmpty()) req.setRawHeader("Cookie","CF_Authorization="+cfg.cloudflareToken);
        else if(!cfg.password.isEmpty()) req.setRawHeader("Authorization","Bearer "+cfg.password.toBase64());
        if(!cfg.caPem.isEmpty()) {
            auto certs=QSslCertificate::fromData(cfg.caPem); if(certs.isEmpty()) fail(QStringLiteral("초대 인증서를 읽을 수 없습니다."));
            auto ssl=QSslConfiguration::defaultConfiguration(); auto roots=ssl.caCertificates(); roots+=certs; ssl.setCaCertificates(roots); req.setSslConfiguration(ssl);
        }
    } else if(url.host()=="api.github.com") { req.setRawHeader("Accept","application/vnd.github+json"); req.setRawHeader("X-GitHub-Api-Version","2022-11-28"); }
    if(file && offset>0) req.setRawHeader("Range","bytes="+QByteArray::number(offset)+"-");
    auto *reply=manager.get(req); reply->setReadBufferSize(1024*1024);
    QEventLoop loop; QString error; Response result; qint64 received=offset; bool headersChecked=false;
    QTimer poll; poll.setInterval(75);
    QObject::connect(&poll,&QTimer::timeout,&loop,[&]{if(cancel){error=QStringLiteral("취소했습니다.");reply->abort();}}); poll.start();
    QTimer deadline; deadline.setSingleShot(true);
    if(!file) { QObject::connect(&deadline,&QTimer::timeout,&loop,[&]{error=QStringLiteral("서버 응답 시간이 초과되었습니다.");reply->abort();}); deadline.start(60000); }
    auto drain=[&] {
        int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if(status>=300 && status<400) { reply->readAll(); return; }
        if(status!=200 && status!=206) { reply->readAll(); return; }
        if(!headersChecked) {
            headersChecked=true;
            auto encoding=reply->rawHeader("Content-Encoding");
            if(!encoding.isEmpty()&&encoding!="identity") { error=QStringLiteral("예상하지 못한 전송 형식입니다."); reply->abort(); return; }
            if(file) {
                if(status==206) {
                    const auto wanted="bytes "+QByteArray::number(offset)+"-"+QByteArray::number(expected-1)+"/"+QByteArray::number(expected);
                    if(offset==0 || reply->rawHeader("Content-Range")!=wanted) { error=QStringLiteral("이어받기 응답이 올바르지 않습니다."); reply->abort(); return; }
                } else if(offset>0) { if(!file->resize(0)||!file->seek(0)) {error=QStringLiteral("다운로드 파일을 다시 준비할 수 없습니다.");reply->abort();return;} received=0; }
                const auto declared=reply->header(QNetworkRequest::ContentLengthHeader);
                if(!declared.isValid() || declared.toLongLong()!=expected-received) {error=QStringLiteral("다운로드 용량이 배포 정보와 다릅니다.");reply->abort();return;}
            }
        }
        auto chunk=reply->readAll(); if(chunk.size()>maximum-received) { error=QStringLiteral("다운로드가 허용 용량을 초과했습니다.");reply->abort();return; }
        if(file) { if(file->write(chunk)!=chunk.size()) {error=QStringLiteral("다운로드를 저장하지 못했습니다.");reply->abort();return;} }
        else result.body+=chunk;
        received+=chunk.size(); if(progress) progress(received,expected);
    };
    QObject::connect(reply,&QNetworkReply::readyRead,&loop,drain);
    QObject::connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit); loop.exec();
    if(reply->bytesAvailable()) drain();
    result.status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if(!error.isEmpty()) fail(error);
    if(result.status>=300&&result.status<400) { if(!publicFile&&!cfg.cloudflareToken.isEmpty())fail(QStringLiteral("이메일 인증이 만료되었어요. 이메일로 다시 연결해 주세요.")); result.redirect=url.resolved(reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl()); return result; }
    if(result.status==401||result.status==403) fail(publicFile?QStringLiteral("런처 업데이트 조회가 제한되었습니다. 잠시 후 다시 시도하세요."):!cfg.cloudflareToken.isEmpty()?QStringLiteral("이메일 인증이 만료되었어요. 이메일로 다시 연결해 주세요."):QStringLiteral("초대 또는 접근 정보를 확인하세요. 만료되었거나 회수된 초대는 새로 받아 주세요."));
    if(result.status==404) fail(QStringLiteral("배포된 파일을 찾을 수 없습니다."));
    if(reply->error()!=QNetworkReply::NoError) fail(QStringLiteral("서버에 연결할 수 없습니다. 주소와 인증서, 인터넷 연결을 확인하세요."));
    if(result.status!=200&&result.status!=206) fail(QStringLiteral("서버가 요청을 처리하지 못했습니다. 잠시 후 다시 시도하세요."));
    if(file&&(received!=expected||!file->flush())) fail(QStringLiteral("다운로드가 완료되지 않았습니다. 다시 시도하면 이어받습니다."));
    return result;
}
}
QByteArray Http::get(const QUrl &url,const NetConfig &config,std::atomic_bool &cancel,qint64 maximum,bool publicFile) {
    auto u=url;
    for(int i=0;i<6;++i) { auto r=request(u,config,cancel,maximum,publicFile); if(r.redirect.isEmpty()) return r.body; if(!publicFile||!githubHost(r.redirect)) fail(QStringLiteral("서버의 주소 이동 요청을 허용하지 않습니다.")); u=r.redirect; }
    fail(QStringLiteral("서버의 주소 이동이 너무 많습니다."));
}
void Http::download(const QUrl &url,const NetConfig &config,const QString &path,qint64 bytes,const QString &hash,std::atomic_bool &cancel,const Progress &progress,bool publicFile) {
    if(bytes<=0||bytes>10ll*1024*1024*1024) fail(QStringLiteral("다운로드 용량이 올바르지 않습니다."));
    noLinks(path); if(!QDir().mkpath(QFileInfo(path).absolutePath())) fail(QStringLiteral("다운로드 폴더를 만들지 못했습니다."));
    QFile f(path); if(!f.open(QIODevice::ReadWrite)) fail(QStringLiteral("다운로드 파일을 열지 못했습니다."));
    if(f.size()>bytes) { if(!f.resize(0)) fail(QStringLiteral("다운로드 파일을 초기화할 수 없습니다.")); }
    if(f.size()!=bytes) {
        auto offset=f.size(); if(!f.seek(offset)) fail(QStringLiteral("이어받기를 준비할 수 없습니다.")); auto u=url; bool completed=false;
        for(int i=0;i<6;++i) {
            auto r=request(u,config,cancel,bytes,publicFile,&f,bytes,offset,progress);
            if(r.redirect.isEmpty()) { completed=true; break; }
            if(!publicFile||!githubHost(r.redirect)) fail(QStringLiteral("다운로드 주소 이동을 허용하지 않습니다.")); u=r.redirect;
        }
        if(!completed) fail(QStringLiteral("다운로드 주소 이동이 너무 많습니다."));
    }
    f.close(); if(fileHash(path,&cancel)!=hash) { QFile::remove(path); fail(QStringLiteral("다운로드 파일의 검증에 실패했습니다. 다시 다운로드하세요.")); }
    if(progress) progress(bytes,bytes);
}
}
