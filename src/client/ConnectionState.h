#pragma once
#include <QString>

struct ConnectionState {
    QString id,label,detail;
};

inline ConnectionState connectionState(bool hasServer,bool loginRequired,bool hadSession,bool checking,bool online,bool attempted){
    if(!hasServer)return {"unconfigured",QStringLiteral("서버 연결 전"),QStringLiteral("서버 또는 초대 파일을 설정해 주세요.")};
    if(loginRequired)return {"authentication-required",hadSession?QStringLiteral("이메일 재인증 필요"):QStringLiteral("이메일 인증 필요"),hadSession?QStringLiteral("저장된 이메일 인증이 만료되었거나 확인되지 않습니다. 다시 인증해 주세요."):QStringLiteral("브라우저에서 이메일을 인증하면 서버에 연결됩니다.")};
    if(checking)return {"checking",QStringLiteral("서버 확인 중"),QStringLiteral("인증된 게임 목록을 확인하고 있습니다.")};
    if(online)return {"online",QStringLiteral("서버 온라인"),QStringLiteral("인증과 게임 목록 검증을 완료했습니다.")};
    if(!attempted)return {"connecting",QStringLiteral("서버 연결 중"),QStringLiteral("서버 연결을 준비하고 있습니다.")};
    return {"offline",QStringLiteral("서버 오프라인"),QStringLiteral("이 클라이언트에서 서버 연결 또는 배포 목록을 확인하지 못했습니다. 30초마다 다시 확인합니다.")};
}
