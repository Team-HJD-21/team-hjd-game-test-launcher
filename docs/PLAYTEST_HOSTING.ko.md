# 테스트 서버 운영

게임을 한 번 게시하면 팀원은 런처에서 이메일 인증 후 목록을 받고 설치·실행·업데이트할 수 있습니다. 같은 버전을 다시 설치하지 않으며, 새 버전은 배포 목록을 통해 확인합니다. 원본 서버는 이 PC의 `127.0.0.1:8443`에서만 받고, 외부 연결은 Cloudflare Tunnel과 Access OTP를 거칩니다.

릴리즈와 게임 발행의 순서는 [릴리즈 절차](RELEASING.ko.md), 같은 오류가 반복될 때의 최소 진단은 [장애 대응](TROUBLESHOOTING.ko.md)을 먼저 확인합니다. 발행 도구의 기본 root는 `.local/private-distribution`, hosting 도구는 `.local/production-distribution`이므로 운영 명령에는 의도한 **`-PrivateRoot`를 명시**합니다. 이 문서의 `D:/Private/...`는 예시 경로입니다.

## 시작과 자동 복구

PowerShell 7.5 이상에서 프로젝트 폴더로 이동한 뒤 실행합니다. 먼저 설정만 확인할 수 있습니다. `-ValidateOnly`는 프로세스를 시작하거나 운영 파일을 쓰지 않습니다.

```powershell
./tools/Start-PlaytestHosting.ps1 -PrivateRoot D:/Private/TeamHJDDistribution -ValidateOnly
./tools/Start-PlaytestHosting.ps1 -PrivateRoot D:/Private/TeamHJDDistribution
./tools/Stop-PlaytestHosting.ps1 -PrivateRoot D:/Private/TeamHJDDistribution -ValidateOnly
./tools/Stop-PlaytestHosting.ps1 -PrivateRoot D:/Private/TeamHJDDistribution
```

`Start-Distribution.ps1`는 Cloudflare가 활성화된 경우 `.deps/server-venv/Scripts/python.exe`를 고르고, `-PythonPath`로 명시할 수도 있습니다. Cloudflare 설정이 없는 기존 서버는 전역 Python 선택을 유지합니다. 공개 운영은 유효한 활성 설정을 요구합니다. 설치 방법과 JSON의 정확한 세 필드는 [Cloudflare 설정](CLOUDFLARE_ACCESS.ko.md)에 있습니다.

관리형 터널은 `private root/cloudflare/tunnel-token.txt`를 기본으로 사용합니다. 다른 파일은 `-TokenFile`로 지정합니다. 토큰 내용은 명령행에 넣지 않습니다. `-TunnelId`는 실제 터널 UUID를 지정할 수 있습니다. 관리형 터널의 hostname → loopback HTTPS, CA·서버 이름, Access audience 검증은 Cloudflare 대시보드의 원격 설정에서 유지해야 합니다. 이 스크립트는 원격 설정을 변경하지 않습니다.

로컬 관리 터널도 지원합니다. 토큰 파일이 없으면 `cloudflare/config.yml`과 `cloudflare/tunnel-credentials.json`을 사용합니다. helper는 `Get-Cloudflared.ps1`의 고정 버전·SHA-256이 일치해야 합니다.

기본 실행은 origin·터널과 숨김 PowerShell 감시 프로세스를 시작합니다. 감시는 30초마다 확인해 종료된 구성 요소를 다시 시작합니다. 이미 실행 중인 프로세스는 저장한 PID·시작 시각·실행 파일·정확한 인수를 확인하고 재사용합니다. 다른 프로세스를 종료하거나 사용 중인 포트를 빼앗지 않습니다. 살아 있지만 listener가 없는 origin이나 상태가 충돌하는 프로세스는 자동 종료하지 않으며 비공개 로그를 확인해야 합니다. `-NoWatch`는 감시 없이 한 번 시작합니다.

수동 중지는 `Stop-PlaytestHosting.ps1`로 합니다. 시작 잠금과 구성 요소의 소유권을 모두 확인한 뒤 감시 → origin → 터널 순서로 중지합니다. 관리 화면용 로컬 서비스와 다음 Windows 로그인의 자동 시작 등록은 유지합니다. `-ValidateOnly`는 잠금 파일도 만들지 않고 중지 계획만 읽습니다. 시작에 성공한 root의 `hosting-launch.json`은 Python·터널 helper·토큰 파일 **경로**·포트를 보존하며, 대시보드 재시작/자동 시작 켜기도 이 설정을 그대로 사용합니다. 관리 요청이 새 명령어나 경로를 지정할 수는 없습니다.

서버 코드를 변경한 뒤 같은 시작 명령을 실행하면 살아 있는 origin은 재사용됩니다. 코드 반영에는 소유권을 확인한 origin의 통제된 재시작과 외부 확인이 필요합니다. 토큰/서명 키/인증 기록을 지우거나 터널을 새로 만들어 재시작을 대신하지 않습니다.

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

비공개 `dashboard/index.html`과 같은 화면을 제공하는 **127.0.0.1:8844 전용 관리 서비스**를 준비하고 기본 브라우저로 엽니다. 게임의 8443 origin·Cloudflare Tunnel과 별도이며 외부 도메인으로 연결하지 않습니다. 서버·터널·감시의 PID/시작 시각/실행 파일/인수를 대조한 상태, 원본 루프백 TCP, 공개 인증 게이트의 익명 HEAD 응답, 공개키로 서명을 검증한 게임 목록, 인증된 이메일·시각·세션 수를 표시합니다. 기본 시각은 KST이며 UTC 원본도 함께 보입니다. 프로세스 실행이나 게이트 응답만으로 로그인 이후 게임 다운로드 성공을 판정하지 않습니다.

상단의 **서버 시작 / 서버 중지 / 다시 시작 / 상태 새로고침 / 자동 시작 켜기·끄기**를 사용합니다. 중지해도 관리 화면은 남으며, 다음 로그인 자동 시작은 별도 설정입니다. 기본은 같은 Windows 사용자 로그인 후 시작·PC 종료 시 종료입니다. 로그아웃·절전·인터넷 단절 중 계속 서비스된다는 뜻은 아닙니다. 테스터 표의 **CSV 만들기**는 현재 목록을 비공개 CSV로 갱신하고 내려받기 링크를 표시합니다.

감시는 30초마다 HTML을 갱신합니다. 로컬 HTTP 관리 화면은 서버를 중지한 동안에도 15초마다 상태를 다시 생성해 표시합니다. 기존 file: HTML도 진입점으로 유지되며 제어 버튼을 누르면 인증된 로컬 관리 화면으로 이동합니다. 마지막 생성 시각으로 오래된 화면과 구분하세요. 대시보드 생성/제어 서비스 실패는 게임 서버 시작·자동 복구를 중단하지 않습니다. `-SkipPublicCheck`는 2초 제한의 외부 HEAD 확인을 생략하고, `-GenerateOnly`는 관리 프로세스나 브라우저를 열지 않고 파일만 생성합니다. `-ExportCsv`는 같은 dashboard 폴더에 CSV를 명시적으로 내보내며 자동 갱신하지 않습니다.

이메일이 들어 있는 HTML·CSV와 관리 파일은 private root의 기존 접근 권한을 상속합니다. 생성 HTML의 숨김 form에는 이 PC의 관리 capability가 있으므로 **HTML도 외부 공유하지 않습니다.** JWT·로그인 세션 해시·publisher 키·IP·원본 로그는 표시하지 않습니다. 외부 스크립트·폰트·이미지·네트워크 API와 JS를 사용하지 않고 텍스트를 이스케이프하며 CSP는 form 제출을 이 로컬 서비스 한 곳으로 제한합니다.

관리 서비스는 정확한 Host, 요청 Origin과 256bit 무작위 token을 검사하며 상태 변경은 POST만 허용합니다. 외부 Origin과 인증 없는 개인정보 조회를 거부하고 CORS를 열지 않습니다. Open 도구의 브라우저 인계 코드는 60초·1회만 사용하고 즉시 `/`로 이동합니다. 장기 관리 token은 URL/요청 로그에 남기지 않으며 세션 cookie는 HttpOnly·SameSite=Strict입니다. `dashboard-control-token.txt`, `dashboard-control.json`, `dashboard-open.json`, 생성 HTML/CSV는 공개 배포에 넣지 않습니다. 같은 Windows 사용자/관리자가 이미 접근할 수 있는 PC 내부 관리 경계이며 PC 자체가 침해된 상황의 격리는 아닙니다.

**접속 기록 위치:** `private root/verified-users.json`. 이메일·최초/최근 인증·최근 요청·서로 다른 인증 세션 수를 보관합니다. ‘최근 5분 내 요청’은 이 기록에서 계산하며 실제 동시 접속 수, 접속 종료나 게임 플레이 상태를 뜻하지 않습니다. 현재 OS·런처 버전·장치 ID·IP·게임 로그는 이 목록에 수집하지 않습니다. Cloudflare OTP만 마치고 실제 origin 요청을 보내지 않은 사용자는 기록되지 않습니다.

관리 서비스 코드가 바뀌면 살아 있는 기존 Python은 재사용되므로 해당 서비스만 소유권 확인 후 재시작해야 합니다. 게임 서버 중지 명령이 관리 서비스까지 종료하도록 바꾸지 않습니다. 운영 프로세스는 `dashboard-control.json`의 PID뿐 아니라 시작 시각·실행 파일·`server/hosting_control.py --root <root> --port <port>` 인수를 다시 확인합니다.

정본 ZIP이 있으면 `Publish-GameArchive.ps1 -LatestPointer ... -PrivateRoot ...`를 사용합니다. `Publish-Game.ps1`은 별도 게임의 압축 해제된 빌드를 새로 포장할 때 사용하며 전달받은 정본 ZIP을 다시 만들기 위한 도구가 아닙니다. 운영자 암호와 개인 만료 초대는 관리·복구 경로로 계속 지원됩니다. [개인 초대 발행·회수](TESTER_INVITES.ko.md)는 별도 문서를 따릅니다. private root나 CSV, 토큰 파일을 공개 저장소·릴리스 ZIP에 넣지 않습니다.

## 맥 게임 게시

기존 수동 게임 ZIP 웹페이지 `/downloads`와 같은 페이지를 제공하던 `/`는 종료했습니다. 인증된 요청에는 빈 HTTP410을 반환하고, 다른 사이트로의 링크나 redirect를 추가하지 않습니다. query가 있는 경로는 기존 HTTP404를 유지합니다. 게임은 런처의 이메일 인증과 서명된 카탈로그를 통해 설치·업데이트합니다. 기존 `playtest.teamhjd.com`의 Cloudflare 로그인, `/catalog.json`, `/packages/<sha256>.zip`, 접속 감사와 이어받기는 유지됩니다. 공개 런처 ZIP 배포는 [별도 공개 배포 안내](PUBLIC_DOWNLOADS.ko.md)를 참고하세요.

고정 cloudflared 2026.10.5의 로그인 확인은 origin HTTP200을 요구하지 않습니다. `isTokenValid`는 Access 로그인으로 향하는 HTTP302인지 검사하며, 확인 요청에 `cloudflared_token_check=true`를 붙입니다. 이 query는 기존 origin에서도 HTTP404였습니다. 앱 metadata는 Cloudflare edge의 서명된 응답 header에서 얻고, 런처는 JWT 수신 후 서명된 카탈로그를 실제로 요청해 세션을 확인합니다. 따라서 HTML 종료를 위해 인증 앱·issuer·AUD 또는 런처 기본 서버 주소를 바꾸지 않습니다. 근거는 [고정 access 로그인 코드](https://github.com/cloudflare/cloudflared/blob/2026.10.5/cmd/cloudflared/access/cmd.go#L519), [Access redirect 판정](https://github.com/cloudflare/cloudflared/blob/2026.10.5/carrier/carrier.go#L109), [metadata 조회](https://github.com/cloudflare/cloudflared/blob/2026.10.5/token/token.go#L395)에 있습니다.

Windows 정본 `Builds/latest.json`과 별도로 받은 `Builds/latest-macos.json`을 지정합니다. 포인터의 `platform`과 실제 `.app/Contents/MacOS/실행파일` 경로를 읽어 기존 ZIP의 크기·SHA256·모든 파일·실행권한을 확인합니다. 원본 게임을 다시 빌드하거나 ZIP을 바꾸지 않습니다.

```powershell
./tools/Publish-GameArchive.ps1 -LatestPointer D:/Game/Builds/latest-macos.json -PrivateRoot D:/Private/TeamHJDDistribution -VerifyOnly
./tools/Publish-GameArchive.ps1 -LatestPointer D:/Game/Builds/latest-macos.json -PrivateRoot D:/Private/TeamHJDDistribution
```

같은 게임의 서명된 `platforms.macos-universal` 항목으로 저장하므로 기존 Windows 패키지·ID·버전은 보존됩니다. 기존 Windows 0.1.2 런처도 기존 항목을 계속 읽습니다. 맥 런처는 실제 CPU에 맞는 항목을 자동 선택하며 지원 빌드가 없으면 설치를 차단합니다. 나중에 Windows 버전을 게시해도 별도로 게시한 맥 항목은 남습니다. 두 운영체제의 BUILD 번호·통신 규약이 함께 테스트할 버전인지 운영자가 확인해야 합니다.

이 서버는 게임 다운로드 서버입니다. 실제 게임에서 방을 열고 친구가 접속하는 멀티플레이 호스트의 실행·네트워크 설정은 별개입니다.
