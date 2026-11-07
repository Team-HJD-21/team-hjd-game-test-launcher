# 0.2.4 Mac 설치·업데이트 검증

2026-11-07 KST. Mac에서 DMG 복구 안내와 업데이트가 반복되는 문제를 수정한다. [v0.2.4](https://github.com/Team-HJD-21/team-hjd-game-test-launcher/releases/tag/v0.2.4)를 **2026-11-07 17:23 KST**에 immutable로 공개했다. 릴리즈·서명 피드 sourceCommit은 `f40a3e23bf17c84e10e2247ab82b041f17b6a824`이며 실제 native source와 엄격한 동일성 검사를 통과했다. 정확11개 원격 uploaded 상태·크기·SHA256을 로컬 검증본과 전부 대조했다.

## 확인한 원인

공개 0.2.3 DMG의 크기·SHA256을 확인하고 ARM·Intel에서 macOS 기본 복사 도구 `ditto`로 앱을 설치했다. 두 환경 모두 `Contents/Resources/seed/._teamhjd-receipt.json`만 누락됐다. 앱의 strict 서명 검증도 실패했고 실제 실행기는 종료1과 “초기 런처가 없습니다. 공식 DMG로 설치해 주세요.”를 반환했다. 진단 run42352684720의 실행 성공은 정상 설치 통과가 아니라 이 실패를 재현한 결과다.

기존 검사는 Python `copytree`로 앱을 복사해 이 파일을 보존했다. 실제 macOS 복사 동작과 달라 놓친 회귀다. 최초 Finder 자동화 실패는 설치 결과로 계산하지 않았다. 후속 run42354244633에서 올바른 Finder alias로 실제 복사를 수행하자 **양쪽 Finder에서도 같은 파일 누락·strict 서명 실패·종료1을 재현**했다. 같은 원본을 Python으로 복사한 앱은 파일 손실 없이 strict 서명·실제 GUI health를 통과했다. 초기 진단의 출력 pipe를 파일로 분리한 재검사도 종료0으로 완료했다.

또한 앱 교체가 GUI health 검사 전에 실패하면 업데이트 queue·pending이 남는 코드 경로를 확인했다. 다음 실행에서 같은 적용을 반복할 수 있었다.

## 수정

- 초기 manifest를 앱의 `Contents/Resources/seed-manifest.json`에 둔다. macOS가 AppleDouble 메타데이터로 취급하는 `._` 이름을 초기 설치에 사용하지 않는다. 관리 runtime과 게임의 기존 receipt 형식은 유지한다.
- 초기 seed의 모든 파일 크기·해시·실행 권한과 앱 전체 strict 서명 검증은 유지한다. 새 manifest는 앱 서명의 보호 범위에 있으며 seed tree 밖에 있어 검증 예외를 늘리지 않는다. 기존 정상 설치의 manifest도 읽을 수 있다.
- 앱 교체 실패도 transaction 복원 대상으로 처리한다. 이전 버전을 보존하고 queue·pending을 정리하며 같은 버전을 자동 재적용하지 않는다. 사용자 재시도와 새 패치 확인은 유지한다.
- 읽기 전용 디스크와 설치 폴더 쓰기 권한을 구분해 안내한다. 관리자 권한 상승·보안 해제·검증 생략을 추가하지 않는다.

## 검사 상태

최종 native source는 `a9c1b8e5e7958d86089a54cfa526c0f8b3e1ceee`이며 전체 ARM·Intel run42353603435의 **native·package 전체4job을 통과**했다. 양쪽6개 CTest와 실제 native 기기 HTTP 검사, 의존성·파일/권한·strict 서명 감사, DMG read-only mount, 실제 DMG→ditto 복사→전체 bytes/modes/서명 동일→첫 GUI health를 확인했다. 쓰기 불가 설치 위치의 이전 GUI 보존·queue/pending 정리·재실행 시 미반복, 권한 복원 후 정상 교체·seed-free 재실행·실패 health 복원도 양쪽에서 통과했다.

Windows11/MSVC2022/Qt6.11.3 최종11/11,44.89초를 통과했다. 별도 TestPackage 설치·복구·제거·재설치·설정/게임/unknown 파일 보존과 최종 Setup의 정확한 부모 대기·새 GUI health·이전 슬롯 보존도 통과했다. 검증 전용 등록은 NSIS 임시 자식 제거기 종료까지 확인해 정리했다. 정상 사용자 설치는 대상으로 지정하지 않았다. 최종 Windows 공개 배포 폴더·Setup의 Defender 검사도 위협 없음/종료0이었다.

첫 sandbox 실행의 TEMP/루프백 실패는 같은 바이너리의 정상 승인 실행으로 대조했다. 전역 TEMP·ACL·방화벽·보안 설정이나 검증 assertion은 변경하지 않았다. 초기 Mac 후보의 추가 QStorageInfo 기반 읽기 전용 검사는 쓰기 가능한 APFS fixture를 오인해 실패했다. 실제 경로를 조회하는 statfs로 바꾼 뒤 양쪽 native 검사를 통과했으며, 그 실패 후보는 공개하지 않는다. 앱 교체 복원 범위는 GUI health까지이고, 이미 건강한 GUI가 실행 중인 commit/백업 정리 실패를 파일 rollback으로 처리하지 않는다.

새 Mac delivery 검사는 실제 최종 DMG를 읽기 전용으로 mount하고 `ditto`로 앱을 설치한다. 원본과 전체 bytes·modes·strict 서명이 같아야 첫 실행을 통과한다. 쓰기 불가 설치 위치에서 이전 GUI 보존·queue/pending 정리·재실행 시 미반복을 확인한 뒤, 권한을 복원해 정상 업데이트와 health 실패 복원을 검사한다.

## 사용자 복구와 남은 범위

0.2.3의 초기 검증 파일이 누락돼 앱 자체가 실행되지 않으면 자동 업데이트를 시작할 수 없다. 이 경우 수정된 최신 DMG의 앱을 응용 프로그램 또는 `~/Applications`에 한 번 새로 복사한다. 기존 게임·설정은 보존한다. 이미 실행 가능한 런처는 서명 피드의 업데이트 승인 흐름을 사용한다. 기존 공개 0.2.3 태그·자산은 덮어쓰지 않는다.

사람의 Mac 이메일 인증, Keychain 승인, Unity 게임 플레이와 재부팅은 별도 미검증 항목이다. 네이티브 실행·설치·복원 검사를 이 항목의 통과로 확대하지 않는다. 인증 정책·게임 원본은 변경하지 않는다.

## 공개 후 검사

실제 공개 DMG를 다시 내려받고 고정한 크기·SHA256을 대조한 run42355259521이 ARM·Intel 모두 성공했다. **각각 Finder·ditto·Python 세 경로 모두** 전체 파일 bytes 유지, 초기 manifest 보존, strict 서명, 실제 GUI health와 종료0을 확인했다. 이전0.2.3의 같은 검사에서는 Finder·ditto 양쪽에서 파일이 사라져 실패했다. OS Gatekeeper 경고의 사람 승인을 대신 수행한 결과는 아니다.

실제 HTTPS 사이트의 서명 피드는 검증본과 동일 bytes이며0.2.4/sequence5와 제품 고정 키 검증을 통과했다. 공개 기록은10개/v0.2.4가 첫 항목이다. Release dispatch42355259609와 Pages42355266579가 성공했다.

공식 Windows0.2.3 ZIP의 해시를 확인한 별도 테스트 클라이언트도 공개 sequence5/버전0.2.4를 받아 저장했다. 추가 화면 검사에서는 PC 잠금 화면이 확인돼 Computer Use 규칙에 따라 중단했다. 이 검사를 실제0.2.3→0.2.4 다운로드·승인·자동 전환 성공으로 기록하지 않는다. 최종 Windows Setup의 GUI health와 설치/제거 보존 검사는 별도 통과 결과다. 정상 사용자 설치는 유지한다.

## 공개 파일 식별

| 파일 | bytes | SHA256 |
| --- | ---: | --- |
| TeamHJD-GameTestLauncher-Windows-x64.zip | 67074441 | `f08fe88f9b5bdcad568c804173ee18d93d4f3c0a9995f437d147f76d6e8925d2` |
| TeamHJD-LauncherPayload-Windows-x64.zip | 62211410 | `efd533a502416a35159463a30afdc47e3f74d948d63f1049a42fce5225917918` |
| TeamHJD-GameTestLauncher-Windows-x64-Setup.exe | 42745683 | `d790fec25d43fe3841a1a8596d7c1a543a925ddf39aaed3a3331ae25591db569` |
| TeamHJD-GameTestLauncher-macOS-arm64.zip | 75360533 | `fef3dd0aa51a248ff23604ee723d811503774e9971e5a5270eda82007a94cff8` |
| TeamHJD-LauncherPayload-macOS-arm64.zip | 63386550 | `345d2ee04410ab1394585bf2b1f1f2e10dce91be32b8cf50090eef227d19029d` |
| TeamHJD-GameTestLauncher-macOS-arm64.dmg | 80522576 | `63c5958e5e23204785c53e8a603ade61cc47ba1a2c847b93dd6d8e26f2ff337d` |
| TeamHJD-GameTestLauncher-macOS-x64.zip | 83409280 | `44e0dd2ac0378d2b0d85c0fc9cc2408d5dfbb9a522d16c6419a599ac50c1b19e` |
| TeamHJD-LauncherPayload-macOS-x64.zip | 69979005 | `a8a335de896a4bcb91d576c03c00e90f18d9745814eb95cb43b7db5d05e6317d` |
| TeamHJD-GameTestLauncher-macOS-x64.dmg | 89637217 | `33f4d238f6ef799d883a6e8a773805aad8072c1107716ec4b10d5fec813fcc03` |
| launcher-stable.json | 2822 | `6beef0265c2e517af1e94d4bda5049353d1513ca099a9bb89dc7e35fc6f410cc` |
| SHA256SUMS.txt | 1057 | `af34260f8f55d86258fe3755efde6fc41aa3fca4d24f60a074cda150fb4d807a` |
