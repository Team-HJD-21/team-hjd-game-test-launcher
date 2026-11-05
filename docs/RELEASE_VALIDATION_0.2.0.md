# v0.2.0 검증 기록

2026-11-05 KST. 현재: v0.2.0 공개 및 실제 latest API·11개 자산 대조 완료. 사이트 배포와 공개 후 클라이언트 검증은 아래에서 구분합니다. 원본 로그·토큰·이메일·개인키·서명 다운로드 URL은 이 문서에 넣지 않습니다.

## 변경

Windows 사용자 단위 Setup과 ARM/Intel Mac DMG, P-256 정적 피드, 인증과 분리된 필수 런처 갱신·자동 재시작·health 복원, 개인정보 개별 선택/180일 보관·권리 처리 도구를 도입했습니다. 게임은 자동 확인·알림 후 명시적 업데이트 버튼을 유지합니다. 앱 제거는 게임·설정·로그인·Unity 데이터·서버 기록을 보존합니다.

## 소스·환경

- 최초 native 소스: `7c3a516f1019a07f590bae6d07740d48a79bbeb4`, 전체 native 빌드 run `41933632479`의 ARM·Intel 두 job 성공.
- 최종 Mac 패키징·추가 앱 교체 QA: source `2d2127e4e4e8e40c97b588b3990d2a56a668f4a8`, run `41942941935`의 ARM·Intel 두 package/delivery 성공. `validate-native-macos-reuse.py`가 동일 native tree와 허용 변경·미만료 원본을 확인했습니다.
- Windows11, VS2022 17.14.39, Qt6.8.3 MSVC x64, PowerShell7.6.5, Python3.13.3, NSIS3.13. Mac runner는 macos-15 ARM / macos-15-intel, Qt6.8.3, deployment target12.0. 네이티브 컴파일 target과 실제 제품 요구사항은 구분합니다. 공식 cloudflared의 LC_BUILD_VERSION이 ARM/Intel 모두15.0인 것을 확인해 앱/웹의 최소 버전을15.0으로 맞추고 모든 Mach-O 최소 요구가 광고 버전 이하인지 검증합니다.
- Windows D3D 라이브러리는 설치된 공식 SDK Redist/x64로 통일하고 복사 후 SHA256을 검증합니다. DLL/SDK/LLVM/Microsoft 라이선스 및 정확한 라이선스 원본 provenance를 포함합니다. 게시자 서명·Apple Developer ID·공증은 구성되지 않았습니다.

## 검증 증거

| 검사 | 결과와 범위 |
| --- | --- |
| Windows CTest | 최종8/8. 서명 피드, bootstrap transaction, 인증/개별 동의, 자동 updater 흐름, core, GUI 시작/종료, 최대화/복원, 수동 게임 업데이트 |
| 최종 Windows payload | 공개할 ZIP 자체를 전체 추출·모든 파일/해시 검증. 그 배포 바이너리의 실제 GUI frame ACK가 PID·0.2.0과 일치 |
| 실제 Setup | 최종 번들 TestPackage로 설치·복구·제거·재설치, Installed Apps·이전 슬롯·게임/설정/unknown 파일 보존. 실제 GUI에서 정확한 부모 종료 후 새 슬롯으로 업데이트·재시작·health commit |
| 실패 복구 | 실제 Windows bootstrap 정상/실패/중단/queue. 별도 C++Installer의 합성 good/bad Client로 health 실패 후 정확한 이전 슬롯 재실행, marker/bootstrap/data 보존과 failure latch 검증 |
| 실패·경계 | 실행 중 제거/다운그레이드 거부, 상위 경로/손상 receipt를 따라 삭제하지 않음, 알 수 없는 파일 보존 후 재설치 |
| Native Mac | ARM/Intel의 전체 바이트·권한·Mach-O/strict signature, 실제 payload roundtrip, 최종 DMG read-only mount/Applications링크/약관 대조. 초기 seed·중단 복사·queue·실패 health 복원 |
| 실제 Mac 앱 갱신 | 유효한 ad hoc wrapper의 버전 정보만 바꾼 fixture에서 실제 안정된 앱을 seed-free 앱으로 원자 교체·strict signature·다시 실행. 옛 버전 바이너리 테스트라고 주장하지 않음 |
| 웹 | 공식3개 설치 링크/허용 정적 파일/브랜드 아이콘/공통 푸터·캐시 버전, ephemeral키로12개 서명 피드 회귀. KO/EN/ZH 모바일375px 실측 가로 넘침 없음·FAQ 펼침/개인정보 페이지 가독성 확인 |
| 개인정보 | 기존 인증/retention/policy/관리 회귀, 별도 retention9개(동의 receipt·중복 session·손상/쓰기/잠금/시각 경계), dashboard4개. 실제 사용자 목록은 공개/테스트에 사용하지 않음 |
| 운영 API | 동일 제품의 native TLS·인증·서명 catalog·정본 게임 다운로드·이어받기·304개 파일 검증 통과. 비공개 owner 인증으로 검사했으며 사람의 신규 OTP·게임 플레이와 구분 |
| 로그인 자동 시작 | 검증 복사한 비공개 PowerShell7.6.5로 HKCU Run과 정확한 private root wrapper 등록 확인. Windows 사용자 로그인 이후 실행이며 실제 재부팅은 미검증 |
| 운영 복구 | 재사용된 보호된 시스템 PID와 오래된 Python state를 구분. 해당 시스템 프로세스를 종료하지 않고 origin·터널·감시·로컬 관리 복구. 동일 시각/다른 실행 파일 충돌은 계속 거부 |

Controller updater fixture는 일회용 테스트키·합성 전송만 사용하며 실제 P256서명·크기·SHA·archive/tree 검사를 유지합니다. 네트워크 실패→timer retry→failed release latch→명시 retry→변조 파일 차단→정상 stage→helper handoff를 확인합니다. 이 증거와 공개 네트워크 검증은 구분합니다.

## 확인된 원인과 수정

1. unknown 파일을 남긴 제거에서 ownership marker를 먼저 삭제해 재설치가 막혔습니다. NSIS의 마지막 정리 뒤 완전히 비었을 때만 marker를 지웁니다. 실제 재설치 통과로 확인했습니다.
2. `hdiutil -srcfolder`가 `._teamhjd-receipt.json` 한 개를 AppleDouble로 분류해 DMG에서 누락했습니다(missing1/extra0/changed0). writable 이미지에 직접 파일을 복사하고 UDZO 변환 후 최종 mount 전체 대조를 통과했습니다. receipt 검사를 제외하지 않았습니다.
3. 이전 origin PID는 재부팅 후 svchost.exe가 사용했고 보호된 StartTime/Path 조회는 불가했습니다. 이미지가 다르고 CIM 생성 시각도 다른 경우만 stale로 판정합니다. 정확한 생성 시각인데 다른 실행 파일/인수인 충돌은 기존처럼 거부합니다. 운영 origin을 복구했고 시스템 프로세스는 건드리지 않았습니다.

## 게시 증거

- [v0.2.0](https://github.com/Team-HJD-21/team-hjd-game-test-launcher/releases/tag/v0.2.0) 공개: `2026-11-05T01:00:12Z`. 태그의 실제 전체 SHA는 `2d2127e4e4e8e40c97b588b3990d2a56a668f4a8`입니다.
- Draft와 공개 latest API 양쪽에서 정확한11개 자산의 uploaded 상태·크기·GitHub SHA256 digest가 로컬 최종 검증본과 모두 일치했습니다. 기존 공개 태그/자산은 덮어쓰지 않았습니다.
- 피드 sequence1, 버전/온라인 최소0.2.0, 제품 고정 P-256 공개키로 서명 검증 완료. 3플랫폼 payload/installer의6개 이름·immutable URL·크기·해시와 sourceCommit이 일치합니다. 체크섬은9개 바이너리와 피드의10개를 포함합니다.
- 정적 피드/사이트 배포, 최소버전/동의 정책 활성화, 실제 기존0.1.4 및 새 피드 다운로드·적용은 공개 후 별도 검사로 이어집니다. 완료 전 통과로 쓰지 않습니다.

| 파일 | bytes | SHA256 |
| --- | ---: | --- |
| TeamHJD-GameTestLauncher-Windows-x64.zip | 61974331 | `3e9d39d3d8eba000eb039364b7e95cc1e7a1ca76308d0bd0682e9bd6a1ac5847` |
| TeamHJD-LauncherPayload-Windows-x64.zip | 58282586 | `0eea3a233708980d7ee16a5f1fb8cfce05c34442be2ddacdb344dfee7e915a8d` |
| TeamHJD-GameTestLauncher-Windows-x64-Setup.exe | 40082850 | `fced582d2830b4aeb7cd6e50bddb956c96a382415bd6149067ccc78d934b0812` |
| TeamHJD-GameTestLauncher-macOS-arm64.zip | 72661677 | `ab5a3258f88056826644fd67e739891f8ffa0cd3cbd816c8631d716956b85d13` |
| TeamHJD-LauncherPayload-macOS-arm64.zip | 60964961 | `924191ab96e2647870355a84db87e28273a18f129b134daf4c79a4d1f748fe74` |
| TeamHJD-GameTestLauncher-macOS-arm64.dmg | 77637326 | `c0026d0e2fb450e4070c4f6585995f2cc9389de5fcc8f4cb4814b7ed05d44c27` |
| TeamHJD-GameTestLauncher-macOS-x64.zip | 80044495 | `318ff24c8cea677718e907a8b9d9f701bf3fb65104b8193f8574977bf9216966` |
| TeamHJD-LauncherPayload-macOS-x64.zip | 67003622 | `f6b2d099603ed62e768b81c3b98629547ef8e15dd05ac50b1a424e902e8b802e` |
| TeamHJD-GameTestLauncher-macOS-x64.dmg | 86025187 | `075a3d7a9d8d822d03ec7be148b1093c5c004ad350878238efb8218cfc4a7939` |
| launcher-stable.json | 2822 | `ca971844b8360cc1cdf39e7a43e3360116726634d70f3d7c12ee452cf7915d93` |
| SHA256SUMS.txt | 1047 | `ba3bf8b6eae73a528d10d9a6a06823f703dba14bac3dd34c001fe605a72c1ec1` |

## 남은 검증 범위

사람의 신규 이메일 OTP·macOS Keychain 승인, Unity 실제 게임/크로스플레이, 실제 재부팅·로그온 자동 시작, macOS12–14(지원 대상 아님)·Windows10 실기·조직 보안 정책의 경고 승인은 이번 자동 검증 범위 밖입니다. Windows 잠금 중 CUA 픽셀 캡처는 잠금 화면을 반환해 실제 앱 화면 증거로 쓰지 않았습니다. 앱 자체 캡처/실제 frame ACK와 Windows·Mac native 실행 증거를 구분했습니다.

일회용 개인정보 선택을 사람 대신 수락하지 않았습니다. 프로그램 제거와 서버 기록 삭제는 별개이며, 국외 지역 제한·특정 DPA 체결·법적 적합성 인증을 확인 없이 주장하지 않습니다. 인터넷/배포처·권한·디스크·OS 차단까지 강제로 해결한다는 뜻의 업데이트 보장은 하지 않습니다. 정상 조건에서는 온라인 사용 전에 서명된 최신 버전을 확인·적용하고 실패를 숨기지 않습니다.
