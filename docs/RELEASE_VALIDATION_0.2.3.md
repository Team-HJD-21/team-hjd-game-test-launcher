# 0.2.3 후보 검증

2026-11-05 KST. **아직 공개 릴리즈가 아니다.** 현재 공개 최신 버전과 정상 사용자 설치는0.2.2다. 후보 소스는 `c3b7194982936282b3e984174b02efaa2a3091a2`이며 문서 후속 커밋과 네이티브 원본을 구분한다. 운영 origin·Cloudflare 정책·최소 버전·서명 피드는 아직 전환하지 않았다.

## 변경

이메일 최초 인증 뒤 기기별 암호화 키로 로그인 유지, 일회용 요청 서명과 접근 회수, 인증 성공 후 앱 복귀, 파일 메뉴/도움말의 런처 경로, 주기 확인의 조용한 footer와 점 애니메이션을 추가했다. Google 로그인이나 유료 서비스는 추가하지 않았다. 새 개인정보 안내3은 사용자에게 별도 동의를 받는다. 기존 서명·해시·health 복원·온라인 필수·명시적 게임 업데이트 정책을 유지한다. [보안 검토](SECURITY_REVIEW_0.2.3.md)를 함께 본다.

## 완료한 검사

| 대상 | 실제 결과와 범위 |
| --- | --- |
| Windows11/MSVC2022/Qt6.11.3 | 최종 네이티브11/11,45.81초. 실제 QML/Controller, 암호화 저장·복원, 창 최소화/최대화 복귀, 조용한 확인·점 애니메이션, 서명 피드·부트스트랩·업데이터·수동 게임 업데이트 포함 |
| PE 보호 | 실제 Client/Launcher의CFG·ASLR·고엔트로피·DEP·stack cookie·app/system import 검색 검사 통과 |
| 서버 | 전체93/93,53.077초. 이후 fixture DNS 수정과 최종 네이티브로 인증/HTTP18/18,3.952초 재검증 |
| 실제 C++↔Python 전송 | 합성 loopback edge에서 등록·중복 재시도·OS 암호화 저장 복원·catalog·Range123- 이어받기·전체 해시·회수·회수 후 거절·이미 회수된 기기 로그아웃 통과. 실제 사람 OTP를 대신하지 않음 |
| macOS native | ARM/macOS15.8.2와Intel/macOS15.7.9,Qt6.11.3/Python3.13.17. 양쪽 각각6/6 CTest와 실제 C++↔Python 기기 HTTP 검사 통과(ARM0.552초/Intel1.537초). 설치 이미지·delivery는 별도 단계 |
| macOS 최종 배포 | run42014278357의ARM/Intel 전체4job 성공. 각앱1830파일·payload1793파일·Mach-O168개·최소OS15.0·의존성·strict 서명, DMG read-only mount의bytes/modes·Applications링크·약관/개인정보, 첫seed·queue·안정된앱교체·실패health복원 모두PASS |
| 최종 Windows 설치기 | 별도TestPackage에서 설치·복구·제거·재설치·unknown 파일/게임/설정 보존·Installed Apps 등록 통과. 실제GUI→Setup→새GUI health·종료된 부모HANDLE·이전 슬롯·설정/게임 보존 통과. 사람 OTP는 하지 않음 |
| HTTPS·부트스트랩 | 합성 패키지 HTTPS 설치/이어받기/업데이트/실행 잠금/미신뢰 인증서 거절, 부트스트랩 정상/실패/중단 복원·queue 적용 통과 |
| 사이트 | JS 구문, 공개 파일 경계·정적 자산 hash, 서명된 웹 피드12개 assertion과 현재 제품 고정키 피드0.2.2/sequence3 검증 통과 |
| Defender | 최종 Windows 후보 폴더와 패키징 완료된 Setup 별도 검사에서 위협 없음,exit0. 기존 보호/예외 설정 변경 없음. 미래 감염이나OS 침해를 보장하는 결과는 아님 |

앞선 후보 `e86e5fc4f35e5413ee6a0cce7588cbc9451e8a05`에 이어 안내2문구 수정까지 포함한 최종c3b7194의Setup을 새 폴더에서 다시 검증했다. 정상 사용자 설치는 제거하지 않았으며,0.2.2 활성 슬롯의1503개 파일 크기·SHA256도 읽기 전용으로 대조해 모두 일치했다.

검사 후 정확한 소유 경로·제품·버전·실행 프로세스를 대조해TestPackage 등록만 제거했다. 정상0.2.2 등록은 유지했다. Mac의 최종8개 증거/파일 묶음과 정확한source/run/각단계PASS를 확인하고, 공개용6개바이너리만 원본bytes 그대로 새dist에 복사해 SHA256을 대조했다. ARM은공식GitHub CLI로 수신했다. Intel은114개 완료Range를 보존한 뒤 시간 초과된1개만 재수신했고 전체240837940bytes/원본artifact SHA256 `043a8002a327a53bbb12a70c9ed56c9d20fcd76e48b7f926b28028d96686194d`,ZIP CRC/정확8항목을 확인했다. 내부Mac ZIP을Windows에서 다시 압축하지 않았다.

## 원인·진단과 남은 검사

- 새 Mac runner의 무시된 `.local/` 부모 부재는 fixture의 부모 생성으로 고쳤다. 후속 서버 준비 지연은 run42013676617의 실제 스레드에서HTTPServer→socket.getfqdn→gethostbyaddr로 확인했다. DNS 대상이 아닌 loopback fixture 생성 동안만localhost로 고정하고 원래5초 준비 상한을 유지한다. 제품 DNS/TLS/인증을 바꾸지 않았다.
- 수동 게임 업데이트의 원자 저장 실패1회는 구체 원인이 미확정이다. 진단 메시지에 basename/Qt 오류를 추가한 최종 검사는 통과했고 직접 덮어쓰기나 무진단 재시도는 추가하지 않았다.
- 최종Mac 전체native·HTTP·의존성 감사·DMG·delivery는 run42014278357에서 통과했다. 과거Intel macdeployqt240초 초과의내부원인은 미확정이다. 이번verbose2/600초상한에서Client배포→후속단계는ARM약196초/Intel약157초였다. 보안 감사 조건은 유지했고 상한 증가만으로 과거 원인을 해결했다고 주장하지 않는다.
- 실제품 도움말의 접근성 정보와 경로 속성/폴더 열기 회귀는 별도 합성 프로필에서 확인했다. 현재 Windows 잠금 화면 때문에 해당 창의 픽셀 검수/실제 포커스 이동을 새로 확인하지 못했다. 실제 사용자OTP·운영 기기 API·Mac 게임 플레이·재부팅도 미검증이다.
- Cloudflare 관리자 로그인과 정확한`/client-device/*` 위임 정책 저장, 기존Access 등록 경로 유지·운영 정상/무인증/회수 검사, PR의 타인 승인·main 병합, 정확11개draft 자산 size/digest 대조와 공개·피드·기존 사용자 전환은 남아 있다. 아직 하지 않은 단계를 완료로 표시하지 않는다.

원본 로그·이메일·키·토큰·서명 다운로드 URL·비공개 게임은 공개 기록에 첨부하지 않는다. 비공개 증거와 새 출력은 `.local/`에 보관한다.

## 최종 후보 바이너리 식별

아래9개는 로컬 검증 후보이며 아직 GitHub Release에 게시하지 않았다. 서명 피드와SHA256SUMS는 검토된 최종sourceCommit을 정한 뒤 생성·대조하므로 아직11개 릴리즈 계약 완료가 아니다.

| 파일 | bytes | SHA256 |
| --- | ---: | --- |
| TeamHJD-GameTestLauncher-Windows-x64.zip | 67073355 | `a153c259e3581271b938774b642cfcb9c501ab571112183577a74cec54065b1b` |
| TeamHJD-LauncherPayload-Windows-x64.zip | 62210288 | `9f8726a74ec2ed398f572103b26cbebd6543702d61602666e91df1484aa301be` |
| TeamHJD-GameTestLauncher-Windows-x64-Setup.exe | 42738345 | `f01e2f841519adbc9141d16dd47d9fec51df6629d502ac51e386b0891ae7a69f` |
| TeamHJD-GameTestLauncher-macOS-arm64.zip | 75356701 | `038c009aa1bc8c39a192fab1cc72d8203b153f7be9fce9e8f820602101379a18` |
| TeamHJD-LauncherPayload-macOS-arm64.zip | 63383970 | `c8c787bd3ef56d9b748989a29806db1bb13070b2af24364632169ed8affb2cda` |
| TeamHJD-GameTestLauncher-macOS-arm64.dmg | 80524340 | `1141ac50358348f5f355f527bc7cb101a3684acb589df854c4ed468a4577df52` |
| TeamHJD-GameTestLauncher-macOS-x64.zip | 83405662 | `f6240c46a8523764c2df64ce16f991bdd080ddf94b14c5f8db174e8466c8e37a` |
| TeamHJD-LauncherPayload-macOS-x64.zip | 69976632 | `108af35d0c231a9cb8d739ffb9bb1884c0a8a8ff13a8735441566ff5c15521b0` |
| TeamHJD-GameTestLauncher-macOS-x64.dmg | 89622147 | `47b65737671ba69af418a90ae90ad25bc8203c822622e2e80deba70d42f8dd5e` |
