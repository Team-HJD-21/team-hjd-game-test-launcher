# 0.2.3 기기 인증 보안 검토

2026-11-05. 후보 소스 검토이며 운영 활성화·공개 릴리즈 완료 기록이 아니다. Google OAuth·유료 인증 서비스는 추가하지 않는다. 플랫폼별 최종 결과는 별도 릴리즈 검증 기록을 따른다.

## 확인한 경계

- 최초 이메일은 기존 Cloudflare Access OTP와 서버 JWT 서명·issuer·audience 검증을 유지한다. 요청의 이메일/쿠키를 origin에서 직접 신뢰하지 않는다. 개인정보 안내3의 개별 동의를 사용자에게 받는다.
- 장기 인증은 기기별 P-256 키로 요청을 서명한다. Windows 현재 사용자 DPAPI, Mac Keychain의 동기화하지 않는 키를 이용한 AES-CBC/HMAC encrypt-then-MAC으로 개인키를 보관한다. 서버에는 공개키만 저장한다. 평문 키 임시 버퍼는 사용 후 지우며 QML·로그·URL·명령행에 전달하지 않는다.
- 60초 nonce를 method/path/Range/버전/동의와 결합하고 검증 후 원자적으로 소비한다. 재생·변조·다른 기기·동시 재사용을 거절한다. 익명 challenge 작업량과 등록/동시 다운로드/본문/저장 크기에 상한이 있다.
- 공개 업데이트 전송은 이메일·기기 인증을 받지 않는다. 기존 고정 키 피드·catalog 서명, 크기/해시, 압축 경로·링크, health ACK와 실패 복원 검사는 유지한다.
- 저장 장애503과 실제 인증 실패401을 구분한다. nonce 만료409는 새 challenge로1회만 재시도한다. 로그아웃은 서버 회수와 로컬 키 삭제를 수행하며 이미 회수된 기기도 정리한다. 오프라인이면 회수 미확인을 알린다.
- 운영자 회수는 별도 로컬 CLI의 정확한ID/미리보기/명시적 적용으로 제공한다. 사용자 삭제·180일 정리는 기기를 함께 정리하며 삭제된 이메일을 기기 접근으로 재생성하지 않는다. OTP 인증 횟수와 일반 접속 시각을 구분한다.
- Windows 실제 PE에서CFG/ASLR/고엔트로피/DEP, stack cookie, app/system 전용 import 검색 플래그를 검사한다. 개발 테스트도app-local Qt를 배치하며 보호 플래그를 끄지 않는다.

## 검증

Windows11/MSVC2022/Qt6.11.3 네이티브11/11, 서버 전체93/93을 통과했다. 실제 C++의 암호화 저장·복원·등록·catalog·Range 이어받기·회수·회수 후 재요청 차단·이미 회수된 기기 로그아웃을 합성 loopback 서버로 교차 검증했다. 서버 JWT·등록 경계·재생·20개 동시 증명·저장 손상·보관기간·삭제·구형 동의가 새 동의를 낮추지 않는 단계 전환도 검사했다. 실제 사람 OTP와 운영 Cloudflare 경로 검증은 이 합성 결과에 포함하지 않는다.

## 운영 전 조건과 한계

1. 기존 전역0.2.2 정책을 유지하며 새 기기 경로만0.2.3/동의3으로 먼저 검증한다. `/client-device/*`에만 구체적인 Access 앱을 두어 origin 서명 인증에 위임한다. 최초 등록·기존 게임 경로·관리 서비스는 공개하지 않는다. 실제 정책 저장 전 설정 범위를 검토한다.
2. 보호된main의 다른 검토자 승인과Security gate, 최종 Mac ARM/Intel native·의존성 감사·DMG·delivery, Windows 설치기 검증이 필요하다. 후보 빌드 성공만으로 공개하지 않는다.
3. 회수는 새 요청부터 적용한다. 이미 전송한 bytes와 내려받은 게임은 원격 삭제하지 않는다. 이메일 자체의 재등록 차단은 현재 모든 이메일 허용 정책과 별개다.
4. OS/같은 사용자 권한 악성코드 침해와 첫 설치 파일의 게시자 신원은 이 설계만으로 해결되지 않는다. DPAPI/Keychain은 하드웨어 attestation이나 완전한 랜섬웨어 방어가 아니다. 현재 Windows Authenticode·Apple Developer ID/공증은 없다. 백신 예외·보호 해제·인증 검사 생략을 해결책으로 쓰지 않는다.

공식 근거: [Microsoft DPAPI](https://learn.microsoft.com/en-us/windows/win32/api/dpapi/nf-dpapi-cryptprotectdata), [정적 import 검색 플래그](https://learn.microsoft.com/en-us/cpp/build/reference/dependentloadflag), [Cloudflare 경로별 앱 우선순위](https://developers.cloudflare.com/cloudflare-one/access-controls/policies/app-paths/). 보호 저장소와 import 검색의 적용 범위를 전체 OS 보안 보장으로 확대하지 않는다.
