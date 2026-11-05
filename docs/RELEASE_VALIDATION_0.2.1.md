# v0.2.1 검증 및 실제 업데이트 장애

2026-11-05. 공개 소스 `e504de4aebfb8f80c49916079d892175a4d64027`. 원본 로그·인증 정보·개인 이메일은 포함하지 않습니다.

## 공개와 검증

- Qt6.11.3/MSVC2022, Windows11, PowerShell7.6.5, NSIS3.13. Windows 네이티브8/8, Python75개 통과.
- Mac 전체 native 원본과 최종 패키지: [run41953061651](https://github.com/Team-HJD-21/team-hjd-game-test-launcher/actions/runs/41953061651). ARM·Intel 빌드·audit·DMG·delivery 모두 성공. 두 아키텍처 모두 Qt6.11.3, deployment target15.0, app1830개/payload1793개, Mach-O168개를 검사했습니다.
- 최종 Windows TestPackage의 설치·복구·제거·재설치와 Installed Apps·게임·설정·unknown 파일 보존을 확인했습니다. 별도 합성 bad-health 복원, 사용 중 제거·다운그레이드·손상 경로 거부도 통과했습니다.
- 실제 제어판 → 프로그램 및 기능 → TeamHJD Game Test Launcher → 제거를 통해 검증 설치를 제거했습니다. 등록·실행 파일 제거와 게임 fixture·설정·unknown 파일 보존을 정상 사용자 권한에서 확인했습니다. 이후 실제 사용자 경로에 공식0.2.0을 재설치했습니다.
- 기존0.2.0→새 Setup의 통제된 종료/새GUI health 전환 검사는 통과했지만, 아래의 실제 자동 전환 장애를 포함하지 못했습니다. 이 좁은 검사를 실제 전체 자동 업데이트 성공으로 해석하지 않습니다.
- [v0.2.1](https://github.com/Team-HJD-21/team-hjd-game-test-launcher/releases/tag/v0.2.1)은 `2026-11-05T02:33:27Z`에 immutable 공개했습니다. 초안·공개11개 파일의 이름·크기·GitHub digest와 로컬 SHA256을 대조했습니다. 실제 태그는 위 전체 SHA와 같습니다.
- 서명 피드 sequence2/버전·최소0.2.1, 기존 고정 P-256 키를 유지했습니다. [Pages41956042923](https://github.com/Team-HJD-21/team-hjd-game-test-launcher/actions/runs/41956042923) 배포 뒤 실제 HTTPS 피드 bytes가 Release 원본과 일치했고 Windows/ARM/Intel latest 설치 링크의206·전체 크기를 확인했습니다. v0.1.0부터7개 릴리즈 기록이 표시됩니다.
- 계정별 패키지 GET2개 제한을 운영 서버 재시작으로 적용했습니다. 전체 이메일 허용 정책은 변경하지 않았습니다. 새 Qt의 실제 native TLS·인증 catalog·정본 다운로드·이어받기 및304개 파일 검증이 통과했습니다.8443·8844는127.0.0.1입니다.

## 실제 PC 자동 업데이트에서 발견한 장애

사용자 PC의 실제0.2.0이 새 피드를 발견하고0.2.1 Setup을 다운로드·실행했으나, 설치기의 `--wait-client` 단계에서 ‘업데이트 요청자를 확인하지 못했습니다’로 중단했습니다. 완료·정상 자동 업데이트로 기록하지 않습니다. 공개 자산을 덮어쓰지 않고 다음 버전에서 수정합니다.

재현한 OS 조건: 종료된 프로세스의 HANDLE을 보유하면 PID 객체가 남습니다. `WaitForSingleObject`는 종료를 알리고 `GetProcessTimes`의 생성 시각은 원래 값과 일치하지만 `QueryFullProcessImageName`은 ERROR_GEN_FAILURE31로 실패합니다. 기존 liveness용 `processStamp`가0을 반환해 동일 부모를 거부했습니다. 합성 retained-HANDLE 검사로 조건을 재현했습니다. 한글 helper stderr의 NSIS 인코딩 감지도 별도 보완 대상입니다.

다음 패치는 살아 있는 프로세스의 이미지·시각 검증을 유지하고, 이미 종료된 객체에 한해서 OS 생성 시각·정확한 bootstrap running 기록·활성 슬롯·설치 receipt와 전체 파일 검증을 모두 요구합니다. 알 수 없는 live PID를 통과시키거나 파일/health 검증을 끄지 않습니다. 이 문서는 해당 후속 버전의 릴리즈 완료 증거를 대신하지 않습니다.

사람의 신규 이메일 OTP·Unity 실제 게임 플레이·PC 재부팅·OS 보안 경고 승인·유료 게시자 서명/Apple 공증은 이번 검증에 포함하지 않았습니다.
