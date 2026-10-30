# 런처 릴리즈와 게임 발행 절차

마지막 절차 대조: **2026-10-31 KST, v0.1.3**. 명령은 저장소 루트의 PowerShell 7.5 이상을 기준으로 한다. 버전, 경로, 커밋과 CI run ID는 매번 실제 대상에 맞춘다. 과거 성공 기록은 [v0.1.3 검증](RELEASE_VALIDATION_0.1.3.md), 증상별 원인과 복구는 [장애 대응](TROUBLESHOOTING.ko.md)에 있다.

## 1. 먼저 배포 대상을 구분한다

| 대상 | 배포 위치 | 적용 방식 |
| --- | --- | --- |
| 런처 client / Qt | 공개 GitHub Release의 플랫폼별 payload | 자동 확인·다운로드·검증 후 다음 실행에 활성화 |
| 안정된 bootstrap | Windows portable 폴더 / macOS의 Applications 앱 | 기존 bootstrap과 호환되지 않는 변경은 종료 후 portable 교체 안내 필요 |
| 게임 | 비공개 HTTPS의 서명된 catalog / hash 이름 ZIP | 자동 확인·알림, 사용자가 **업데이트**를 눌러 적용 |
| 운영 서버 | 이 PC의 origin·Cloudflare Tunnel·감시 | 런처 Release 게시만으로 서버가 재시작되거나 코드가 교체되지는 않음 |

공개 저장소는 `Team-HJD-21/team-hjd-game-test-launcher`다. 런처 버전은 `CMakeLists.txt`의 project VERSION, GitHub 태그는 `v<version>`을 쓴다. 게임 BUILD 번호와 런처 버전은 별개다. 런처의 latest stable 조회 주기는 성공한 확인 후 최대 한 시간이다. 게임 목록은 연결 시 약 2분, 오프라인 재시도는 약 30초이며 선택/앱 재활성화 등에서도 확인한다.

공개 자산에는 게임·음악·Unity 데이터·사용자 로그·이메일 목록·설정·인증·서명 개인키를 넣지 않는다. 공개 CI에는 합성 게임 fixture만 보낸다. 게임 원본을 새로 만들거나 수정해 배포 문제를 우회하지 않는다.

## 2. 릴리즈 작업 디렉터리를 분리한다

다음 값은 **다음 릴리즈를 위한 예시**다. 0.1.4가 자동으로 승인되거나 이미 배포됐다는 의미가 아니다.

```powershell
$releaseVersion = '0.1.4'
$qtRoot = 'C:/Qt/6.8.3/msvc2022_64'
$releaseRun = Join-Path $PWD ('.local/releases/' + $releaseVersion + '-' + (Get-Date -Format yyyyMMdd-HHmmss))
$releaseDist = Join-Path $releaseRun 'dist'
New-Item -ItemType Directory -Path $releaseRun -ErrorAction Stop | Out-Null
git status --short
```

`$qtRoot`는 실제 설치된 **Qt 6.8.3 MSVC x64** 경로로 지정한다. 개인 PC의 다른 프로젝트 SDK 경로에 의존하는 명령을 공개 기본값으로 만들지 않는다. `build/`를 다른 작업자가 사용 중이면 별도 checkout/worktree에서 빌드한다. `build-windows.ps1 -Deploy`는 새 OutputRoot를 전달하지 못하고 기본 `dist/`에 패키징하므로 운영 PC에서는 사용하지 않는다.

**기존 `dist/`를 비우거나 이동하지 않는다.** 이 PC의 사용자 런처와 작업표시줄 고정 대상이 실제로 그 폴더를 썼다. 새 출력에는 `-OutputRoot $releaseDist`를 사용한다. 복구/정리 대상은 작업 전용 절대 경로와 reparse 여부를 확인한다.

버전을 변경한 뒤 client뿐 아니라 bootstrap도 빌드한다. 기존 사용자에게 자동 전달되는 것은 payload이므로, bootstrap의 상태/receipt/실행 경로/health 계약을 바꿨다면 **구형 bootstrap과 새 payload의 호환성**을 검증하고 portable 교체가 필요한지 릴리즈 노트에 쓴다. 별도의 bootstrap 호환 버전을 자동 판정하는 필드는 현재 없다.

## 3. Windows 빌드·검증·패키징

```powershell
./tools/build-windows.ps1 -QtRoot $qtRoot
./tools/verify-bootstrap.ps1 -QtRoot $qtRoot
./tools/package-windows.ps1 -QtRoot $qtRoot -OutputRoot $releaseDist
& ./build/LauncherCoreTests.exe --verify-payload `
  (Join-Path $releaseDist 'TeamHJD-LauncherPayload-Windows-x64.zip') `
  (Join-Path $releaseRun 'payload-check')
if ($LASTEXITCODE -ne 0) { throw 'Final payload verification failed.' }
```

`build-windows.ps1`의 CTest는 core, GUI startup/shutdown, 수동 게임 업데이트 Controller, native window frame의 네 대상을 포함한다. 실행 당시 실제 대상/결과를 기록한다. bootstrap 검사는 정상 활성화, 실패 복원, 중단 복구와 다음 실행 queue를 확인한다. 단순히 프로세스가 보이는 것만으로 최대화/복원을 통과로 판정하지 않는다.

변경 범위가 서버/카탈로그/인증을 포함하면 필요한 Python 합성 검사를 **의존성이 준비된 서버 venv**로 실행한다. 배포·이어받기/실행 중 차단 변경에는 `verify-local.ps1`, UI 변경에는 `verify-ui.ps1`와 실제 메뉴·최대화 검수를 추가한다. CTest가 이미 실행한 같은 검사를 이유 없이 반복하지 않는다.

```powershell
./.deps/server-venv/Scripts/python.exe -m unittest discover -s tests -p 'test_*.py' -v
# HTTPS/서명/이어받기 경로 변경 시:
./tools/verify-local.ps1 -QtRoot $qtRoot
```

새 ZIP을 별도 폴더에 완전히 풀어 실행한다. 개발 Qt가 PATH에 없는 새 프로세스에서도 켜져야 한다. UI 검수에는 격리된 `--data-root`, 실제 배포 바이너리, 캡처와 stderr를 사용한다. `build/TeamHJDClient.exe`가 개발 SDK PATH 덕분에 실행되는 것은 배포 DLL 검사가 아니다. 사용자 인증 프로필이나 설치된 실제 게임을 합성 검사에 사용하지 않는다.

최종 ZIP 생성 후 다시 패키징하면 **다른 산출물**이다. 이전 해시/크기/receipt 검사 결과를 새 ZIP에 옮겨 적지 않는다.

## 4. macOS는 두 실제 Mac runner에서 확인한다

개인 소유 Mac이 없어도 GitHub의 `macos-15` ARM / `macos-15-intel` runner에서 빌드·실행 검사가 가능하다. Windows에서 만든 ZIP 구조 검사로 이를 대체하지 않는다. Apple Developer ID/공증은 현재 구성에 없다.

버전 변경이나 새 네이티브 코드가 있는 릴리즈는 **전체 빌드**를 dispatch한다. 소스를 먼저 검토·커밋·push하고 정확한 head SHA를 기록한다.

```powershell
gh workflow run macos-release.yml --ref main
gh run list --workflow macos-release.yml --branch main --limit 5 `
  --json databaseId,headSha,status,conclusion
# 위 목록에서 의도한 head SHA의 run ID를 선택한다.
$macRunId = '<실제 run ID>'
gh run watch $macRunId --interval 15 --exit-status
```

반드시 `Build and test macOS arm64/x64`와 `Package and verify macOS arm64/x64`의 결과를 구분한다. runner queue나 `steps: []`, 로그 HTTP404만 보고 컴파일 실패/성공을 추정하지 않는다. 자세한 로그 수집은 장애 대응 문서를 따른다.

### 패키징만 다시 실행할 수 있는 조건

앞선 빌드가 성공했고 Python 패키저/검사만 고칠 때에 한해 다음을 사용할 수 있다.

```powershell
$nativeRunId = '<원본 네이티브 빌드 run ID>'
gh workflow run macos-release.yml --ref main -f "native_run_id=$nativeRunId"
gh run list --workflow macos-release.yml --branch main --limit 5 `
  --json databaseId,headSha,status,conclusion
$macRunId = '<방금 생성된 패키징 run ID>'
gh run watch $macRunId --interval 15 --exit-status
```

`$nativeRunId`는 원본 빌드, `$macRunId`는 **새로 실행한 최종 패키징/검사**다. 아래 다운로드에도 갱신한 `$macRunId`를 사용한다. 과거 run의 산출물을 새 검사 결과로 대신하지 않는다.

`validate-native-macos-reuse.py`가 같은 저장소/워크플로/원본 SHA, 두 build job의 성공, 두 미만료 원본 artifact를 검증해야 한다. 원본 run 전체가 패키징 실패여도 build job 두 개가 성공했다면 검토 가능하다. **CMake 버전, src, qml, assets, vendor의 Git tree가 동일**해야 하며 그 밖의 변경도 allowlist를 통과해야 한다. C++ 테스트 변경이나 새 `AGENTS.md`처럼 allowlist 밖 변경도 거절될 수 있다. 거절 시 전체 빌드를 실행하며 검증을 완화하지 않는다. 새 버전으로 올리면 CMake가 달라지므로 구버전 바이너리를 재사용하지 않는다.

이 게이트가 SDK/컴파일러/빌드 플래그까지 완전한 설정 지문으로 검증하지는 않는다. workflow에서 Qt/aqt/컴파일러/빌드 옵션을 바꿨다면 allowlist가 허용해도 **전체 빌드를 다시 한다**. tests 폴더는 전체 Git diff의 확장자 규칙으로 검사하며, 폴더 전체 tree 해시를 대조한다고 설명하지 않는다.

현재 원본/후보 artifact 보관은 **3일**, 최종 artifact는 **14일**이다. 만료되면 새 빌드를 한다. Qt SDK 캐시는 빌드 결과나 성공 증거를 대신하지 않는다.

| Artifact | 의미 | 공개 릴리즈에 사용할 수 있는가 |
| --- | --- | --- |
| `macOS-native-arm64`, `macOS-native-x64` | 앱 2개와 native probe 2개의 원본 빌드 | 아니오, 배포용 Qt 패키징 전 |
| `macOS-candidate-arm64`, `macOS-candidate-x64` | 의존성/서명/전체 payload 검증 통과 | 아니오, delivery 검사 전 |
| `macOS-arm64`, `macOS-x64` | native bootstrap/update/실패 복원까지 통과 | 아래 증거 확인 후 ZIP만 선택 |

각 최종 artifact를 별도 폴더로 받는다.

```powershell
gh run download $macRunId -n macOS-arm64 -D (Join-Path $releaseRun 'macos-arm64')
gh run download $macRunId -n macOS-x64 -D (Join-Path $releaseRun 'macos-x64')
```

두 아키텍처 모두 `audit-result.json`의 native checks, `delivery-result.json`의 실제 GUI·중단 seed 복구·frame ACK·queue 적용·실패 복원, 재사용했다면 `native-source-result.json`을 확인한다. `macos-ui.png`도 직접 본다. v0.1.3의 파일 수 1,642/145 Mach-O는 **과거 결과**이며 차기 버전의 고정 합격 숫자가 아니다.

패키저는 Qt framework 심볼릭 링크를 일반 dylib로 평탄화하고 install ID/의존성/RPATH를 상대 경로로 바꾼다. bootstrap에는 client 전체 QML을 넣지 않는다. 외부 Homebrew SQL 의존성은 제외하고 app-relative `qt.conf`와 Unix 실행권한을 보존한다. 변환 후 codesign 검사를 실행한다. Windows에서 풀고 다시 압축해 Mac 실행권한·서명 검증 결과를 잃지 않는다.

`test-macos-delivery.py`의 합성 seed/bad-fixture 버전 문자열도 새 버전에서 의도한 장애를 여전히 재현하는지 검토한다. 실패 fixture가 종료 코드 4/ACK 없음으로 선검증되고 receipt에 실패 플래그까지 포함되어야 한다. 제품을 고치기 전에 fixture가 실제로 실패하는지부터 확인한다.

cloudflared는 현재 **2026.10.5**의 공식 플랫폼별 SHA 고정본이다. 업데이트하려면 `src/core/CloudflareLogin.h`, Windows `Get-Cloudflared.ps1`/패키저와 Mac 패키저의 pin·provenance·라이선스를 함께 검토하고 관련 패키지를 다시 검증한다. 공식 helper를 강제로 재서명하면 원본 SHA pin이 깨진다. SDK/도구 기준은 Qt **6.8.3**, aqtinstall **3.3.0**, 전체 커밋 SHA로 고정된 Actions다. 무조건 최신 바이너리로 갈아끼우지 않는다.

Mac 패키저와 delivery 검사는 `--arch x64`, 감사 도구 `verify-macos.py`는 `--arch x86_64`를 사용한다. ARM은 모두 `arm64`다. 감사 도구의 `--structural-only`는 네이티브 서명·의존성·실행 합격의 대체가 아니다.

## 5. 정확한 공개 파일 목록과 checksum을 만든다

Mac 최종 artifact의 **portable/payload ZIP 네 개만** `$releaseDist`로 모은다. CI evidence, 원본 native ZIP이나 private 게임 ZIP을 같이 올리지 않는다. 각 ZIP에서 링크·경로 충돌·비공개 파일 유입을 확인하고 receipt 파일 목록/크기/해시와 실행권한을 검증한다. macOS native audit와 Windows `--verify-payload` 결과를 보관한다.

```powershell
$assetNames = @(
  'TeamHJD-GameTestLauncher-Windows-x64.zip',
  'TeamHJD-LauncherPayload-Windows-x64.zip',
  'TeamHJD-GameTestLauncher-macOS-arm64.zip',
  'TeamHJD-LauncherPayload-macOS-arm64.zip',
  'TeamHJD-GameTestLauncher-macOS-x64.zip',
  'TeamHJD-LauncherPayload-macOS-x64.zip'
)
$releaseFiles = @($assetNames | ForEach-Object { Join-Path $releaseDist $_ })
$sumLines = foreach ($name in ($assetNames | Sort-Object)) {
  $file = Get-Item -LiteralPath (Join-Path $releaseDist $name) -ErrorAction Stop
  $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
  "$hash  $name"
}
[IO.File]::WriteAllLines((Join-Path $releaseDist 'SHA256SUMS.txt'), [string[]]$sumLines, [Text.Encoding]::ASCII)
$releaseFiles += Join-Path $releaseDist 'SHA256SUMS.txt'
```

와일드카드 대신 위에서 명시한 **ZIP 6개＋checksum 1개**를 사용한다. `SHA256SUMS.txt`는 사람이 대조하는 용도이며, updater는 정확한 GitHub HTTPS API의 asset `digest`를 검증한다. 다른 곳에서 받은 checksum만으로 배포본의 진위를 판단하지 않는다.

## 6. 초안 업로드 → GitHub digest 대조 → 공개

소스를 push하고 전체 40자리 대상 SHA를 기록한다. docs-only 후속 커밋이라면 검증한 네이티브 source tree와 같음을 확인한다. 미커밋 코드로 만든 산출물을 다른 커밋의 결과로 표시하지 않는다. `docs/RELEASE_VALIDATION_<version>.md`와 릴리즈 노트에는 환경/검사/실제 성공 범위/미검증 범위를 쓴다. 여러 줄 본문은 UTF-8 파일과 `--notes-file`을 사용한다.

```powershell
$releaseCommit = (git rev-parse HEAD).Trim()
if ($releaseCommit -notmatch '^[0-9a-f]{40}$') { throw 'Expected full commit SHA.' }
$notesPath = Join-Path $releaseRun 'release-notes.ko.md' # 검토한 본문 파일을 먼저 작성
gh release create "v$releaseVersion" @releaseFiles --draft --target $releaseCommit `
  --title "v$releaseVersion — Windows + macOS" --notes-file $notesPath
```

초안의 실제 REST metadata를 읽고 **tag/target/draft**, 정확한 파일명 7개, `state: uploaded`, 모든 크기와 `digest: sha256:<로컬해시>`를 대조한다. 업로드 프로세스 시작/성공 메시지만으로 공개하지 않는다. 짧은 SHA는 API가 target으로 거절한 이력이 있다. 이미 공개된 버전의 tag/ZIP은 수정하지 않는다.

```powershell
gh api 'repos/Team-HJD-21/team-hjd-game-test-launcher/releases?per_page=10'
# 위 metadata와 일곱 로컬 파일의 크기/SHA256 대조가 끝난 뒤:
gh release edit "v$releaseVersion" --draft=false --latest
gh api repos/Team-HJD-21/team-hjd-game-test-launcher/releases/latest
```

공개 후 latest가 정확한 stable 버전이고 draft/prerelease가 아니며, 여섯 ZIP의 `browser_download_url`이 정확한 저장소/태그/파일명이고 크기·digest가 그대로인지 확인한다. GitHub 화면의 Assets 수에는 자동 source ZIP/tar도 포함되므로 업로드 자산 7개와 혼동하지 않는다.

### 게시 후 기존 Windows 클라이언트 확인

```powershell
./tools/verify-launcher-auto.ps1 -QtRoot $qtRoot -ExpectedRelease $releaseVersion `
  -Executable '<검증할 이전 배포본의 TeamHJDClient.exe 절대 경로>'
```

이 도구는 이전 client EXE를 복사하고 테스트 전용 profile/install root에서 **실제 latest API·다운로드·GitHub digest·전체 파일·다음 실행 queue**를 검증한다. 유일한 합성 입력은 비교 버전 `0.0.9`이며 실제 보고 버전/API/응답을 바꾸지 않는다. 보호 조건 4개도 확인한다. 따라서 이 결과는 **사용자의 현재 설치가 이미 새 버전으로 활성화됐다는 증거가 아니다.** 실제 활성화를 별도로 관찰했을 때만 그 범위를 기록한다. 테스트 override를 운영 설정에 저장하지 않는다.

`-ExpectedRelease`는 매번 명시한다. 현재 스크립트 기본값 `0.1.0`에 의존하면 새 릴리즈 확인이 잘못된 대상으로 실패한다.

문제가 있는 공개 버전을 고칠 때에는 원인을 수정한 **더 높은 버전**을 다시 검증해 낸다. latest를 과거 태그로 돌리는 것만으로 이미 설치된 더 높은 client가 자동 downgrade되지는 않는다. 다운로드/활성화 실패 시 정상 이전 버전을 지키는 bootstrap 복원 동작을 유지한다.

## 7. 게임만 새 BUILD로 발행하기

런처 Release를 다시 만들 필요가 없다. 전달받은 Windows `Builds/latest.json` / macOS `Builds/latest-macos.json`을 **명시적으로 지정**하고, 먼저 `-VerifyOnly`로 정본 ZIP·manifest·전체 항목·Mac 실행권한을 확인한다. 게임 발행 도구의 기본 private root는 `.local/private-distribution`이며 운영 root와 다르므로 **`-PrivateRoot`를 반드시 지정**한다.

```powershell
$privateRoot = Join-Path $PWD '.local/production-distribution'
./tools/Publish-GameArchive.ps1 -LatestPointer '<Windows Builds/latest.json>' -PrivateRoot $privateRoot -VerifyOnly
./tools/Publish-GameArchive.ps1 -LatestPointer '<Windows Builds/latest.json>' -PrivateRoot $privateRoot
./tools/Publish-GameArchive.ps1 -LatestPointer '<Mac Builds/latest-macos.json>' -PrivateRoot $privateRoot -VerifyOnly
./tools/Publish-GameArchive.ps1 -LatestPointer '<Mac Builds/latest-macos.json>' -PrivateRoot $privateRoot
```

포인터의 상대 경로는 해당 `Builds` 폴더 기준이다. 수정 시각이나 옛 ZIP 이름으로 최신 빌드를 추측하지 않는다. Windows는 schema 1의 최상위 항목을 유지하고 Mac은 같은 게임 ID의 `platforms`에 추가한다. Windows/Mac의 BUILD·wire/save 호환성을 확인하고, 후속 Windows 발행에서 Mac 항목이 유지되는지도 본다. 서명 sequence·유효기간·정본 SHA를 확인한 뒤 인증된 공개 경로에서 catalog와 패키지를 검증한다.

같은 검증된 ZIP은 재사용한다. 운영 monitor는 일일 검사에서 유효기간이 2일 미만인 catalog를 7일로 재서명한다. 기간 필드만 수동 변경하거나 서명 검증을 끄지 않는다. 새 게임의 실제 화면/소리/입력/크로스플레이는 런처·ZIP 검증과 별도의 인수 항목이다.

## 8. 운영 확인과 인수인계

현재 운영 경로는 **Cloudflare Access 이메일 OTP → Tunnel → loopback HTTPS `127.0.0.1:8443`**다. 공개 HTTPS는 기본 443이며 익명 요청은 Access 로그인으로 302 이동한다. 직접 외부 8443 연결을 전제로 하는 `distribution-preflight.yml`은 별도 구성용이며 현재 Cloudflare 경로의 검증을 대신하지 않는다. 이 절차에 IP/포트 공개나 허용목록 변경을 추가하지 않는다.

```powershell
$privateRoot = Join-Path $PWD '.local/production-distribution'
./tools/Start-PlaytestHosting.ps1 -PrivateRoot $privateRoot -ValidateOnly
./tools/Start-PlaytestHosting.ps1 -PrivateRoot $privateRoot
./tools/Enable-HostingAutostart.ps1 -PrivateRoot $privateRoot -Action Status
./tools/Open-HostingDashboard.ps1 -PrivateRoot $privateRoot
./tools/Get-VerifiedTesters.ps1 -PrivateRoot $privateRoot | Format-Table
```

시작 스크립트는 기존 소유 프로세스를 검증해 재사용한다. Windows 로그인 상태·절전 없이 켜진 PC·인터넷이 필요하다. 감시는 30초, dashboard HTML은 30초 생성/15초 새로고침이다. PID 존재, 익명 302, 인증 catalog 성공, 전체 게임 다운로드 성공을 각각 구분해 기록한다. 실제 재부팅/로그온을 시험하지 않았다면 자동 시작 성공으로 쓰지 않는다. 살아 있는 서버는 시작 명령만 다시 실행해도 재사용되므로, 서버 코드 변경의 반영은 소유권을 확인한 origin의 통제된 재시작과 별도로 확인한다.

인증 이메일/시각은 private root의 `verified-users.json`에 있다. 표시/CSV는 관리 도구를 사용하고 공개 Release에 첨부하지 않는다. 원본 JWT, SAS URL, 키, 개인 이메일을 검증 기록에 붙이지 않는다. 자세한 내용은 [운영 가이드](PLAYTEST_HOSTING.ko.md), [이메일 인증](CLOUDFLARE_ACCESS.ko.md)을 따른다.

## 완료 체크리스트

- [ ] 버전/대상 commit/변경 범위/기존 bootstrap 호환성을 확인했다.
- [ ] Windows의 필요한 회귀 검사와 최종 ZIP 검증이 통과했다.
- [ ] ARM/Intel 양쪽 최종 delivery와 실제 GUI 이미지를 확인했다.
- [ ] 원본 빌드 재사용을 검증했거나 전체 빌드를 실행했다.
- [ ] 공개 ZIP 6개에 게임/개인정보/인증/키가 없고 라이선스와 helper provenance가 있다.
- [ ] draft의 자산 7개 크기/digest가 로컬 검증본과 일치한다.
- [ ] stable 공개 후 latest API/URL/digest가 일치한다.
- [ ] 구형 client의 실제 API 업데이트 검사와 사용자 설치의 적용 상태를 구분해 기록했다.
- [ ] 게임도 발행했다면 인증 경로·catalog 서명·정본 ZIP을 확인했다.
- [ ] 새 장애의 원인/확실도/수정/증거/한계를 적절한 문서에 기록했다.
- [ ] 팀원에게 portable 다운로드, 이메일 인증→설치→플레이, Mac 첫 실행 승인과 남은 실기기 검증을 안내했다.
