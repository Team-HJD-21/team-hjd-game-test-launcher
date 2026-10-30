# 비공개 게임 배포

공개 GitHub Release에는 런처 ZIP·런처 업데이트 payload·checksum만 올립니다. 게임 ZIP·BGM·개인 로그·암호·키를 올리지 않습니다. 런처 릴리즈는 [릴리즈 절차](RELEASING.ko.md), 현재 서버는 [운영 가이드](PLAYTEST_HOSTING.ko.md)를 먼저 읽으세요. 아래 `D:/Private/...` 경로와 example 도메인은 예시입니다.

현재 운영은 **Cloudflare Access 이메일 OTP → Tunnel → `127.0.0.1:8443`**입니다. 외부에는 기본 HTTPS 443으로 접속하며 공유기 포트포워딩이나 origin 방화벽 공개가 필요하지 않습니다. 이메일 인증 설정은 [Cloudflare 가이드](CLOUDFLARE_ACCESS.ko.md)를 따릅니다. 직접 TCP8443을 공개하는 별도 구성과 혼합하지 마세요.

## 1. HTTPS와 접근 정보

origin 전용 TCP **8443**은 게임 UDP 포트와 별개입니다. 현재 Tunnel은 loopback HTTPS의 CA·서버 이름을 검증하고 테스터는 Cloudflare의 공개 HTTPS를 사용합니다. 인증서 검증을 끄지 않습니다. 별도의 직접 접속 구성에서는 서버 DNS에 맞는 유효한 인증서를 준비하고 팀 전용 CA를 쓰면 초대 파일에 정확한 공개 인증서를 넣습니다. Windows 전역 인증서 저장소는 변경하지 않습니다.

새 publisher 초기화 또는 별도 운영자/초대 인증 경로에는 PowerShell 프로세스 환경 변수 `TEAMHJD_DISTRIBUTION_PASSWORD`를 사용합니다. 현재 테스터의 기본 로그인은 이메일 OTP이며 팀원이 운영자 암호를 입력하는 흐름이 아닙니다. 실제 암호를 명령줄 인자나 공개 문서에 적지 마세요. 초기화 도구는 평문 대신 PBKDF2 검증값을 보관합니다. 기존 운영 root에 새 publisher를 만들거나 기존 키를 교체하지 않습니다.

```powershell
./tools/New-Publisher.ps1 -ServerUrl https://playtest.example.org:8443/ `
  -PrivateRoot D:/Private/TeamHJDDistribution -CertificatePath D:/Private/tls/ca-cert.pem
```

`publisher.dpapi`는 이 Windows 계정/PC에 묶인 서명 키입니다. 덮어쓰지 말고 안전한 별도 백업 정책으로 관리하세요. 기본 발행 경로 `.local/private-distribution`과 현재 hosting 기본 경로 `.local/production-distribution`은 다르므로 운영 게시 명령에는 **`-PrivateRoot`를 명시**하세요. 별도 초대 인증을 사용할 때 `invite.json`은 신뢰할 수 있는 팀 채널로, 접근 암호는 따로 전달합니다.

## 2. 정본 ZIP 발행

게임 프로젝트의 `Builds/latest.json` schema 1을 입력합니다. `archive`, `packageManifest`는 **그 latest.json이 있는 Builds 폴더 기준 상대 경로**입니다. 수정 시각, 예전 `Windows.zip`, `Playtest`나 Archive에서 최신 빌드를 추측하지 않습니다.

```powershell
./tools/Publish-GameArchive.ps1 -LatestPointer D:/Games/Project/Builds/latest.json -VerifyOnly
./tools/Publish-GameArchive.ps1 -LatestPointer D:/Games/Project/Builds/latest.json `
  -PrivateRoot D:/Private/TeamHJDDistribution
```

포인터와 package manifest의 버전/빌드/구성/wire/GUID/크기/ZIP SHA256, ZIP 전체 항목의 크기와 SHA256, 내장 build-manifest의 runtime 목록을 확인합니다. 같은 ZIP을 private packages에 복사하고 별도의 P-256 서명 catalog를 발행합니다. Unity를 빌드하거나 원본 파일을 수정하지 않습니다. 다른 게임의 압축 해제 빌드를 포장할 때는 `Publish-Game.ps1`을 사용합니다.

catalog 기본 유효기간은 7일입니다. 현재 hosting 감시는 일일 검사에서 잔여 기간이 2일 미만이면 원본 ZIP을 바꾸지 않고 sequence·기간·서명을 갱신합니다. 수동 갱신에는 `Renew-DistributionCatalog.ps1 -PrivateRoot ...`를 사용합니다. 같은 정본 ZIP을 다시 발행해도 패키지는 재사용하며 파일 해시가 같은 설치본은 중복 설치하지 않습니다. Mac 포인터와 플랫폼 항목의 발행/보존은 [맥 게임 게시](PLAYTEST_HOSTING.ko.md#맥-게임-게시)를 따릅니다.

## 3. 서버 실행과 외부 연결

현재 Cloudflare 설정과 터널 자격 증명이 준비된 root에서는 다음을 사용합니다. 기존 소유 origin·터널·감시를 검증해 재사용하며 전역 방화벽/공유기를 변경하지 않습니다.

```powershell
./tools/Start-PlaytestHosting.ps1 -PrivateRoot D:/Private/TeamHJDDistribution -ValidateOnly
./tools/Start-PlaytestHosting.ps1 -PrivateRoot D:/Private/TeamHJDDistribution
```

Windows 로그인 상태·절전 없이 켜진 PC·인터넷이 필요합니다. 자동 시작 등록, 복구 감시와 상태 표시는 [운영 가이드](PLAYTEST_HOSTING.ko.md)를 따릅니다. 실제 게임의 멀티플레이 호스트 실행과는 별개입니다.

### 별도 직접 접속 구성

아래는 Tunnel을 사용하지 않는 별도 서버를 명시적으로 운영할 때의 원시 실행 예시입니다. 현재 Cloudflare 운영 PC에 그대로 적용하지 않습니다.

```powershell
python server/distribution_server.py --root D:/Private/TeamHJDDistribution `
  --bind 0.0.0.0 --port 8443 `
  --cert D:/Private/tls/server-chain.pem --key D:/Private/tls/server-key.pem
```

직접 접속 구성에서는 먼저 `--bind 127.0.0.1`로 로컬 HTTPS를 검증하고 명시적인 외부 공개 작업으로 방화벽·라우터·인증서를 구성한 뒤 다른 네트워크에서 인증 목록·다운로드를 확인합니다. 원시 Python 실행 명령은 방화벽/공유기/자동 시작을 설정하지 않습니다. `distribution-preflight.yml`도 이 직접 TCP8443 구성용이며 현재 Cloudflare의 익명 302 확인을 대신하지 않습니다.

루트 목록·키·암호·사용자 감사 파일은 HTTP로 노출하지 않습니다. 현재 서버는 인증된 `/catalog.json`, `/packages/<sha256>.zip` API를 제공합니다. 기존 수동 게임 다운로드 HTML인 `/`와 `/downloads`는 종료했으며, 인증 뒤 빈 HTTP410을 반환합니다. 다른 사이트로 이동시키거나 링크를 표시하지 않습니다. query가 붙은 경로는 기존처럼 HTTP404입니다. 런처의 Cloudflare 로그인과 비공개 게임 설치·업데이트 API는 같은 도메인에서 유지합니다. 업로드/원격 실행 기능은 없습니다. 잘못된 암호 시도는 제한합니다. TLS 없는 인터넷 HTTP는 허용하지 않으며 `--test-http`는 명시적 loopback 시험에만 사용합니다. 공개 런처 ZIP 배포와는 별도이며 [공개 배포 안내](PUBLIC_DOWNLOADS.ko.md)를 참고하세요.

## 로컬 검증

`verify-local.ps1`은 임의 시험 암호, 2일짜리 localhost 인증서와 합성 실행 파일을 생성해 HTTPS·서명·이어받기·업데이트·실행 중 차단·신뢰하지 않는 TLS 거부를 확인하고 서버를 종료합니다. 생성물은 `.local/`에만 남습니다. 실제 외부 배포 서버가 만들어졌다는 의미가 아닙니다.
