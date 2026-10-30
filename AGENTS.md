# GameTestLauncher 작업 지침

## 먼저 읽기

- 릴리즈·배포 작업: [릴리즈 절차](docs/RELEASING.ko.md).
- 공개 다운로드 페이지/도메인/푸터: [공개 배포 안내](docs/PUBLIC_DOWNLOADS.ko.md). `testlauncher`는 GitHub 런처 ZIP, `playtest`는 기존 인증 게임 API다. 종료한 `/downloads` 포털을 되살리거나 기존 API 도메인을 삭제하지 않는다.
- 오류 조사: [프로젝트 장애 대응](docs/TROUBLESHOOTING.ko.md)에서 같은 증상을 먼저 찾는다.
- 환경·CLI·권한·도구 문제는 `C:/Users/c/.codex/agent-troubleshooting.md`의 관련 항목을 먼저 읽고, 다른 프로젝트에 재사용할 수 있는 검증된 진단을 같은 파일에 누적한다. 프로젝트 기능 버그는 프로젝트 문서에 기록한다.
- 이 PC의 인수인계가 필요하면 무시된 로컬 파일 `.local/OPERATIONS_HANDOFF.ko.md`를 읽는다. 다른 PC에는 없을 수 있다. 실제 상태는 저장된 경로와 상태 파일을 다시 확인하며, 과거 PID나 CI run ID를 현재 값으로 가정하지 않는다.
- Windows Python으로 문서/JSON을 다룰 때 `read_text(encoding='utf-8-sig')`, `write_text(..., encoding='utf-8')`를 명시한다. 기본 cp949로 한글 UTF-8 파일을 읽는 오류를 반복하지 않는다. 여러 파일 편집 중 실패하면 이미 저장된 diff부터 확인한다.

## 유지할 경계

- 공개 저장소/Release는 런처·의존성·라이선스만 포함한다. Unity 게임, BGM, `.local/`, 인증/감사 기록, 토큰, 서명 개인키는 공개하거나 공개 CI로 보내지 않는다.
- 게임은 전달받은 `Builds/latest.json` / `latest-macos.json`과 정본 ZIP을 검증해 비공개 발행한다. 이 프로젝트에서 Unity 소스를 수정·재빌드·재압축하지 않는다.
- 게임 업데이트는 자동 확인·알림 후 사용자가 명시적으로 적용한다. 런처 자체 업데이트는 검증 후 다음 실행에 적용한다. 두 동작을 혼동하지 않는다.
- `dist/`가 실제 사용자 설치/고정 바로가기의 대상일 수 있다. 새 패키지는 `package-windows.ps1 -OutputRoot`로 별도 출력한다. 기존 설치 루트나 버전 폴더를 일괄 이동·삭제·덮어쓰지 않는다.
- 바로가기는 안정된 `TeamHJDLauncher.exe` / `TeamHJDLauncher.app`을 가리킨다. 개발용/버전별 `TeamHJDClient`를 직접 고정하지 않는다.

## 릴리즈 완료 조건

- Windows 빌드/필수 회귀 검사와 ARM·Intel의 실제 macOS 검증을 구분한다. Windows에서 ZIP 구조를 확인한 것만으로 Mac 실행 성공을 주장하지 않는다.
- `macOS-native-*`는 원본 빌드, `macOS-candidate-*`는 감사 통과 후보, `macOS-arm64` / `macOS-x64`는 delivery 검사까지 통과한 최종 산출물이다. 후보나 빌드 성공만으로 공개하지 않는다.
- Mac 바이너리 재사용은 `validate-native-macos-reuse.py`의 검증을 통과할 때만 허용한다. 버전/C++/QML/assets/vendor/C++ 테스트 변경 또는 허용되지 않은 파일 변경이면 전체 네이티브 빌드를 다시 한다. 검증 조건을 느슨하게 바꾸어 재사용하지 않는다.
- 새 버전은 초안에 명시적인 파일 목록으로 올리고, 실제 GitHub 크기·digest를 로컬 검증본과 대조한 뒤 공개한다. 전체 40자리 커밋 SHA를 사용한다. 기존 공개 태그/자산은 덮어쓰지 않는다.
- 공개 후 실제 latest API와 기존 클라이언트의 다운로드·검증·queue를 확인한다. 격리 테스트의 비교 버전 override와 실제 사용자 설치의 활성화를 구분해 기록한다.
- 새 검증 기록 `docs/RELEASE_VALIDATION_<version>.md`에 환경, 원인/확실도, 수정, 성공 증거와 남은 미검증 범위를 남긴다. 원본 로그·인증 정보·개인 이메일·서명 다운로드 URL을 붙이지 않는다.

## 운영 조사

현재 기본 운영은 Cloudflare Access/Tunnel → `127.0.0.1:8443`이다. 직접 포트포워딩 안내를 이 구성에 적용하지 않는다. 프로세스 재사용/종료는 PID뿐 아니라 시작 시각·실행 파일·정확한 인수를 검증한다. 자동 시작은 Windows 사용자 로그인 이후이며 절전/인터넷 단절 중에는 서비스되지 않는다.

보안 검증·서명·해시·health ACK·회귀 assertion을 끄는 방식으로 장애를 숨기지 않는다. 아직 원인이 확인되지 않은 문제는 미확정으로 남기고, 실제로 하지 않은 Mac 게임 플레이·이메일 인증·재부팅 검증을 통과로 쓰지 않는다.
