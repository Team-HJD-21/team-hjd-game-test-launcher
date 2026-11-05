# 런처 릴리즈와 게임 발행 절차

2026-11-05 · 설치형 배포 계약0.2.0부터. 이전 릴리즈 검증 문서는 각 버전의 역사적 증거이며 현재 서버·인증·클라이언트 상태를 대신하지 않습니다.

## 1. 배포 대상을 구분한다

| 대상 | 적용 방식 |
| --- | --- |
| Windows 설치형 런처 | 정적 서명 피드 → Setup EXE 크기/SHA256 확인 → 정확한 부모 종료 대기 → 새 슬롯·제거기·바로가기·Installed Apps 버전 갱신 → GUI health ACK → commit |
| 기존 ZIP/portable 런처 | 서명 피드 → payload 검증 → 새 payload의 helper로 자동 재시작 → 안정된 실행기 갱신 → health ACK. 설치형 전환은 새 Setup으로 한 번 진행 |
| macOS 런처 | ARM/Intel DMG로 최초 설치. 자체 업데이트는 검증된 payload와 seed-free 앱을 사용해 runtime·안정된 앱을 관리. 쓰기 권한 없는 앱은 최신 DMG로 복구 |
| 게임 | 비공개 서명 catalog를 자동 확인·알림하고 **업데이트 버튼으로만 적용** |
| 운영 서버 | 별도 소유 프로세스 재시작 필요. Release 게시만으로 origin 코드가 교체되지 않음 |

클라이언트와 다운로드 사이트는 `testlauncher.teamhjd.com/updates/stable.json`의 P-256 서명을 검증합니다. 네이티브 보조 경로는 GitHub 최신 Release의 `launcher-stable.json` 직접 파일이며 REST latest API가 아닙니다. 성공 확인은 약15분 주기, 오류는15초부터 최대약5분 간격으로 재시도합니다. 업데이트/동의 확인 전 온라인 기능은 차단하며 설치 게임은 명시적 오프라인 실행을 제공합니다. 다운로드·해시·서명·시각·sequence·health 검사를 끄지 않습니다.

0.1.4 등 구형 클라이언트의 첫 전환은 여전히 그 버전의 GitHub API 제약을 받습니다. 따라서 팀원에게는 이번 한 번 최신 Setup/DMG 설치와 안정된 실행기 재고정을 안내합니다. 인터넷 단절·배포처 차단·디스크/권한/OS 차단까지 소프트웨어가 강제로 해결한다고 보장하지 않습니다.

## 2. 소스·비밀·출력을 분리한다

- 공개 저장소/CI/Release에는 런처·의존성·라이선스만 넣습니다. Unity 게임, BGM, 실제 이메일/로그/설정/토큰/서명 개인키는 제외합니다.
- 원본 `dist/`는 실제 사용자 설치/작업표시줄 대상일 수 있습니다. **이동·삭제·덮어쓰지 않습니다.** 모든 패키징은 새로운 `-OutputRoot`를 사용합니다.
- CMake의 project VERSION을 새 버전으로 변경합니다. 기존 공개 태그/자산은 바꾸지 않습니다. 아래 값은 매 릴리즈 실제 값으로 수정합니다.

```powershell
$version = '0.2.0'
$qtRoot = 'C:/Qt/6.8.3/msvc2022_64' # 실제 설치된 Qt6.8.3 MSVC x64
$releaseRun = Join-Path $PWD ('.local/releases/' + $version + '-' + (Get-Date -Format yyyyMMdd-HHmmss))
$releaseDist = Join-Path $releaseRun 'dist'
New-Item -ItemType Directory -Path $releaseRun -ErrorAction Stop | Out-Null
```

`build-windows.ps1 -Deploy`는 기본dist를 사용하므로 운영PC에서 사용하지 않습니다. 변경 소스를 검토·커밋한 뒤 full40SHA를 기록하고 최종 빌드와 원본을 연결합니다. 패키지 생성 이후 재패키징한 파일에는 새 해시/검증 기록이 필요합니다.

## 3. Windows·설치기 검증

```powershell
./tools/build-windows.ps1 -QtRoot $qtRoot
./tools/verify-bootstrap.ps1 -QtRoot $qtRoot
./tools/package-windows.ps1 -QtRoot $qtRoot -OutputRoot $releaseDist
$bundle = Join-Path $releaseDist 'TeamHJD-GameTestLauncher-Windows-x64'
./tools/package-installer-windows.ps1 -BundleRoot $bundle -OutputDirectory (Join-Path $releaseRun 'installer-tests') -TestPackage
./tools/verify-installer-windows.ps1 -Installer (Join-Path $releaseRun 'installer-tests/TeamHJD-GameTestLauncher-Windows-x64-Setup-test.exe') -Fixture './build/LauncherFixtureGame.exe'
# 위 출력의 정확한 새result.json을 지정:
./tools/verify-installer-update-windows.ps1 -VerificationResult '<새 테스트 run/result.json>' -Installer (Join-Path $releaseRun 'installer-tests/TeamHJD-GameTestLauncher-Windows-x64-Setup-test.exe')
./tools/package-installer-windows.ps1 -BundleRoot $bundle -OutputDirectory $releaseDist
```

NSIS3.13은 `Get-Nsis.ps1`가 공식 ZIP 크기2363925/SHA256고정과 추출된 파일 bytes를 검증합니다. 기존 캐시는 `-Offline`으로 확인할 수 있습니다. HTML을 ZIP으로 실행하거나 전역ASLR/PATH/권한을 변경하지 않습니다. NSIS 자체를 시스템에 설치하지 않습니다.

TestPackage는 별도HKCU `TeamHJDGameTestLauncher-Test` 등록/바로가기 생략/합성프로필만 사용합니다. 기존 테스트 등록이 남아 있으면 소유 위치를 먼저 확인합니다. 제작기·검증기·실제 사용자 설치를 혼용하지 않습니다. 정상 설치·복구·제거·재설치, unknown 파일/게임/설정 보존, 정확한 부모wait·실제GUIhealth·ARP버전을 확인합니다.

C++/Controller 필수 회귀는 서명 피드, bootstrap transaction, 인증 상태/개별 동의, 실제 core, GUI창/최대화, 수동 게임 업데이트와 Windows의 자동 업데이터 흐름을 포함합니다. 환경에 따라 대상 수가 다르므로 실제 결과를 기록합니다. `LauncherUpdaterTests`는 ephemeral키/합성 transport로 실패→재시도→해시 차단→queue→helper handoff를 확인하며, 실제 네트워크 검사를 대체하지 않습니다.

실패 health 복원/사용 중 제거/다운그레이드/손상 경로는 `verify-installer-rollback-windows.ps1`, `verify-installer-negative-windows.ps1`의 명시적 합성 bundle로 검수합니다. fixture의 정상/실패 Client와 receipt 해시를 함께 만들며, 검증을 끄거나 Unity 게임을 재빌드하지 않습니다. synthetic health와 실제제품 GUI health를 구분합니다. Windows가 잠긴 경우 픽셀/새GUIframe 검증에 영향을 줄 수 있으므로 결과를 추정하지 않습니다.

서버 변경은 준비된 venv에서 전체 `python -X utf8 -m unittest discover -s tests -p 'test_*.py' -v`를 실행합니다. 제한TEMP/socket 오류는 공통 장애 기록의 정상 사용자 실행 경계와 구분합니다. assert를 줄이지 않습니다.

## 4. 실제 ARM·Intel Mac 검사

개인Mac 없이도 `macos-15` ARM과 `macos-15-intel` runner에서 빌드·실행합니다. Windows ZIP 구조 확인만으로 합격시키지 않습니다. C++/버전/QML/assets/vendor/C++테스트 변경 시 전체 빌드합니다.

```powershell
gh workflow run macos-release.yml --ref '<커밋을 올린 브랜치>'
gh run list --workflow macos-release.yml --limit 5 --json databaseId,headSha,status,conclusion
```

후속 Python패키징/검사만 변경했을 때 `-f native_run_id=<원본run>`을 사용할 수 있으나, **validate-native-macos-reuse.py**가 원본동일저장소/두build성공/미만료artifact/전체native tree와변경allowlist를 통과해야 합니다. 조건을 느슨하게 바꾸지 않습니다. SDK/컴파일러/플래그 변경도 전체빌드 대상입니다.

| artifact | 공개 가능 여부 |
| --- | --- |
| macOS-native-arm64/x64 | 원본빌드. 공개불가 |
| macOS-candidate-arm64/x64 | 감사후보. delivery전 공개불가 |
| macOS-arm64/x64 | 아래 최종 증거를 확인한 뒤 정확한2ZIP+DMG만 선택 |

원본보관3일/최종14일입니다. 최종run에서 `audit-result.json`, `dmg-result.json`, `delivery-result.json`, 재사용시 `native-source-result.json`, `macos-ui.png`를 확인합니다. 파일 수는 버전마다 달라지며 과거 고정 숫자와 비교하지 않습니다. 실제core payload roundtrip, 의존성·실행권한·strict codesign, DMG read-only mount의 전체앱 bytes/modes·Applications링크·약관, 첫seed·queue·건강확인·실패복원을 모두 확인합니다. 사람이OTP/Unity플레이/실제재부팅을 하지 않았으면 미검증으로 기록합니다.

**DMG 생성에서 `hdiutil -srcfolder`로 되돌리지 않습니다.** `._teamhjd-receipt.json`이 AppleDouble로 분류되어 누락된 이력이 있습니다. 현재는 writable image→POSIX파일복사→unmount→UDZO변환이며 최종mount에서 원본과 완전 대조합니다. receipt누락을 무시하도록 검사하지 않습니다. Mac ZIP은 Windows에서 풀고 재압축하지 않습니다.

## 5. 최종11개 파일·로컬 서명

모든 플랫폼 최종파일을 releaseDist에 모으되 와일드카드로 업로드하지 않습니다.

```powershell
$names = @(
 'TeamHJD-GameTestLauncher-Windows-x64.zip',
 'TeamHJD-LauncherPayload-Windows-x64.zip',
 'TeamHJD-GameTestLauncher-Windows-x64-Setup.exe',
 'TeamHJD-GameTestLauncher-macOS-arm64.zip',
 'TeamHJD-LauncherPayload-macOS-arm64.zip',
 'TeamHJD-GameTestLauncher-macOS-arm64.dmg',
 'TeamHJD-GameTestLauncher-macOS-x64.zip',
 'TeamHJD-LauncherPayload-macOS-x64.zip',
 'TeamHJD-GameTestLauncher-macOS-x64.dmg'
)
$releaseCommit = (git rev-parse HEAD).Trim() # full40SHA
./tools/Write-LauncherFeed.ps1 -ReleaseDirectory $releaseDist -OutputFile (Join-Path $releaseDist 'launcher-stable.json') -SourceCommit $releaseCommit -Version $version -MinimumVersion $version
$names += 'launcher-stable.json'
$lines = foreach($name in ($names|Sort-Object)) {
 $file = Get-Item -LiteralPath (Join-Path $releaseDist $name) -ErrorAction Stop
 ((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant() + '  ' + $name)
}
[IO.File]::WriteAllLines((Join-Path $releaseDist 'SHA256SUMS.txt'),[string[]]$lines,[Text.Encoding]::ASCII)
$names += 'SHA256SUMS.txt'
$files = @($names|ForEach-Object {Join-Path $releaseDist $_})
```

서명키는 최초에만 `New-LauncherPublisher.ps1`로 생성하며 공개키가 제품에고정됩니다. 기존 `.local/launcher-publisher/publisher.dpapi`를 재생성하지 않습니다. 현재Windows계정DPAPI/DACL보호이며 공개CI에 개인키를 보내지 않습니다. 다른PC에 암호화파일만복사하면복호화된다는뜻이 아닙니다. 키유실시새서명키를기존클라이언트가자동신뢰하지 않으므로 공식새Setup/DMG로복구·키회전계획이필요합니다.

피드sequence는단조증가, 유효기간365일입니다. 새릴리즈가없어도만료전에동일한검증산출물로상위sequence 피드를갱신해사이트에게시해야합니다. 기존공개Release asset을덮어쓰지않고, 기본피드와직접Release보조피드의유효기간차이를기록합니다. sourceCommit/파일크기/해시가최종검증본과같은지서명후다시확인합니다.

## 6. Draft 대조→공개→사이트/운영 정책

```powershell
gh release create "v$version" @files --draft --target $releaseCommit --title "v$version — Setup + macOS DMG" --notes-file '<검토한UTF8릴리즈노트파일>'
# Draft API에서정확11이름/stateuploaded/크기/digest:sha256:<로컬>대조후:
gh release edit "v$version" --draft=false --latest
gh api repos/Team-HJD-21/team-hjd-game-test-launcher/releases/latest
```

fullSHA/최종source일치/모든필수회귀/양쪽Mac최종증거를대조한뒤공개합니다. 공개태그/자산은불변입니다. 후보·prototype·합성bad-health installer·원본CI로그를올리지않습니다.

**사이트피드는모든파일을공개대조한뒤마지막에게시합니다.** `launcher-stable.json`을 `website/updates/stable.json`으로동일bytes복사하고 `node tools/check-download-site.mjs --update-asset-versions`, `node tools/test-release-feed.mjs`를실행합니다. Pages배포성공및실제HTTPS피드서명/모든URL을확인합니다. 새필수버전/개인정보gate는이후에비공개 `client-policy.json`으로활성화합니다. 포털 `/downloads`를살리지않고기존playtest API도메인도삭제하지않습니다.

공개후실제latest API의정확버전/11digest, 기존0.1.4다운로드·검증·queue, 새피드기반다운로드·설치/handoff를검사합니다. 격리테스트의비교override와사용자실제설치활성화는별도로기록합니다. 오류를숨기기위해비교버전/상태파일/active포인터를실제사용자설치에수동주입하지않습니다.

`docs/RELEASE_VALIDATION_<version>.md`에환경·원인확실도·수정·증거·미검증범위를남기고운영handoff와팀원안내를갱신합니다. 모든필수작업이끝나기전목표를complete로표시하지않습니다.

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
- [ ] draft의 자산11개(6ZIP·Setup·2DMG·피드·체크섬) 크기/digest가 로컬 검증본과 일치한다.
- [ ] stable 공개 후 latest API/URL/digest가 일치한다.
- [ ] 구형 client의 실제 API 업데이트 검사와 사용자 설치의 적용 상태를 구분해 기록했다.
- [ ] 게임도 발행했다면 인증 경로·catalog 서명·정본 ZIP을 확인했다.
- [ ] 새 장애의 원인/확실도/수정/증거/한계를 적절한 문서에 기록했다.
- [ ] 팀원에게 기존 ZIP에서 Setup/DMG로 한 번 전환, 개인정보 선택·이메일 인증→게임 설치→플레이, 게임 수동 업데이트와 오프라인·제거/복구를 안내했다.

## 2026-11-03 추가 기준

[v0.1.4 검증](RELEASE_VALIDATION_0.1.4.md)을 함께 확인한다. 런처 시작 시 이메일 인증보다 먼저 업데이트를 확인하며, 업데이트·인증 모달이 열린 동안 배경 조작을 막는다. `queued-launcher.json`은 다음 안정된 TeamHJDLauncher 실행에서 적용된다. Client 파일만 반복해서 직접 열어 적용 완료로 오인하지 않는다. 설치 게임은 인증/업데이트 확인 실패 모달에서 오프라인 모드로 계속할 수 있다.

Mac CI의 connection/core 테스트는 같은 임시 Keychain에서 서로 다른 unsigned 바이너리의 wrap 항목을 공유하지 않는다. 정해진 disposable Keychain의 해당 테스트 항목만 정리하고 각 바이너리의 실제 protect/unprotect 검사는 유지한다. 테스트 UI의 GitHub API rate-limit 오류와 native 전달 성공을 구분한다.
