# 0.2.3 릴리즈 검증

2026-11-06 KST. [v0.2.3](https://github.com/Team-HJD-21/team-hjd-game-test-launcher/releases/tag/v0.2.3)을 00:50 KST에 공개했다. 불변 릴리즈의 11개 파일 크기·GitHub SHA256을 검증본과 전부 대조했다. 릴리즈와 피드의 sourceCommit은 `67d8b40b34d22cf98a8e4249f366e68afa5e0e45`, 네이티브 빌드 소스는 `c3b7194982936282b3e984174b02efaa2a3091a2`다. 기존 엄격한 소스 재사용 검사로 두 커밋의 네이티브 소스가 같음을 확인했다. 정상 사용자 설치를 변경하지 않고 별도 프로필로 아래 검사를 수행했다.

## 실제 운영과 공개 업데이트

| 대상 | 확인한 결과 |
| --- | --- |
| 이메일·기기 로그인 | 사용자가 안내3에 직접 동의하고 이메일 인증했다. 후속 검사는 그 브라우저 세션의 Approve를 사용했다. 운영 서버의 JWT 확인·기기 등록·서명 catalog 검증 후 DPAPI 암호화 키와 목록이 저장됐다. 기존 JWT는 최종 프로필에 저장하지 않았다. |
| 재실행 | 정상 종료 후 같은 프로필로 실행했다. 브라우저 재인증 탭 없이 서버 온라인으로 연결되고, 설치한 BUILD24와 플레이 버튼이 유지됐다. catalog 저장 시각도 갱신됐다. |
| 게임 설치 | 실제 비공개 Windows BUILD24를 검증용 폴더로 내려받아 검증·설치했다. 설치 영수증과 351,737,265bytes의 설치 파일을 확인했다. Unity 플레이·멀티플레이 검사를 뜻하지 않는다. |
| 실제 0.2.2→0.2.3 | 공식 ZIP의 67,053,560bytes와 SHA256을 대조한 실제 0.2.2를 격리 실행했다. **비교 버전 override 없이** 이메일 인증 전 승인 창→진행 창→자동 재실행→0.2.3 GUI health commit을 확인했다. 공개 payload 62,210,288bytes의 SHA256과 새 1,503개 파일의 크기·해시가 모두 일치했다. 이전 0.2.2 슬롯이 보존되고 pending/failure 상태는 없었다. |
| 공개 사이트 | 실제 HTTPS 피드가 검증본과 동일 bytes이며 제품 고정 키·sequence4·버전0.2.3을 검증했다. 업데이트 기록에 v0.2.3이 표시된다. 공개 이벤트 dispatch42059020603과 Pages42059042779가 성공했다. |
| 인증 경계 | 익명 기기 challenge/catalog는401, 최초 등록·기존 catalog·유사한 별도 namespace는 Access302였다. 정상 기기 요청은 서명 catalog와 게임 설치까지 통과했다. 로컬 관리 서비스는 외부에 노출하지 않았다. |
| Chrome | Approve 뒤 인증 탭만 닫혔다(8→7). 기존7개 탭과 Chrome 연결은 유지됐다. 고정 도구의 `--auto-close` 동작이며 브라우저 전체를 종료하는 코드는 없다. |

최초 이메일 인증과 이후 기기 서명 인증을 운영 환경에서 확인했다. 이메일 JWT와 기기 서명, TLS 검증을 유지했다. 공개 피드와 실제 구버전 전환 검사 뒤 전체 최소 버전도0.2.3/안내3으로 활성화했다. 상세 운영 설정과 관리 정보는 비공개 인수인계로 보관한다.

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
| 사이트 | JS 구문, 공개 파일 경계·정적 자산 hash, 서명 웹 피드12개 assertion과 최종 제품 고정키 피드0.2.3/sequence4 검증 통과 |
| Defender | 최종 Windows 후보 폴더와 패키징 완료된 Setup 별도 검사에서 위협 없음,exit0. 기존 보호/예외 설정 변경 없음. 미래 감염이나OS 침해를 보장하는 결과는 아님 |

앞선 후보 `e86e5fc4f35e5413ee6a0cce7588cbc9451e8a05`에 이어 안내2문구 수정까지 포함한 최종c3b7194의Setup을 새 폴더에서 다시 검증했다. 정상 사용자 설치는 제거하지 않았으며,0.2.2 활성 슬롯의1503개 파일 크기·SHA256도 읽기 전용으로 대조해 모두 일치했다.

검사 후 정확한 소유 경로·제품·버전·실행 프로세스를 대조해TestPackage 등록만 제거했다. 정상0.2.2 등록은 유지했다. Mac의 최종8개 증거/파일 묶음과 정확한source/run/각단계PASS를 확인하고, 공개용6개바이너리만 원본bytes 그대로 새dist에 복사해 SHA256을 대조했다. ARM은공식GitHub CLI로 수신했다. Intel은114개 완료Range를 보존한 뒤 시간 초과된1개만 재수신했고 전체240837940bytes/원본artifact SHA256 `043a8002a327a53bbb12a70c9ed56c9d20fcd76e48b7f926b28028d96686194d`,ZIP CRC/정확8항목을 확인했다. 내부Mac ZIP을Windows에서 다시 압축하지 않았다.

## 원인·진단과 남은 검사

- 새 Mac runner의 무시된 `.local/` 부모 부재는 fixture의 부모 생성으로 고쳤다. 후속 서버 준비 지연은 run42013676617의 실제 스레드에서HTTPServer→socket.getfqdn→gethostbyaddr로 확인했다. DNS 대상이 아닌 loopback fixture 생성 동안만localhost로 고정하고 원래5초 준비 상한을 유지한다. 제품 DNS/TLS/인증을 바꾸지 않았다.
- 수동 게임 업데이트의 원자 저장 실패1회는 구체 원인이 미확정이다. 진단 메시지에 basename/Qt 오류를 추가한 최종 검사는 통과했고 직접 덮어쓰기나 무진단 재시도는 추가하지 않았다.
- 최종Mac 전체native·HTTP·의존성 감사·DMG·delivery는 run42014278357에서 통과했다. 과거Intel macdeployqt240초 초과의내부원인은 미확정이다. 이번verbose2/600초상한에서Client배포→후속단계는ARM약196초/Intel약157초였다. 보안 감사 조건은 유지했고 상한 증가만으로 과거 원인을 해결했다고 주장하지 않는다.
- 후보 검사 당시 잠금 화면으로 구분했던 Windows 픽셀 검수는 후속 운영 검사에서 실제 제품 창으로 수행했다. OS가 다른 활성 창을 유지할 수 있어 촬영 전에는 직접 활성화했다. 매번 OS 포커스를 가져온다고 주장하지 않는다. Mac의 사람 OTP·게임 플레이·재부팅은 여전히 미검증이다.
- 최초 운영 검사에서 기기 경로가 부모 로그인으로302 이동했다. 최종 설정과 정상·익명 응답은 상단에 기록했다. 후속 통신 실패1회의 세부 원인은 미확정이다. 별도 제품 코어 검사는 TLS 명시 초기화 유무 양쪽에서 성공했고, 근거 없이 제품 코드나 TLS 검증을 변경하지 않았다. 최종 실제 GUI 인증·설치·재실행은 성공했다.
- main 병합·원치 않은 타인 승인 강제 규칙 정리·11개 draft 대조·불변 공개·웹 피드·실제 구버전 전환을 완료했다. 기기 회수는 합성 C++/Python 회귀와 운영 CLI 검증 범위이며, 실제 운영 검증 사용자의 기기는 회수하지 않았다.

원본 로그·이메일·키·토큰·서명 다운로드 URL·비공개 게임은 공개 기록에 첨부하지 않는다. 비공개 증거와 새 출력은 `.local/`에 보관한다.

## 최종 후보 바이너리 식별

아래9개 바이너리와 서명 피드·SHA256SUMS의 총11개를 공개했다. GitHub의 uploaded 상태·크기·digest가 모두 일치한다. 공개 태그·자산은 덮어쓰지 않는다.

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
| launcher-stable.json | 2822 | `9b4e1b96fe1b94e3e338a057c4dce1d131b04c47b218ff6b7780c128039696a8` |
| SHA256SUMS.txt | 1057 | `6157c7cdfd2834c50c4f7a9ccdd5c83b8ab4d5e67ad1695933a02a5fe210a465` |
