# 프로젝트 장애 대응 기록

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
curl.exe --head --max-time 10 https://playtest.teamhjd.com/downloads
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
curl.exe --head --max-time 10 https://playtest.teamhjd.com/downloads
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

2026-10-31 현재 네이티브 Mac CI, Windows 검사, 인증 서버 경로·원본 게임 ZIP 전달의 통과는 각 검증 문서 범위에 한정된다. 사람이 실제 Mac에서 수행하는 이메일 OTP/브라우저 복귀·변경된 앱의 키체인 동의, Unity Metal 화면·오디오·입력, Windows↔Mac 크로스플레이, 기존 실제 v0.1.2 사용자 profile의 v0.1.3 자동 업데이트 완료, 전체 재부팅 후 autostart는 관측 증거 없이 완료로 기록하지 않는다. Mac Developer ID/공증과 Windows Authenticode 서명도 현재 없다.

새 사건은 날짜·OS/도구 버전·증상·최소 확인·원인 확실도·실제 수정·성공 증거·남은 한계를 같은 형식으로 추가한다. 개인정보, 원본 JWT, 인증 파일 내용, DPAPI/서명 키, 서명된 임시 URL은 증거 설명에도 포함하지 않는다.
