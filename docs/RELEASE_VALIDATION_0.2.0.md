# v0.2.0 검증 기록

2026-11-05 KST. 현재: 최종 자산 대조와 공개 후 검증 전. 공개를 마치면 아래 게시 증거를 갱신합니다. 원본 로그·토큰·이메일·개인키·서명 다운로드 URL은 이 문서에 넣지 않습니다.

## 변경

Windows 사용자 단위 Setup과 ARM/Intel Mac DMG, P-256 정적 피드, 인증과 분리된 필수 런처 갱신·자동 재시작·health 복원, 개인정보 개별 선택/180일 보관·권리 처리 도구를 도입했습니다. 게임은 자동 확인·알림 후 명시적 업데이트 버튼을 유지합니다. 앱 제거는 게임·설정·로그인·Unity 데이터·서버 기록을 보존합니다.

## 소스·환경

- 최초 native 소스: `7c3a516f1019a07f590bae6d07740d48a79bbeb4`, 전체 native 빌드 run `41933632479`의 ARM·Intel 두 job 성공.
- 최종 Mac 패키징·추가 앱 교체 QA: source `a01f3d24d401149ab09c6fdd4f97ef60b01116e2`, run `41939539019`의 ARM·Intel 두 package/delivery 성공. `validate-native-macos-reuse.py`가 동일 native tree와 허용 변경·미만료 원본을 확인했습니다.
- Windows11, VS2022 17.14.39, Qt6.8.3 MSVC x64, PowerShell7.6.5, Python3.13.3, NSIS3.13. Mac runner는 macos-15 ARM / macos-15-intel, Qt6.8.3, deployment target12.0. macOS12 실제 기기 실행과는 구분합니다.
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
| 운영 복구 | 재사용된 보호된 시스템 PID와 오래된 Python state를 구분. 해당 시스템 프로세스를 종료하지 않고 origin·터널·감시·로컬 관리 복구. 동일 시각/다른 실행 파일 충돌은 계속 거부 |

Controller updater fixture는 일회용 테스트키·합성 전송만 사용하며 실제 P256서명·크기·SHA·archive/tree 검사를 유지합니다. 네트워크 실패→timer retry→failed release latch→명시 retry→변조 파일 차단→정상 stage→helper handoff를 확인합니다. 이 증거와 공개 네트워크 검증은 구분합니다.

## 확인된 원인과 수정

1. unknown 파일을 남긴 제거에서 ownership marker를 먼저 삭제해 재설치가 막혔습니다. NSIS의 마지막 정리 뒤 완전히 비었을 때만 marker를 지웁니다. 실제 재설치 통과로 확인했습니다.
2. `hdiutil -srcfolder`가 `._teamhjd-receipt.json` 한 개를 AppleDouble로 분류해 DMG에서 누락했습니다(missing1/extra0/changed0). writable 이미지에 직접 파일을 복사하고 UDZO 변환 후 최종 mount 전체 대조를 통과했습니다. receipt 검사를 제외하지 않았습니다.
3. 이전 origin PID는 재부팅 후 svchost.exe가 사용했고 보호된 StartTime/Path 조회는 불가했습니다. 이미지가 다르고 CIM 생성 시각도 다른 경우만 stale로 판정합니다. 정확한 생성 시각인데 다른 실행 파일/인수인 충돌은 기존처럼 거부합니다. 운영 origin을 복구했고 시스템 프로세스는 건드리지 않았습니다.

## 게시 증거

아직 대기: 최종11개 자산의 로컬/실제GitHub 크기·digest 대조, stable 공개, 정적 피드/사이트 배포, 최소버전/동의 정책 활성화, 실제 기존0.1.4 및 새 피드 다운로드·적용 확인. 완료 전에 이 항목을 통과로 쓰지 않습니다.

## 남은 검증 범위

사람의 신규 이메일 OTP·macOS Keychain 승인, Unity 실제 게임/크로스플레이, 실제 재부팅·로그온 자동 시작, macOS12 실기·Windows10 실기·조직 보안 정책의 경고 승인은 이번 자동 검증 범위 밖입니다. Windows 잠금 중 CUA 픽셀 캡처는 잠금 화면을 반환해 실제 앱 화면 증거로 쓰지 않았습니다. 앱 자체 캡처/실제 frame ACK와 Windows·Mac native 실행 증거를 구분했습니다.

일회용 개인정보 선택을 사람 대신 수락하지 않았습니다. 프로그램 제거와 서버 기록 삭제는 별개이며, 국외 지역 제한·특정 DPA 체결·법적 적합성 인증을 확인 없이 주장하지 않습니다. 인터넷/배포처·권한·디스크·OS 차단까지 강제로 해결한다는 뜻의 업데이트 보장은 하지 않습니다. 정상 조건에서는 온라인 사용 전에 서명된 최신 버전을 확인·적용하고 실패를 숨기지 않습니다.
