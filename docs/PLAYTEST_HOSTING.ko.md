# 테스트 서버 운영

게임을 한 번 게시하면 팀원은 런처에서 이메일 인증 후 목록을 받고 설치·실행·업데이트할 수 있습니다. 같은 버전을 다시 설치하지 않으며, 새 버전은 배포 목록을 통해 확인합니다. 원본 서버는 이 PC의 `127.0.0.1:8443`에서만 받고, 외부 연결은 Cloudflare Tunnel과 Access OTP를 거칩니다.

## 시작과 자동 복구

PowerShell 7.5 이상에서 프로젝트 폴더로 이동한 뒤 실행합니다. 먼저 설정만 확인할 수 있습니다. `-ValidateOnly`는 프로세스를 시작하거나 운영 파일을 쓰지 않습니다.

```powershell
./tools/Start-PlaytestHosting.ps1 -PrivateRoot D:/Private/TeamHJDDistribution -ValidateOnly
./tools/Start-PlaytestHosting.ps1 -PrivateRoot D:/Private/TeamHJDDistribution
```

`Start-Distribution.ps1`는 Cloudflare가 활성화된 경우 `.deps/server-venv/Scripts/python.exe`를 고르고, `-PythonPath`로 명시할 수도 있습니다. Cloudflare 설정이 없는 기존 서버는 전역 Python 선택을 유지합니다. 공개 운영은 유효한 활성 설정을 요구합니다. 설치 방법과 JSON의 정확한 세 필드는 [Cloudflare 설정](CLOUDFLARE_ACCESS.ko.md)에 있습니다.

관리형 터널은 `private root/cloudflare/tunnel-token.txt`를 기본으로 사용합니다. 다른 파일은 `-TokenFile`로 지정합니다. 토큰 내용은 명령행에 넣지 않습니다. `-TunnelId`는 실제 터널 UUID를 지정할 수 있습니다. 관리형 터널의 hostname → loopback HTTPS, CA·서버 이름, Access audience 검증은 Cloudflare 대시보드의 원격 설정에서 유지해야 합니다. 이 스크립트는 원격 설정을 변경하지 않습니다.

로컬 관리 터널도 지원합니다. 토큰 파일이 없으면 `cloudflare/config.yml`과 `cloudflare/tunnel-credentials.json`을 사용합니다. helper는 `Get-Cloudflared.ps1`의 고정 버전·SHA-256이 일치해야 합니다.

기본 실행은 origin·터널과 숨김 PowerShell 감시 프로세스를 시작합니다. 감시는 30초마다 확인해 종료된 구성 요소를 다시 시작합니다. 이미 실행 중인 프로세스는 저장한 PID·시작 시각·실행 파일·정확한 인수를 확인하고 재사용합니다. 다른 프로세스를 종료하거나 사용 중인 포트를 빼앗지 않습니다. 살아 있지만 listener가 없는 origin이나 상태가 충돌하는 프로세스는 자동 종료하지 않으며 비공개 로그를 확인해야 합니다. `-NoWatch`는 감시 없이 한 번 시작합니다.

로그·상태는 private root의 `runtime-logs/`, `server-process.json`, `cloudflare/tunnel-process.json`, `hosting-monitor.json`에 남습니다. 터널 프로세스가 실행됐다는 결과는 외부 연결 성공의 증거와 별개이므로 공개 HTTPS 응답을 확인합니다. 감시 경고는 고정 메시지만 기록하며 원본 JWT·토큰·요청 URL은 저장하지 않습니다.

자동 시작은 **같은 Windows 사용자가 로그인할 때** 실행하도록 아래 도구로 등록합니다. 관리자 권한 없이 현재 사용자의 `HKCU/Software/Microsoft/Windows/CurrentVersion/Run`에 `TeamHJDPlaytestHosting` 한 항목만 관리합니다. 짧은 Run 명령은 private root의 `hosting-autostart.ps1`을 실행하며 이 파일에는 경로·포트만 들어갑니다. 등록은 서버를 즉시 실행하지 않습니다. 서명 키가 해당 Windows 계정의 DPAPI로 보호되므로 다른 사용자나 로그인 이전 서비스로 임의 전환하지 않습니다. PC가 켜져 있어도 로그인 전에는 자동 시작되지 않으며 절전·네트워크 단절 상태이면 배포 서버도 사용할 수 없습니다.

```powershell
./tools/Enable-HostingAutostart.ps1 -PrivateRoot D:/Private/TeamHJDDistribution -ValidateOnly
./tools/Enable-HostingAutostart.ps1 -PrivateRoot D:/Private/TeamHJDDistribution
./tools/Enable-HostingAutostart.ps1 -PrivateRoot D:/Private/TeamHJDDistribution -Action Status
./tools/Enable-HostingAutostart.ps1 -PrivateRoot D:/Private/TeamHJDDistribution -Action Disable
```

`-ValidateOnly`는 등록 계획만 읽고 wrapper·레지스트리를 쓰지 않습니다. 해제는 다음 로그인의 자동 시작을 끄며 이미 실행 중인 origin·터널·감시를 종료하지 않습니다. 다른 private root가 같은 등록 이름을 사용하는 경우 덮어쓰지 않습니다.

Windows가 로그인 후 Run 실행을 지연할 수 있습니다. 짧은 wrapper는 공식 [Run 키의 260자 명령행 제한](https://learn.microsoft.com/windows/win32/setupapi/run-and-runonce-registry-keys)에 맞추기 위해 사용합니다.

## 목록 유효기간 자동 갱신

```powershell
./tools/Renew-DistributionCatalog.ps1 -PrivateRoot D:/Private/TeamHJDDistribution
```

시작 시와 감시 중 하루마다 검사합니다. 남은 유효기간이 2일 미만이면 기존 서명을 검증하고 sequence를 1 올려 현재 UTC 시각부터 7일로 다시 서명합니다. 게임 버전·패키지 URL·해시·파일 목록·기타 목록 정보는 보존하며 ZIP을 다시 만들거나 변경하지 않습니다. 아직 여유가 있으면 파일을 그대로 둡니다. 수동 게시와 같은 잠금을 사용하므로 게시 중에는 겹쳐 쓰지 않습니다. `-Force`는 명시적인 강제 갱신용입니다.

## 인증한 이메일 보기

```powershell
./tools/Get-VerifiedTesters.ps1 -PrivateRoot D:/Private/TeamHJDDistribution | Format-Table
./tools/Get-VerifiedTesters.ps1 -PrivateRoot D:/Private/TeamHJDDistribution -AsJson
./tools/Get-VerifiedTesters.ps1 -PrivateRoot D:/Private/TeamHJDDistribution -CsvPath D:/Private/verified-testers.csv
```

인증된 이메일, 처음/최근 인증 확인 UTC, 최근 요청 UTC, 서로 다른 세션 수를 보여줍니다. 기본 실행은 파일을 쓰거나 업로드하지 않습니다. CSV는 `-CsvPath`를 명시할 때만 쓰며 기존 파일 교체에는 `-Force`가 필요합니다. 세션 해시·JWT·IP는 보고서에 포함하지 않습니다. 이메일은 Cloudflare OTP 인증 후 실제 origin 요청까지 도착한 시점에 기록됩니다. 아직 아무도 접속하지 않았다면 빈 목록이 정상입니다.

## 로컬 브라우저 대시보드

```powershell
./tools/Open-HostingDashboard.ps1 -PrivateRoot D:/Private/TeamHJDDistribution
./tools/Open-HostingDashboard.ps1 -PrivateRoot D:/Private/TeamHJDDistribution -ExportCsv
```

비공개 `dashboard/index.html`을 생성하고 기본 브라우저로 엽니다. 새 공개 포트나 웹 서버는 만들지 않습니다. 서버·터널·감시의 PID/시작 시각/실행 파일/인수를 대조한 상태, 원본 루프백 TCP, 공개 인증 게이트의 익명 HEAD 응답, 공개키로 서명을 검증한 게임 목록, 인증된 이메일·시각·세션 수를 표시합니다. 기본 표 시각은 KST이며 UTC 원본도 함께 보입니다. 프로세스 실행이나 게이트 응답만으로 로그인 이후 게임 다운로드 성공을 판정하지 않습니다.

감시가 실행 중이면 30초마다 HTML을 다시 생성하고 브라우저는 15초마다 로컬 파일을 새로 읽습니다. 마지막 생성 시각으로 오래된 화면과 구분하세요. 대시보드 생성 실패는 서버 시작·자동 복구를 중단하지 않습니다. `-SkipPublicCheck`는 2초 제한의 외부 HEAD 확인도 생략하고, `-GenerateOnly`는 브라우저를 열지 않고 파일만 생성합니다. `-ExportCsv`는 같은 dashboard 폴더에 안전한 CSV를 명시적으로 내보내 상대 링크를 추가합니다. CSV는 내보낸 시각을 별도로 표시하며 감시가 자동으로 다시 내보내지 않습니다.

이메일이 들어 있는 HTML·CSV는 private root의 기존 접근 권한을 상속합니다. 파일에는 JWT·세션 해시·키·IP·원본 로그를 넣지 않습니다. 외부 스크립트·폰트·이미지·네트워크 API를 불러오지 않는 정적 HTML이며 사용자 텍스트는 HTML 이스케이프합니다. 이 폴더를 별도 웹 서버나 공개 저장소로 공개하지 않습니다.

새 빌드는 기존 `Publish-Game.ps1`로 게시합니다. 운영자 암호와 개인 만료 초대는 관리·복구 경로로 계속 지원됩니다. [개인 초대 발행·회수](TESTER_INVITES.ko.md)는 별도 문서를 따릅니다. private root나 CSV, 토큰 파일을 공개 저장소·릴리스 ZIP에 넣지 않습니다.

## 맥 게임 게시

테스터에게 [게임 다운로드 페이지](https://playtest.teamhjd.com/downloads)를 전달하면 이메일 인증 후 Windows 또는 macOS 원본 ZIP을 선택해 직접 설치할 수 있습니다. 이 페이지와 패키지 모두 같은 Cloudflare Access 인증을 요구합니다. 페이지는 서명된 카탈로그와 패키지 해시·크기를 검증하며, 읽기 전용입니다. 서버에는 업로드나 원격 실행 기능이 추가되지 않습니다.

Windows 정본 `Builds/latest.json`과 별도로 받은 `Builds/latest-macos.json`을 지정합니다. 포인터의 `platform`과 실제 `.app/Contents/MacOS/실행파일` 경로를 읽어 기존 ZIP의 크기·SHA256·모든 파일·실행권한을 확인합니다. 원본 게임을 다시 빌드하거나 ZIP을 바꾸지 않습니다.

```powershell
./tools/Publish-GameArchive.ps1 -LatestPointer D:/Game/Builds/latest-macos.json -PrivateRoot D:/Private/TeamHJDDistribution -VerifyOnly
./tools/Publish-GameArchive.ps1 -LatestPointer D:/Game/Builds/latest-macos.json -PrivateRoot D:/Private/TeamHJDDistribution
```

같은 게임의 서명된 `platforms.macos-universal` 항목으로 저장하므로 기존 Windows 패키지·ID·버전은 보존됩니다. 기존 Windows 0.1.2 런처도 기존 항목을 계속 읽습니다. 맥 런처는 실제 CPU에 맞는 항목을 자동 선택하며 지원 빌드가 없으면 설치를 차단합니다. 나중에 Windows 버전을 게시해도 별도로 게시한 맥 항목은 남습니다. 두 운영체제의 BUILD 번호·통신 규약이 함께 테스트할 버전인지 운영자가 확인해야 합니다.

이 서버는 게임 다운로드 서버입니다. 실제 게임에서 방을 열고 친구가 접속하는 멀티플레이 호스트의 실행·네트워크 설정은 별개입니다.
