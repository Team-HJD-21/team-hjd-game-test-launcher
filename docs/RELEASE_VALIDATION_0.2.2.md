# v0.2.2 릴리즈 검증

2026-11-05. 공개 바이너리 소스 `b26565981642526fcda697dd6ead97c4cc6557b6`, Qt6.11.3. [정식 Release](https://github.com/Team-HJD-21/team-hjd-game-test-launcher/releases/tag/v0.2.2)는 `2026-11-05T03:49:10Z`에 immutable 공개했습니다. 원본 로그·이메일·인증값·게임·개인키는 공개하지 않습니다.

## 수정과 근거

- Qt6.8.3을 보안 수정이 포함된 무료 공식 SDK6.11.3으로 교체하고 Windows·ARM Mac·Intel Mac을 전체 다시 빌드했습니다. 공식 metadata/archive 체크섬을 유지하며 DLL만 수동 교체하거나 이전 Mac 바이너리를 재사용하지 않았습니다.
- 업데이트는 이메일 인증 전에 확인하며 **업데이트 시작** 승인 후 별도 진행 모달에서 다운로드·검증하고 자동 재시작합니다. 시작·약1분·창 복귀에 사이트/GitHub 정적 서명 피드를 대조합니다. 오래된 정상 사이트 피드가 최신 GitHub 피드를 가리지 않으며, 같은 sequence의 다른 내용은 거부합니다.
- 게임 업데이트는 사용자가 버튼으로 적용합니다. 연결·최신 런처·인증 확인 전에는 게임 실행을 포함한 기능이 차단되며, 사용자 최종 요청에 따라 오프라인 우회를 제거했습니다.
- 실제0.2.0→0.2.1의 종료된 부모 HANDLE 오류를 재현했습니다. 종료 객체의 이미지 조회 실패31은 검증된 원인입니다. 살아 있는 부모의 이미지·생성 시각 검사를 유지하고, 종료된 경우 OS 시각·정확한 bootstrap 기록·활성 슬롯·receipt·전체 파일을 모두 요구합니다.
- NSIS helper 오류는 UTF-16LE+BOM과 binary stderr를 사용합니다. CRT가 LF 바이트를 늘려55bytes 홀수가 되는 원인을 확인했고 최종 helper의54bytes·엄격 decode·한글 원문·줄바꿈을 검증했습니다.
- 설치 마법사의 깨진 비트맵 사이드바를 없애고 원본 로고·네이티브 텍스트·어두운 배경·금색 버튼으로 변경했습니다. 자체 테마 플러그인은 화면만 그리며 업데이트/인증 검증을 변경하지 않습니다.

## 실제 사용자 PC 업데이트

Windows11/MSVC2022/PowerShell7.6.5/NSIS3.13 환경입니다. 실제 정상 설치 경로의0.2.0을 안정된 실행기로 열어 공개0.2.2 피드를 발견하고, 실제 다운로드된 새 Setup에서 **현재v0.2.0 → 새v0.2.2** 승인 화면을 확인·캡처했습니다. 사용자가 직접 승인하고 완료를 확인했습니다. 이 과정은 비교 버전·프로필 override를 사용하지 않았습니다.

업데이트 후 정상 Installed Apps 등록0.2.2, active0.2.2, 실행 GUI의PID·이미지·OS 생성 시각·bootstrap health 인수, 최종 공개 Client SHA를 대조했습니다. 활성 슬롯1503개 파일과 bootstrap manifest26개 파일의 크기·SHA가 일치했습니다. 같은 실제 사용자 프로필의 게임1개·304개 정본 파일·receipt·active, 서버/공개키/게임 경로/개인정보 선택의 설정 지문과 보호된 인증값 존재가 업데이트 전후 보존됐습니다. 비밀값을 로그에 출력하거나 인증 완료를 대신 조작하지 않았습니다.

사용자는 **현재 설치본을 유지하고 제거 검사는 검증용 결과로 마무리**하도록 지정했습니다. 따라서 정상0.2.2를 다시 제거하지 않았습니다. 실제 사용자 설치의 제거·재설치가 통과했다고 주장하지 않습니다. 승인 화면은 확보했으나 사용자가 먼저 완료한 이후의 모든 진행 프레임을 녹화한 것은 아닙니다.

## 회귀·설치기·Mac

| 검사 | 결과와 범위 |
| --- | --- |
| 최종 Windows native | 9/9,43.25초 통과. 서명·승인·오프라인 차단·연결·bootstrap·최대화·게임 수동 업데이트 포함 |
| 최종 TestPackage | 설치·복구·제거·재설치, Installed Apps 등록, 슬롯 보존, unknown 파일·게임·설정 보존 통과 |
| 실제 GUI→Setup | 부모 SafeHandle을 종료 후에도 유지한 전환·새 슬롯·새 GUI health 통과 |
| 실패 경로 | failed-health 복원, 잘못된 live 이미지/시각·기록 없음 거부, 다운그레이드·사용 중 제거·손상 경로 거부 통과 |
| 제어판 UI | 별도 검증 설치의 프로그램 및 기능→제거를 확인한 이전0.2.1 실화면 검수. 이를 정상0.2.2 제거 증거로 확대하지 않음 |
| 서버 | 전체 Python75개 및 PR Security regression gate 통과. 실제 native TLS·인증 catalog·정본 게임304개 파일·이어받기 통과. 최소0.2.2 정책 적용 뒤 재검사도 통과 |
| 최종 Mac | [run41960606630](https://github.com/Team-HJD-21/team-hjd-game-test-launcher/actions/runs/41960606630) ARM·Intel 전체 native와 최종 packaging 모두 성공. Qt6.11.3/macOS15.0 |

Mac 양쪽 모두 app1830개·payload1793개·Mach-O168개를 감사했고 architecture·의존성·모드·strict ad hoc 서명, read-only DMG mount의 전체 bytes/modes·Applications 링크·약관, 실제 packaged GUI, seed·중단 복구·queue 활성화·안정된 앱 재실행·failed health 복원이 통과했습니다. 원본이나 candidate만 공개한 것이 아닙니다. 중간 Intel macdeployqt240초 지연은 새 runner에서 실패 job1회 재시도했고, 최종 C++ 수정 뒤에는 양쪽을 다시 전체 빌드했습니다. Qt 내부 지연 원인은 미확정입니다.

## 공개 파일과 웹

초안 및 실제 latest API의11개 정확한 이름·크기·digest를 로컬 최종 SHA와 대조했습니다. Windows Setup42731775bytes, ARM DMG80506140bytes, Intel DMG89598816bytes이며 실제 latest 다운로드3개 모두 Range206·전체 크기가 일치했습니다. 기존 공개 태그/파일을 덮어쓰지 않았습니다.

기존 제품 고정키의 P-256 피드 sequence3/버전·최소0.2.2를 유지합니다. [main Pages41963945322](https://github.com/Team-HJD-21/team-hjd-game-test-launcher/actions/runs/41963945322) 이후 실제 HTTPS 피드가 Release 원본과 byte 단위로 일치했습니다. 업데이트 기록은v0.1.0부터v0.2.2까지8개이며 공개 Release 원문을 보존합니다. 공통 header/footer·언어 메뉴를 유지했고390px의 한국어/영어/중국어에서 가로 넘침을 확인하지 못했습니다. 최종 공개 화면에0.2.2와 세 설치 링크가 표시됩니다.

Release 태그에서 직접 Pages를 실행한41963948237은 main 전용 환경 정책에 거부됐습니다. 환경 보호를 유지하고 별도 dispatcher→main workflow_dispatch로 수정했습니다. [dispatcher41966128405](https://github.com/Team-HJD-21/team-hjd-game-test-launcher/actions/runs/41966128405)가 생성한 [main Pages41966141756](https://github.com/Team-HJD-21/team-hjd-game-test-launcher/actions/runs/41966141756) 배포가 성공했습니다. 같은 수정의 [Security regression gate41966128394](https://github.com/Team-HJD-21/team-hjd-game-test-launcher/actions/runs/41966128394)도 통과했습니다. 향후 태그에는 새 dispatcher 정의가 포함되어야 하며 이미 공개된 태그를 수정하지 않습니다.

## 운영 보안과 남은 범위

운영의 계정별 패키지 동시GET2개 제한을 적용했고 전역 연결/처리 상한도 유지했습니다. 모든 이메일 OTP 허용 정책과 무료 운영을 유지했습니다. 수집 항목·목적·보관 기간은 바뀌지 않아 개인정보 동의 버전2026-11-05.1을 유지하고 이용조건은2026-11-05.2로 갱신했습니다. 사용자별 인증 이메일·시각 기록은 비공개 관리 화면에서 확인하며 마지막 접근180일 후 정리합니다.

실제 로컬 관리 비인증401, 비인증 cross-origin POST403·CORS 미허용, 익명 공개 catalog의 Access302,8443/8844 loopback 전용, 운영 저장소/서명키의 제한된 DACL을 확인했습니다. 릴리즈 태그 수정·삭제 차단, 관리자만 태그 생성, immutable Release를 적용했습니다. main의 필수 Security regression gate·PR1명 승인·최근 push 승인·미해결 대화 차단·force push/삭제 차단은 저장소 보호 도구로 적용·GET 대조하는 최종 운영 단계입니다. 계정/조직 전체2FA 설정은 변경하지 않았습니다.

Windows 게시자 서명·Apple Developer ID/공증은 추가 비용 없는 범위에 포함하지 않았고 사이트 FAQ에 첫 실행 안내를 유지합니다. 사람의 신규 이메일 OTP·Mac 실제 Unity 플레이/크로스플레이·OS 경고 승인·실제 재부팅은 이번 검사에 포함하지 않았습니다. 로그인 자동 시작은 등록/경로를 확인했으며 로그인 전·절전·인터넷 단절 중 서비스된다고 보장하지 않습니다. Google API 연동은 보류했습니다.

비공개 재현 증거는 이 PC의 `.local/release-0.2.2-final.json`이 가리키는 root에 실제 업데이트/공개 파일/Mac/경계 검사 JSON으로 보관합니다. 설치기 결과는 `.local/installer-verification-20261105-122251-126/result.json`, 데이터 보존은 `.local/production-distribution/release-verification/`입니다. [릴리즈 절차](RELEASING.ko.md)·[장애 대응](TROUBLESHOOTING.ko.md)·[팀원/운영 안내](DISTRIBUTION_HANDOFF.ko.md)를 다음 발행의 기준으로 사용합니다.
