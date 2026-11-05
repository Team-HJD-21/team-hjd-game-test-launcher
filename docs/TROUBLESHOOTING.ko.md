# 프로젝트 장애 대응 기록

## 2026-11-05 — 기기 회수·인증 창 복귀와 후보 검사

추가 진단run42013676617의Python3.13.17/ARM Mac 스레드가HTTPServer.server_bind→socket.getfqdn('127.0.0.1')→gethostbyaddr에서20초 이상 머문 것을 확인했습니다. 익명 요청이나 기기 서명 검증 전의 fixture 준비 지연입니다. 해당 단독 loopback fixture의 서버 생성 동안만 reverse-DNS 결과를localhost로 고정하고 원래5초 준비 상한으로 되돌렸습니다. 제품 DNS/TLS·JWT·서명 검증은 변경하지 않았습니다. 실제품 화면의 남아 있던 오프라인 플레이 안내2곳도 접속 필수 정책에 맞췄습니다.

새 Mac runner의 기기 HTTP 단독 검사에서 저장소의 무시된 `.local/` 부모가 없어FileNotFoundError로 준비 단계가 실패했습니다(run42012488965). 기기 키/QML 검사는 그 전에 통과했으나 이 실패를 전체 인증 성공으로 처리하지 않습니다. 격리 fixture가 자신이 쓸 부모 경로를mkdir(parents=True)로 생성하도록 수정하고 전체 native 검사를 다시 합니다. Windows 부트스트랩 fixture도 새 import 검색 보호에 맞춰 공식Qt를 실행 파일 옆에 배치하며 PATH 의존을 제거했습니다. 제품 인증·보호 조건은 바꾸지 않습니다.

0.2.3 후보의 운영자 회수 CLI는32자 URL-safe 등록ID를 사용합니다. ID가`-`로 시작해도 옵션으로 해석되지 않도록 Python 인수는 단일 `--device-id=<ID>`로 전달합니다. 해당 형태의 실제 CLI 미리보기·회수·다음 요청 차단 회귀를 통과했습니다. 새 경로의 저장 장애503/nonce 만료409/회수401을 구분하며, 이미 회수된 기기의 로그아웃도 로컬 키·동의를 정리합니다.

인증 후 창 복귀에서Qt의show()가 최대화 상태를 초기화한 증상을 재현했습니다. 최소화 상태만 복원하고setVisible(true)·활성화 요청을 사용합니다. 콜백 수명은WindowFrame의QObject context에 묶고 OS 포커스 거절 시 기본 주의 표시를 사용합니다. 실제품 QML·최소화/최대화 회귀를 통과했으며 사람 OTP 검증과는 구분합니다.

Windows 최종 검사 중 수동 게임 업데이트의 재적용에서 원자 저장 오류가1회 발생했습니다. 같은 소스의 이전 여러 검사에서는 통과했고, 실패한 로그는 보존했습니다. 구체 파일/OS 오류가 없던 메시지에 basename·Qt errorString을 추가한 뒤 최종11/11 검사는 통과했습니다. 일시 파일 잠금·백신 간섭 등 원인은 미확정입니다. 직접 덮어쓰기 fallback이나 검증 생략·원인 없는 반복 retry를 추가하지 않았습니다. 재발 시 새 진단 정보와 해당 격리 profile을 함께 조사합니다.

기록 갱신: **2026-10-31 KST**. Windows 기준 환경은 Windows 11 build 26200, PowerShell 7.6.5, Qt 6.8.3 MSVC x64와 Visual Studio 2022 C++ 도구다. 맥 네이티브 검증은 GitHub macOS 15 Apple Silicon·Intel, AppleClang 17.0.0.17000013, Qt 6.8.3에서 수행했다. 아래 이전 Windows 사건은 2026-10-29에 관측한 결과도 포함한다.

이 문서는 런처·배포 구성과 검사 코드의 원인을 기록한다. 환경·CLI·권한·도구 문제의 재사용 진단은 먼저 운영 PC의 [공통 에이전트 장애 기록](C:/Users/c/.codex/agent-troubleshooting.md)을 읽는다. 같은 원인을 확인 없이 재시도하거나 전역 인증·PATH·샌드박스·TLS 설정을 바꿔 숨기지 않는다.

새 릴리즈의 순서는 [릴리즈 절차](RELEASING.ko.md), 실제 서버 조작은 [운영 안내](PLAYTEST_HOSTING.ko.md), 인증 경계는 [Cloudflare 안내](CLOUDFLARE_ACCESS.ko.md)를 따른다. 통과 범위는 [0.1.2 검증](RELEASE_VALIDATION_0.1.2.md)과 [0.1.3 검증](RELEASE_VALIDATION_0.1.3.md)을 대조한다. 아래 명령의 예시 경로·run ID는 현재 점검 대상에 맞춰 지정한다. `.local/`의 과거 증거는 재실행 도구나 최신 산출물로 취급하지 않는다.

## 1. 고정 아이콘으로 실행하면 Qt DLL이 없다고 나옴

**증상:** 작업표시줄 아이콘 실행 시 `Qt6QuickControls2.dll` 누락 오류가 났다.

**최소 확인:** 해당 앱의 바로가기 대상과 전체 배포 폴더의 DLL을 확인한다. 아래 조회는 바로가기를 저장하거나 변경하지 않는다.

```powershell
$shell = New-Object -ComObject WScript.Shell
Get-ChildItem -LiteralPath "$env:APPDATA/Microsoft/Internet Explorer/Quick Launch/User Pinned/TaskBar" -Filter '*TeamHJD*.lnk' |
    ForEach-Object { $link = $shell.CreateShortcut($_.FullName); [pscustomobject]@{ Link=$_.Name; Target=$link.TargetPath; Arguments=$link.Arguments } }
```

**원인·확실도:** 실제 고정 대상은 개발 출력인 `build/TeamHJDClient.exe`였다. 이 폴더에 직접 의존 Qt DLL 6개가 없었고, 공식 배포 ZIP에는 들어 있었다. 해당 바로가기의 원인은 확인됐다. 다른 DLL 오류의 원인을 모두 같다고 단정하지 않는다.

**실제 수정:** 바로가기를 비공개 폴더에 백업하고 전체 ZIP을 푼 폴더의 안정된 `TeamHJDLauncher.exe`를 대상으로 바꿨다. 버전별 client EXE를 직접 고정하지 않는다. SDK 경로를 전역 PATH에 넣거나 DLL을 임의로 여러 폴더에 복사하지 않았다.

**성공 증거:** 새 폴더의 공식 배포본이 개발 Qt 없는 Windows/System32 PATH에서 정상 종료했고, 수정한 바로가기의 대상이 bootstrap과 일치했다. 제품 코드나 ZIP을 바꿀 필요가 없었던 사건이다.

**범위·한계:** 다운로드 손상·백신 격리·사용자 파일 삭제는 별도로 확인한다. 프로젝트 최상위 `dist/`는 실제 고정 바로가기가 가리키는 설치 경로일 수 있으므로 새 포장 작업을 위해 덮어쓰거나 이동하지 않는다. 새 산출물은 `package-windows.ps1 -OutputRoot`로 다른 폴더에 만든다.

## 2. Windows 최대화 버튼을 누르면 종료됨

**증상:** 네이티브 최대화 클릭에서 접근 위반 `0xc0000005`가 재현됐다.

**최소 확인:** 빌드용 Qt 환경을 준비한 테스트 셸에서 다음 회귀 검사와 callback 처리를 대조한다.

```powershell
ctest --test-dir build -R gui_window_frame --output-on-failure
rg -n 'if\(result\)|if\(!result\)|invokeMethod' src/client/WindowFrame.cpp
```

**원인·확실도:** Qt 6.8.3은 큐에 들어온 `windows_generic_MSG`에 null `result`를 전달할 수 있다. 기존 callback은 이를 무조건 역참조했다. Qt Windows context는 해당 입력을 전역 filter에 다시 전달하지 않으므로, null이면 모든 입력을 무시하는 수정은 충돌만 막고 최대화 동작을 잃는다. 런처 callback 오류로 확인했으며 Qt 자체 결함으로 기록하지 않는다.

**실제 수정:** 최대화 입력은 null 결과 포인터에서도 소비하고, 결과가 있을 때만 값을 쓴다. geometry 질의에는 유효한 결과 포인터를 요구한다. Qt 최대화·복원 상태 변경은 callback 안에서 즉시 실행하지 않고 큐로 넘겼다. 창 수명과 중복 예약도 보호했다.

**성공 증거:** null/non-null 입력 회귀와 10회 최대화·복원 검사가 통과했다. 실제 앱 클릭 2회에서도 1000×700 ↔ 2560×1392 전환과 프로세스 생존을 확인했다. 최신 Windows 0.1.3 CTest 4/4에도 포함된다. 근거와 고정 Qt 소스 링크는 [0.1.2 검증 기록](RELEASE_VALIDATION_0.1.2.md)에 있다.

**범위·한계:** visibility나 `IsZoomed` 값 하나만으로 최대화를 판정하지 않는다. Qt geometry와 실제 Win32 client bounds, 모니터 작업 영역, 복원 크기를 확인한다. 다른 충돌은 새 로그·실행 파일 지문으로 구분한다.

## 3. Mac 빌드에서 `explicit_bzero`가 선언되지 않음

**증상:** Apple SDK 컴파일에서 `Core.cpp`, `CloudflareLogin.cpp`의 `explicit_bzero`가 미선언 식별자로 거절됐다. `string.h`를 포함한 상태였다.

**최소 확인:** 네이티브 빌드의 최초 오류와 실제 플랫폼 분기를 함께 읽는다.

```powershell
rg -n 'explicit_bzero|volatile char|SecureZeroMemory' src/core/Core.cpp src/core/CloudflareLogin.cpp
```

**원인·확실도:** 해당 macOS 15/AppleClang 17 SDK에서 사용한 심벌의 선언을 제공하지 않는 것은 컴파일 오류로 확인했다. BSD 계열 전체가 같은 함수를 제공한다고 추정한 것이 이식 실수였다.

**실제 수정:** macOS의 민감한 임시 바이트만 `volatile char*`를 통한 명시적 덮어쓰기로 바꿨다. Windows의 `SecureZeroMemory` 경로는 유지했다. 외부 라이브러리를 추가하거나 시스템 SDK를 바꾸지 않았다.

**성공 증거:** 수정 후 양쪽 Mac 아키텍처에서 빌드·core·GUI 시작 단계가 통과했다. 최종 네이티브 포장·전달 검증도 뒤의 성공 run에 포함된다.

**범위·한계:** 이 결과는 컴파일과 코드의 명시적 버퍼 정리 근거다. Qt·시스템 내부 복사본, 스왑까지 모두 지운다는 보장은 아니다. 도구/API 이식의 상세 기록은 공통 에이전트 장애 기록에 있다.

## 4. Mac 포장 후 SDK·Homebrew 경로 또는 framework 링크가 남음

**증상:** 소스 빌드는 성공했지만 배포본 검사에서 SDK의 절대 dylib 참조가 남았다. 이후 optional SQL ODBC 플러그인이 제작 환경의 `/usr/local/opt/libiodbc/lib/libiodbc.2.dylib`에 의존하는 것도 식별됐다.

**최소 확인:** macOS에서 실제 포장본의 `otool -L`, `otool -D`, `otool -l`, 두 앱의 `Contents/Resources/qt.conf`를 읽는다. 최종 판정은 포장 감사 도구로 한다.

```bash
python tools/verify-macos.py --help
otool -L "/검증할/TeamHJDClient.app/Contents/MacOS/TeamHJDClient"
otool -l "/검증할/TeamHJDClient.app/Contents/MacOS/TeamHJDClient"
codesign --verify --deep --strict "/검증할/TeamHJDLauncher.app"
```

**원인·확실도:** framework 본체만 고쳐도 QML 플러그인의 `LC_ID_DYLIB`, 다른 소비자의 load command, `LC_RPATH`에는 SDK 경로가 남을 수 있다. 정확한 ODBC 외부 의존도 네이티브 감사로 확인했다. 상대 app 경로와 절대 경로의 포함 관계 비교도 반드시 같은 기준이어야 한다.

**실제 수정:** `tools/package-macos.py`에서 app 경로를 `resolve(strict=True)`로 확정한다. 링크를 허용하지 않는 ZIP 설치 정책을 유지하면서 Qt framework를 일반 dylib로 배포하고, 모든 dylib ID·소비자 의존성·rpath를 다시 쓴 뒤 서명한다. 두 앱에 app-relative `qt.conf`를 넣는다. JSON 저장을 사용하는 런처에 불필요한 비 SQLite SQL driver는 제외한다. bootstrap에는 client의 `-qmldir`를 전달하지 않는다. 공식 cloudflared는 고정 해시가 변하지 않도록 임의 재서명하지 않는다.

**성공 증거:** 최종 run `41107822889`에서 ARM·Intel 모두 각 145개 Mach-O의 아키텍처·서명·의존성·검색 경로를 통과했다. 1,642개 payload 파일이 seed와 일치하고 네이티브 압축 해제·GUI 실행까지 성공했다.

**범위·한계:** SDK가 설치된 runner에서 실행됐다는 사실만으로 독립 배포를 증명하지 않는다. `--structural-only` 검사도 네이티브 서명·로더 검사를 대신하지 못한다. 검사에서 외부 의존성을 허용하거나 ZIP 링크 거부를 풀어 통과시키지 않는다. 세부 절차는 [릴리즈 절차](RELEASING.ko.md)와 [0.1.3 검증](RELEASE_VALIDATION_0.1.3.md)을 따른다.

## 5. 첫 실행이 키체인 대기 또는 중단된 seed 복사 때문에 막힐 수 있음

**증상:** ad hoc 서명된 새 client는 키체인 접근 동의를 요구할 수 있다. 이를 첫 화면 전에 읽으면 bootstrap의 약 20초 health 대기와 충돌한다. 또 고정된 seed 폴더에 직접 복사하다 중단되면 다음 실행도 같은 불완전 폴더에서 막히는 경로가 있었다.

**최소 확인:** 인증 내용을 출력하지 말고 실행 순서와 복사 대상 생성 코드를 확인한다.

```powershell
rg -n 'frameSwapped|health|restoreCredentials' src/client/main.cpp src/client/Controller.cpp
rg -n 'seed-|verifyTree|activate' src/bootstrap/main.cpp
```

**원인·확실도:** 동기 키체인 조회가 health ACK 전에 있다는 코드 경로와 고정 seed 폴더의 재시도 불능은 확인했다. 실제 사람이 새 코드의 키체인 동의 창에서 오래 대기하는 재현은 수행하지 않았다. legacy Keychain의 UI 억제 상수만으로 이 경로가 해결된다고 간주하지 않았다.

**실제 수정:** Mac 자격 증명 복원은 첫 frame/health ACK 뒤에 예약한다. 초기 seed는 시도마다 UUID가 붙은 독립 디렉터리에 복사하고, 파일 해시·실행권한 검증 후 활성화한다. 이전 불완전 복사본은 증거로 남길 수 있으며, 다음 시도를 막지 않는다. 키체인 접근 권한을 넓히거나 프로세스·OS의 인증 정책을 바꾸지 않았다.

**성공 증거:** 최종 양쪽 Mac delivery에서 일부러 남긴 불완전 seed와 별개로 최초 복사·ACK·정상 실행이 통과했다. queued payload 활성화와 실패 시 복원도 통과했다.

**범위·한계:** 실제 Mac 사용자의 최초 OTP·키체인 동의 UX는 별도 확인 대상이다. UUID 폴더가 남았다는 이유로 `LauncherRuntime-*` 전체나 키체인 항목을 지우지 않는다. 정상 사용 중인 버전과 상태 파일을 보존한다.

## 6. Mac rollback 검사가 실패했지만 후보 fixture가 정상 응답한 경우

**증상:** 실패 후보를 넣은 delivery 검사에서 이전 버전으로 돌아오지 않는 assertion이 났다.

**최소 확인:** `tests/MacFixtureGame.cpp`의 `no-health.flag` 조건과 `tools/test-macos-delivery.py`의 선검사를 확인한다. 실패 fixture를 독립 실행했을 때 **exit 4, ACK 파일 없음**을 먼저 증명해야 한다.

```powershell
rg -n 'no-health.flag|return 4' tests/MacFixtureGame.cpp
rg -n 'prepare_failed_health_fixture|returncode == 4|probe.exists|failedNativeHealthRollback' tools/test-macos-delivery.py
```

**원인·확실도:** 처음 시험은 실패 분기를 켜는 flag를 빠뜨렸다. fixture가 health ACK를 써서 bootstrap이 정상 후보로 인정한 것은 시험 구성 실수로 확인됐다. 이 assertion만으로 제품 rollback 결함이라고 결론 내리지 않는다.

**실제 수정:** `no-health.flag`를 만들고 그 파일도 receipt의 크기·SHA-256·비실행 플래그에 포함했다. 대체 실행 파일의 receipt·실행권한도 갱신한다. 독립 선검사에서 exit 4/no ACK를 확인한 뒤 원래 rollback assertion을 그대로 실행한다. 실패 assertion을 삭제하거나 성공 후보를 실패한 것으로 꾸미지 않았다.

**성공 증거:** [run 41107822889](https://github.com/Team-HJD-21/team-hjd-game-test-launcher/actions/runs/41107822889)의 ARM·Intel 최종 delivery 모두 deliberate exit/no ACK와 이전 활성 버전 복원을 통과했다.

**범위·한계:** health 복원과 hash 손상 거부는 다른 시험이다. 이번 후보는 파일 receipt가 맞는 상태에서 시작 확인을 실패하도록 만든 합성 native process다. Unity 게임을 실패시키거나 사용자 앱을 강제 종료하는 시험이 아니다.

## 7. 게임을 릴리즈했는데 런처에 새 버전이 안 보임

**증상:** 게임 BUILD0024가 준비됐어도 운영 카탈로그는 이전 게임 버전을 가리킨 적이 있었다.

**최소 확인:** GitHub 런처 릴리즈와 비공개 게임 카탈로그를 따로 확인한다. 파일명 추측 대신 게임 프로젝트가 제공한 정확한 pointer를 검사한다.

```powershell
./tools/Publish-GameArchive.ps1 -LatestPointer 'D:/Game/Builds/latest.json' -PrivateRoot 'D:/Private/TeamHJDDistribution' -VerifyOnly
./tools/Publish-GameArchive.ps1 -LatestPointer 'D:/Game/Builds/latest-macos.json' -PrivateRoot 'D:/Private/TeamHJDDistribution' -VerifyOnly
./tools/Open-HostingDashboard.ps1 -PrivateRoot 'D:/Private/TeamHJDDistribution' -GenerateOnly
```

**원인·확실도:** GitHub Releases는 런처 자체 업데이트 원본이다. 비공개 게임 버전은 별도 서명 카탈로그의 pointer·sequence·유효기간과 인증 상태에 따라 보인다. 이 사건에서는 Windows 게임 pointer와 운영 카탈로그의 불일치를 확인했다. 단순 대기 시간 문제라고 설명하지 않는다.

**실제 수정:** 정본 ZIP의 크기·SHA-256·전체 파일을 확인해 그대로 비공개 게시했다. Mac은 별도 pointer에서 읽어 기존 Windows 항목 아래 `platforms`로 게시한다. Windows 원본 필드와 패키지를 보존하며, 만료 갱신도 같은 게시 잠금으로 처리한다. UI는 자동 버전 확인과 명시적인 게임 **업데이트** 버튼을 분리했다.

**성공 증거:** 현재 양쪽 게임은 Alpha24 / BUILD0024 / HFH-FIELD-v14다. 공개 인증 경로로 서명된 카탈로그와 Mac ZIP 전체 SHA-256, Windows ZIP 범위 바이트를 확인했다. 실제 v0.1.2 파서도 Mac variant를 무시하고 기존 Windows Alpha24 304파일을 읽었다. Controller의 수동 업데이트 회귀 11개가 통과했다.

**범위·한계:** 설치된 게임은 버튼을 누르기 전 그대로 유지된다. 런처 자체는 새 payload를 준비한 뒤 **다음 실행**에 적용한다. 서명 거부·만료·인증 실패를 해결하려고 sequence를 낮추거나 캐시·키를 무조건 지우지 않는다. 실제 사용자 v0.1.2 profile의 v0.1.3 업그레이드 완료는 별도 관측 전까지 주장하지 않는다. [운영 안내](PLAYTEST_HOSTING.ko.md)의 플랫폼 게시 규칙을 따른다.

## 8. 운영 서버가 꺼진 것처럼 보임

**증상:** 외부 다운로드가 되지 않거나 운영 대시보드 갱신이 멈춘다.

**최소 확인:** 운영 Windows 사용자의 로그인, PC가 깨어 있음, 인터넷 연결을 먼저 확인한다. 이어 저장된 origin·tunnel·monitor의 상태와 실제 PID·시작 시각·실행 파일·정확한 인수를 대조한다. 명령줄 전체나 인증 파일을 출력하지 않는다.

```powershell
./tools/Enable-HostingAutostart.ps1 -PrivateRoot 'D:/Private/TeamHJDDistribution' -Action Status
./tools/Start-PlaytestHosting.ps1 -PrivateRoot 'D:/Private/TeamHJDDistribution' -ValidateOnly
./tools/Open-HostingDashboard.ps1 -PrivateRoot 'D:/Private/TeamHJDDistribution' -GenerateOnly
curl.exe --head --max-time 10 https://playtest.teamhjd.com/catalog.json
```

**원인·확실도:** 자동 시작은 현재 사용자 logon의 HKCU Run 등록이며, 부팅 직후 로그인 전 서비스가 아니다. 절전·꺼짐·인터넷 단절 중에는 배포를 제공할 수 없다. **이번 부팅에서 자동 시작이 누락된 정확한 원인은 미확정**이다. 상태 파일의 오래된 PID만으로 자동 시작 원인을 단정하지 않는다.

**실제 수정:** 기존 운영 도구로 소유권을 검증한 origin·tunnel·monitor를 다시 시작하거나 재사용했다. 감시는 30초마다 종료된 구성 요소를 복구한다. portal 코드 적용 시에도 hosting 잠금과 소유권 대조 후 해당 origin만 교체하고 터널·감시를 유지했다.

**성공 증거:** 복구 뒤 세 프로세스의 PID·시작 시각이 저장 상태와 일치했고, 대시보드 갱신과 공개 HTTPS 인증 게이트 응답을 확인했다. 실제 인증 후 portal HTTP200도 별도로 확인했다.

**범위·한계:** `-ValidateOnly`나 프로세스 생존은 외부 배포 성공 증거가 아니다. 익명 302도 인증 후 다운로드 성공과 구분한다. 확인되지 않은 PID를 종료하거나 port를 빼앗지 않는다. 상태·DPAPI 키·터널 자격 증명을 지워 초기화하지 않는다. 전체 재부팅 뒤 자동 시작은 아직 완료된 검증이 아니다. 재시작은 [운영 안내](PLAYTEST_HOSTING.ko.md)의 기존 진입점을 사용한다.

## 9. 인증 메일을 입력했는데 접속자 목록에 없음

**증상:** Cloudflare에서 OTP를 마쳤지만 운영 목록에 사용자가 없거나 예상 시각과 다르게 보인다.

**최소 확인:** 원본 JSON·세션 해시 대신 운영 보고 도구나 비공개 대시보드를 연다.

```powershell
./tools/Get-VerifiedTesters.ps1 -PrivateRoot 'D:/Private/TeamHJDDistribution'
./tools/Open-HostingDashboard.ps1 -PrivateRoot 'D:/Private/TeamHJDDistribution'
```

**원인·확실도:** 이메일은 OTP 화면 입력 시점이 아니라 검증된 JWT로 **실제 origin 요청이 도착한 뒤** 기록된다. 브라우저 인증 완료와 origin 도착은 다른 단계다. 보고 도구의 원본 시각은 UTC, 대시보드 기본 표시는 KST이며 UTC도 함께 표시한다.

**실제 수정:** 인증 기록 조회 경로와 시각 의미를 문서화하고 보고 도구·대시보드에 선택된 필드만 표시했다. 이메일/최초·최근 인증/최근 접속/세션 수를 보며 JWT·IP·내부 세션 해시는 내보내지 않는다. CSV는 명시적 export 때만 만들고 비공개 권한을 유지한다.

**성공 증거:** 인증 감사 합성 검사와 private dashboard 2/2, 실제 인증 후 운영 기록 도착을 확인했다. 자동 생성은 감시 30초, 브라우저 HTML 갱신은 15초다.

**범위·한계:** 기록이 아직 없는 것을 Cloudflare 인증 실패로 단정하지 않는다. 대시보드 HTML·CSV·이메일 목록을 공개 저장소나 공개 웹 경로에 넣지 않는다. file URL의 브라우저 자동 관찰 정책 거부를 다른 표면·프록시로 우회하지 않는다. 원본 요청 검증과 기록 경계는 [Cloudflare 안내](CLOUDFLARE_ACCESS.ko.md)에 있다.

## 10. Actions API는 실행 중인데 로그가 끝났거나 `steps=[]`·로그 404가 나옴

**증상:** run/jobs API의 502·504, 빈 steps, 진행 중 로그의 404, 완료 로그와 원격 상태의 일시적 불일치가 관측됐다.

**최소 확인:** 현재 점검 run/job을 지정한 공식 상태 조회와 이미 내려받은 전체 완료 로그·실제 artifact를 대조한다. 예전 job ID를 새 실행에 재사용하지 않는다.

```powershell
$runId = '현재 점검할 run ID'
gh run view $runId --repo Team-HJD-21/team-hjd-game-test-launcher --json status,conclusion,jobs
# 완료 로그를 받되 ANSI가 있는 내용을 터미널에 바로 출력하지 않는다.
$logDirectory = Join-Path $PWD ('.local/action-logs-' + (Get-Date -Format yyyyMMdd-HHmmss))
New-Item -ItemType Directory -Path $logDirectory -ErrorAction Stop | Out-Null
gh api "repos/Team-HJD-21/team-hjd-game-test-launcher/actions/runs/$runId/logs" `
  --allow-escape-sequences > (Join-Path $logDirectory 'workflow-logs.zip')
```

전체 로그 API도 진행 중에는 404일 수 있다. 다운로드 종료 코드와 ZIP 무결성을 확인하고 ANSI 제어 문자를 제거한 필요한 실패 행만 읽는다. 인증/환경 전체나 원본 로그를 공개 문서에 붙이지 않는다. PowerShell 예시는 바이너리 stdout 리다이렉트를 보존하는 현재 7.5 이상 기준이다.

**원인·확실도:** 원격 조회가 실패하거나 부분 정보만 반환한 사실은 확인했다. 빈 응답만으로 runner 중단, 성공, GitHub 전체 장애를 확정하지 않았다. 로그 전송·ANSI 처리 문제는 공통 에이전트 장애 기록을 따른다.

**실제 수정:** API 결과 하나에 의존하지 않고 완료된 작업 로그를 보존해 실제 실패 행을 읽었다. 패키저에는 단계별 flush 출력·명령 timeout을 추가했다. 포장만 다시 할 때는 성공한 원본 build job, 같은 저장소 artifact provenance, 동일한 native source tree를 검증한 뒤 재사용했다. 검사·assertion을 건너뛰지 않았다.

**성공 증거:** 최종 [run 41107822889](https://github.com/Team-HJD-21/team-hjd-game-test-launcher/actions/runs/41107822889)의 양쪽 완료 결과와 실제 delivery artifact를 확보했다. 원본 네이티브 build는 run `41103462194`이며, 재사용한 두 source tree의 동일성 근거는 [0.1.3 검증](RELEASE_VALIDATION_0.1.3.md)에 있다.

**범위·한계:** 오래된 로그에 PASS가 있다는 것과 현재 배포 파일이 검사됐다는 것은 다르다. 전체 파일 해시·native delivery 결과를 연결할 수 없으면 릴리즈 통과로 표시하지 않는다. 상세 도구 문제는 공통 기록에 남기고 토큰·환경 전체·인증 파일을 출력하지 않는다.

## 11. 로컬 사설 CA의 Python strict·Schannel 검증 오류

**증상:** Python 3.13 기본 strict TLS에서 `Missing Authority Key Identifier`, Windows curl/Schannel에서 폐기 상태를 확인할 수 없다는 오류가 났다.

**최소 확인:** 인증 정보를 붙이지 않은 로컬 TLS 요청과 공개 Cloudflare HTTPS 요청을 구분한다. 개인 CA 경로는 운영 폴더에 맞춘다.

```powershell
curl.exe --cacert 'D:/Private/TeamHJDDistribution/tls/ca-cert.pem' --head https://localhost:8443/downloads
curl.exe --head --max-time 10 https://playtest.teamhjd.com/catalog.json
```

**원인·확실도:** Python strict 검증이 기존 사설 인증서의 AKI 조건을 만족하지 못한 것은 예외로 확인했다. Schannel 폐기 조회의 세부 실패 원인은 미확정이며 두 오류를 같은 원인으로 묶지 않는다.

**실제 수정:** 이 인증서 확장 보완·재발급은 아직 수행하지 않았다. strict, hostname, certificate-chain, revocation 검사를 끄거나 전역 CA 저장소를 바꾸지 않았다. 따라서 이 항목은 **미해결 진단**이다.

**성공 증거:** 별개 경로인 공개 Cloudflare HTTPS는 정상 TLS 검증으로 익명 302와 인증 후 200을 확인했다. 운영 portal의 서명·패키지 hash 및 합성 서버 검사 20개도 통과했다. 비공개 증거 `.local/portal-production-proof.json`은 이 경계를 구분한다.

**범위·한계:** 공개 TLS 성공은 로컬 strict CA 진단 해결을 뜻하지 않는다. 로컬 요청도 TLS 뒤의 익명 401까지 도착했다고 기록하지 않는다. `-k`, `verify=False`, 인증서 callback의 무조건 승인, 폐기/strict 플래그 해제로 통과시키지 않는다. 공통 기록의 2026-10-31 사설 CA 항목을 먼저 읽고 별도 인증서 수정·재검증 작업으로 다룬다.

## 12. 큰 CI artifact 다운로드가 멈춘 듯 매우 느림

**증상:** 공식 `gh run download`가 몇 분 동안 완성 파일을 만들지 않았다. 자기 프로세스의 I/O 누적과 연결은 움직여 완전 정지와는 구분됐다.

**최소 확인:** 현재 artifact metadata의 원본 길이·전체 SHA-256, 자기 수신 작업의 진행량과 종료를 확인한다. 내려받은 최종 파일에는 다음 검사를 적용한다.

```powershell
Get-Item -LiteralPath '검증할-artifact.zip' | Select-Object Length
Get-FileHash -LiteralPath '검증할-artifact.zip' -Algorithm SHA256
```

**원인·확실도:** 느린 전송은 관측했으나 서버·네트워크의 특정 제한을 원인으로 확정하지 않았다. 파일이 아직 없다는 이유만으로 같은 대용량 다운로드를 반복 시작하지 않는다.

**실제 수정:** 공식 artifact API가 제공한 HTTPS 저장소 주소를 메모리에서만 사용한 한정 작업으로 범위 수신했다. 각 요청의 **206·Content-Range·정확한 길이**, 조립한 전체 길이와 metadata SHA-256을 모두 확인했다. GitHub 인증은 API 호스트에만 보내고 외부 저장소에 전달하지 않았다. 서명 URL·토큰을 로그나 문서에 남기지 않았다.

**성공 증거:** 실제 108,415,430바이트 artifact의 전체 SHA-256이 metadata와 일치했고, 허용한 6개 ZIP 항목·CRC 및 후속 포장 검사를 통과했다. 그 뒤 자기 기존 수신 프로세스만 신원을 재확인해 정리했다. 재사용 가능한 세부 진단은 공통 에이전트 장애 기록의 2026-10-31 대용량 artifact 항목에 있다.

**범위·한계:** Range나 원본 digest가 없으면 이 성공 경로를 일반화하지 않는다. HTTP200 전체 응답을 부분 블록으로 잘라 성공으로 처리하거나, 길이만 맞는 파일을 릴리즈에 쓰지 않는다. 당시 `.local/` 수신 스크립트는 turn 한정 도구이며 hardcoded artifact/run ID를 그대로 재사용하지 않는다. 매번 현재 공식 metadata·provenance·전체 지문에서 시작한다.

## 완료 판정을 넓히지 않는 항목

후속 운영 변경으로 기존 수동 다운로드 HTML은 종료했다. `/`와 `/downloads`의 인증 후410은 의도된 응답이며 API 장애나 TLS 실패가 아니다. 익명 edge는 계속 OTP로302 이동한다. 최신 런처 웹 배포와 도메인 구분은 [공개 배포 안내](PUBLIC_DOWNLOADS.ko.md)를 따른다. 위의 portal HTTP200은 종료 전 수집한 역사적 증거다.

2026-10-31 현재 네이티브 Mac CI, Windows 검사, 인증 서버 경로·원본 게임 ZIP 전달의 통과는 각 검증 문서 범위에 한정된다. 사람이 실제 Mac에서 수행하는 이메일 OTP/브라우저 복귀·변경된 앱의 키체인 동의, Unity Metal 화면·오디오·입력, Windows↔Mac 크로스플레이, 기존 실제 v0.1.2 사용자 profile의 v0.1.3 자동 업데이트 완료, 전체 재부팅 후 autostart는 관측 증거 없이 완료로 기록하지 않는다. Mac Developer ID/공증과 Windows Authenticode 서명도 현재 없다.

새 사건은 날짜·OS/도구 버전·증상·최소 확인·원인 확실도·실제 수정·성공 증거·남은 한계를 같은 형식으로 추가한다. 개인정보, 원본 JWT, 인증 파일 내용, DPAPI/서명 키, 서명된 임시 URL은 증거 설명에도 포함하지 않는다.

## 2026-10-31 — Pages 배포 후 FAQ 문구가 비고 스타일이 빠짐

**환경·증상:** Windows11/PowerShell7.6.5, Chrome 연결 브라우저와 GitHub Pages. 브라우저 세부 버전은 미측정. 새 HTML에는 FAQ3개가 있었지만 화면에는 OS 이름·번호만 나오고 질문·답변이 비었다. 캐시 없는 HTTPS 요청의 세 파일은 새 소스와 일치했다.

**최소 확인·원인:** 실제 브라우저에서 `.faq-item` 수, `[data-i18n="faqTitle"]`의 문구, FAQ summary의 계산된 스타일, HTML의 CSS/JS URL을 대조한다. 이전 JS가 새 번역 key를 알지 못해 빈 문자열로 만들고 이전 CSS가 새 class를 꾸미지 못하는 혼합과 증상이 일치했다. 브라우저 캐시와의 연관은 재로딩 대조로 확인했으며 CDN 내부 동작까지 확정하지 않는다.

**수정·검증:** CSS/JS 내용 hash query를 HTML에 붙이고 공개 사이트 검사기가 hash 불일치를 거부하게 했다. `node tools/check-download-site.mjs --update-asset-versions`로 두 링크만 갱신한다. 번역은 key가 없어도 기존 HTML을 비우지 않고 한국어 fallback을 사용한다. 최종 배포 뒤 실제 브라우저에서 FAQ 질문·답변과 computed style까지 확인하며, HTTP200·DOM 항목 수·CI 성공만으로 완료 판정하지 않는다.

**범위·한계:** 정적 웹 자산 캐시 경계의 수정이다. 기존 런처 ZIP·게임·인증 설정과 footer는 변경하지 않는다. HTML 자체가 남은 기존 탭은 새로고침이 필요할 수 있다. `.local/download-site-validation/faq-production.json`은 초기 파일 대조이며 최종 브라우저 표시 증거를 대신하지 않는다.

## 2026-10-31 — 관리 화면에서 중지한 서버가 곧 다시 켜짐을 방지

**환경·증상:** Windows11/PowerShell7.6.5/Python3.13.3, 기존30초 자동 복구 감시. origin만 종료하면 감시가 복구하는 설계이므로 수동 중지와 충돌한다. 이는 감시의 의도된 동작이며 인증·게임 파일 손상이 아니다.

**수정:** `Stop-PlaytestHosting.ps1`에서 시작 잠금과 전체 소유권을 먼저 확인한 후 감시 → origin → 터널을 중지한다. `127.0.0.1:8844` 관리 서비스와 HKCU 로그인 자동 시작은 유지한다. 재시작/자동 시작 등록은 `hosting-launch.json`의 기존 Python·helper·토큰 파일 경로·포트를 보존한다. 관리 API는 정확한 Host·Origin·256bit capability·POST를 요구하고, 개인정보 GET은 HttpOnly/SameSite cookie를 요구한다. Open 도구의 브라우저 인계는60초·1회다.

**성공 증거:** 합성 관리/CSRF/설정 보존/소유 프로세스 중지8개와 기존 대시보드2개 통과. 실제 관리 버튼의 재시작 후 origin·터널·감시는 새 PID로 실행되고 관리 서비스 PID는 유지됐다. 기본 로그인 자동 시작도 유지됐다. 기존 클라이언트 인증으로 외부 catalog200/원본 일치, 게임 Range206/정본4096bytes 일치를 확인했다. 인증정보·개인 이메일은 검증 출력에 넣지 않았다. 상세 기록은 비공개 `.local/control-dashboard-validation/`에 둔다.

**남은 한계:** 브라우저 자동 검수의 재시작 직후 CDP snapshot timeout은 새 탭에서 정상 화면을 확인한 사실과 구분한다. 실제 재부팅·로그온·절전 복구는 이번 작업에서 재현하지 않았다. 최근5분 요청은 현재 온라인 상태나 게임 플레이 수가 아니다. 관리 token/일회용 URL/생성 HTML을 공개하거나 UI 편의를 위해 제어 인증을 제거하지 않는다.

## 2026-11-03 — 관리 화면 통계와 새로고침 상태 구분

**환경·증상:** Windows11/PowerShell7.6.5/Python3.13.3/Chrome 연결 브라우저(세부 Chrome 버전 미측정). 기존 전체15초 새로고침은 검색/목록 탐색을 유지하기 어려웠고 기존 인증 파일은 누적 이메일·마지막 요청·관측 세션만 보관했다. 과거 일별 활성·게임 실행·다운로드 완료는 원본 기록에 없다는 점을 코드로 확인했다.

**수정·원인 확실도:** 기존 레지스트리만 읽어 최초 인증일별 KST 추이·최근 요청 기간·재방문을 집계한다. 잘못된 시각/미래 시각은 기간 집계에서 제외한다. 인증 기록 읽기 실패를0명으로 대체하지 않는다. 고정 hash CSP로 허용한 로컬 JS가 같은 인증 origin의 HTML을30초마다 읽으며 검색/메뉴/페이지를 유지한다. 오류면 이전 테스터 데이터를 유지하고 경고한다. 토큰/Host/Origin/POST/소유권/서명 검증은 유지했다.

**최소 확인·성공 증거:** `python -X utf8 -m unittest discover -s tests -p test_hosting_dashboard.py -v` 4개, 기존 `test_hosting_control.py` 8개 통과. 합성 명단으로25행·다음 쪽·검색/정렬 유지·상세/Escape·CSV·잘못된 registry 후 이전 명단 유지·7/30일 차트와390px/900px 넘침 없음을 확인했다. 실제 운영 화면에서도30초간 생성 시각이 전진하고 같은 이름으로 바꾼 바로가기가4메뉴 화면을 열었다. 근거는 비공개 `.local/dashboard-redesign-validation/`이며 개인 이메일/토큰/일회용 URL은 공개하지 않는다.

**범위·한계:** 새 계측/추적 수집은 추가하지 않았다. 실제 동접·게임 플레이·인증 대기/메일 수는 확인할 수 없다. 새로고침이 성공해도 게임 배포 서버가 실행됐다는 뜻이 아니다. 이번 운영 UI는 세 게임 구성 요소와 로그인 자동 시작이 꺼진 상태를 표시했고 시작/중지는 수행하지 않았다. 부팅/로그인 자동 시작·실제 게임 다운로드를 이번 화면 작업의 합격으로 기록하지 않는다.

## 2026-11-03 — 서버는 켜져 있는데 클라이언트가 자동 재연결로 표시됨

**원인·확실도:** origin·터널·감시의 PID/시각/실행 경로와8443 listener는 일치했고 공개 인증 gate는302였다. 운영자 PC의 저장된 세션은 만료된 것으로 확인했다. 기존0.1.3은 needsLogin으로 catalog 요청을 건너뛰면서 UI에서는 hasServer만 보고 자동 재연결로 표시했다. 실제 인증 만료와 표시 오류를 확인했으며 서버 프로세스 실행만으로 로그인 이후 다운로드 성공을 주장하지 않는다.

**수정·검증:** 인증 필요/서버 확인 중/온라인/오프라인을 구분하고 재인증 모달을 표시한다. 업데이트는 이메일 인증보다 먼저 강제로 한 번 확인하고 별도 모달로 다운로드·검증·다음 실행 적용을 안내한다. 설치된 Client 직접 실행은 receipt/self hash가 일치하는 관리 경로만 찾아 복원한다. 인증/업데이트 확인 실패에서도 설치 게임을 오프라인으로 선택할 수 있다. 만료·모드 전환·실제 합성 게임 실행과22개 connection assertion, Windows5개 및 양쪽 Mac native/delivery 검사를 통과했다. 상세는 v0.1.4 검증 문서에 있다. 세션 만료·토큰·서명 검증을 없애는 수정은 하지 않았다.


## 2026-11-05 — 설치형·서명 피드 전환의 회귀

- Windows11/Qt6.8.3/MSVC2022/NSIS3.13. 제거가unknown사용자파일을보존한뒤ownership marker를지워재설치를차단했던문제를실제TestPackage에서확인했습니다. C++은removed marker를남기고NSIS가등록/제거기정리후폴더가완전히비었을때만지우도록수정했습니다. 실제설치·복구·제거·재설치와합성game/settings/unknown파일보존을재검증했습니다. 전체폴더재귀삭제로우회하지않습니다.
- 정확한PID/생성시각/EXE/Qt인수대조에서Windows와Qt의경로구분자혼용이검사fixture를거절했습니다. exe는GetFullPath, Qt의root/profile은실제전달형태로통일했습니다. 소유권검사자체는유지했습니다. 수정후실제설치GUI의Setup /UPDATE를visible/silent모두통과했습니다. 잠금화면에서의실제픽셀/새frame검증과합성health를혼동하지않습니다.
- 실제Windows bootstrap의정상전환·bad health복원·중단복구·queue, 별도실제C++Installer의합성badhealth→정확이전slot재시작/marker/bootstrap/데이터보존을확인했습니다. 사용중제거·다운그레이드는거부하고손상/상위경로receipt는보존하며외부데이터를따라삭제하지않습니다.

## 2026-11-05 — macOS DMG에서검증JSON한개가누락됨

- 환경: GitHub macos-15 ARM/macOS-15-intel, Qt6.8.3, Python3.13, hdiutil HFS+/UDZO. native코드원본run41933632479 두build성공.
- 증상/최소확인: ZIP/완전한payload roundtrip/codesign은성공했으나최종DMGread-onlymount후tree대조에서 `Contents/Resources/seed/._teamhjd-receipt.json` 한개만missing, extra0/changed0을관측했습니다.
- 원인/확실도: hdiutil -srcfolder가dot-underscore JSON을AppleDouble파일로처리하는경로와일치했으며POSIX복사방식변경후그파일을포함한전체대조가통과해진단을뒷받침했습니다. `._` 파일을검사에서빼는수정은하지않았습니다.
- 수정: 빈writable이미지→Python파일복사→원본receiptbytesassert→unmount→UDZO변환→최종read-onlymount전체bytes/modes·Applications링크·strictsignature검증. blank create에는 -format을지정하지않고convert에서UDZO를지정합니다(실제hdiutil오류로확인).
- 성공증거: 최종run41937605235 ARM/Intelpackage/nativeaudit/DMGmount/delivery모두success. native-source검증기가동일한native source와미만료원본을확인했습니다. 사람이메일OTP/Unity실제플레이/재부팅이나macOS12실기를통과했다는증거는아닙니다.


## 2026-11-05 — 오래된 origin PID가 svchost로 재사용되어 시작이 막힘

저장한Python PID와현재CIM의svchost.exe/다른생성시각을대조했고8443listener가없었습니다. 보호된Windows프로세스의Get-Process.StartTime/Path는null이라기존helper가소유권확인불가로중지했습니다. 시스템프로세스는종료하지않았습니다.

HostingCommon은이미지가다르면접근가능한생성시각또는CIM의일치하는이미지/생성시각으로저장값과다름을증명할때만stale로반환합니다. 정확한시각인데실행파일/인수가다르거나확인불가하면계속거부합니다. 원래보안회귀8개와추가소유권6개를통과했고서버가다시시작됐습니다. 미래의PID를현재값으로고정하거나임의프로세스를kill하지않습니다. 실제재부팅·로그온시험은별도이며최초서버종료원인은미확정입니다.


## 2026-11-05 — Release 이벤트의 Pages 배포가 시작 전에 거부됨

환경은 GitHub Actions/Pages, `github-pages` 배포 허용 브랜치가 main뿐인 구성입니다. v0.2.2의 main push 배포41963945322는 성공했지만 Release 이벤트41963948237은 step 없이 실패했습니다. 최소 확인은 해당 job의 check-run annotations와 `environments/github-pages/deployment-branch-policies` GET입니다. annotation에 `Tag "v0.2.2" is not allowed to deploy to github-pages`가 명시되어 원인은 확정입니다. checkout의 ref만 main으로 바꿔도 이벤트의 배포 ref는 바뀌지 않습니다.

태그 배포를 허용하는 대신 Release 전용 dispatcher에서 main의 Pages 워크플로를 호출하도록 분리했습니다. 후속 main 배포가 실제 완료되는지까지 검사하며, 공개 피드 bytes와 latest 설치 링크도 대조합니다. 미래 Release가 만들어질 태그에는 dispatcher 파일이 포함되어야 합니다. 이미 공개된 immutable 태그를 수정하거나 가짜 Release를 만들어 테스트하지 않습니다.

## 2026-11-05 — 실제 자동 업데이트의 종료된 부모 HANDLE·한글 오류

Windows11/Qt6.11.3/NSIS3.13. 실제0.2.0이0.2.1 설치기를 실행한 뒤 종료되었지만 프로세스 HANDLE이 남았습니다. QueryFullProcessImageName은 ERROR_GEN_FAILURE31, GetProcessTimes 생성 시각은 원본 일치, WaitForSingleObject는 종료 상태였습니다. 기존 liveness용 processStamp의0을 잘못된 요청자로 취급했습니다. controlled Setup만으로는 이 전체 자동 흐름의 성공을 증명하지 못했습니다.

0.2.2는 살아 있는 요청자의 이미지·시각 검사를 유지합니다. 이미 종료된 객체의 이미지 조회가 실패한 경우만 OS 생성 시각, 정확한 bootstrap running 기록의PID/stamp/folder/executable, 활성 슬롯·receipt·전체 파일을 모두 대조합니다. 기록 없음·다른시각·다른이미지·손상 파일은 거부합니다. retained-HANDLE 단위 검사와 실제GUI→Setup 전환을 재현했고 검사·hash·health를 끄지 않았습니다.

한글 helper 오류는 UTF-16LE+BOM으로 전달합니다. CRT text mode가 UTF-16의LF 바이트를CRLF로 늘려55bytes 홀수가 된 것을 원시 출력에서 확인하고, 해당 installer 출력에만 binary mode를 적용했습니다. 최종 배포 helper의54bytes 짝수/BOM/엄격 Unicode/한글 원문·줄바꿈 검사가 통과했습니다. nsExec에 UTF-8을 ANSI로 해석시키거나 깨진 문자열을 정상 메시지로 취급하지 않습니다. 실제 사용자 전환 결과는 해당 릴리즈 검증 문서를 함께 확인합니다.

## 2026-11-05 — 새 버전이 늦게 보이고 승인 없이 업데이트됨

0.2.0/0.2.1의 성공 후 확인 주기는약15분이며 유효한 사이트 피드를 받으면 GitHub 보조 피드를 보지 않았습니다. 새Release 공개와Pages 피드 게시 사이에는 이전 버전으로 표시될 수 있었습니다.0.2.2는약1분/창복귀/시작에 두서명피드를 확인해높은sequence를선택하며 같은sequence·다른내용은거부합니다. 업데이트가있으면available상태에서승인을기다리고별도진행모달을거쳐자동재실행합니다. 서버의온라인최소버전은공개피드검증뒤에만올립니다. 오프라인우회는사용자요청으로제거했습니다. 구형Windows가넘긴최초Setup에도적용전승인페이지를둡니다. 구형바이너리의승인전다운로드동작을소급변경했다고주장하지않습니다.

## 2026-11-05 — Intel Mac macdeployqt 240초 시간 초과

0.2.2 중간run41957825831의Intel native빌드는성공했으나첫macdeployqt호출이240초TimeoutExpired로종료했습니다. Qt내부지연원인은미확정입니다. 검증/기한을유지한채실패job만새runner에서1회재실행해통과했습니다. 이후C++최종수정때에는전체ARM/Intel native를다시빌드했고run41960606630이모두성공했습니다. timeout시captured도구출력64KiB를남겨반복오류진단을돕습니다. 같은실패를원인확인없이반복재시도하거나Mac검증을생략하지않습니다.
