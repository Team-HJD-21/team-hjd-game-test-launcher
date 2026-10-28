# 비공개 게임 배포

공개 GitHub Release에는 런처 ZIP과 런처 업데이트 payload만 올립니다. 게임 ZIP·BGM·개인 로그·암호·키를 올리지 않습니다. 아래 경로와 도메인은 예시입니다. 실제 외부 접속과 포트포워딩은 별도로 확인해야 합니다.

## 1. HTTPS와 접근 정보

업데이트 전용 TCP 포트로 **8443**을 제안합니다. 게임 UDP 포트와 별개입니다. 서버 DNS 이름에 맞는 유효한 TLS 인증서를 준비하세요. 공인 인증서 또는 팀 전용 CA를 쓸 수 있습니다. 팀 전용 CA는 초대 파일에 정확한 공개 인증서를 넣으며, Windows 전역 인증서 저장소나 검증 설정을 변경하지 않습니다.

PowerShell 프로세스 환경 변수 `TEAMHJD_DISTRIBUTION_PASSWORD`에 팀 전용 접근 암호를 설정하세요. 명령줄 인자나 공개 문서에 실제 암호를 적지 마세요. 초기화 도구는 평문 대신 PBKDF2 검증값을 보관합니다.

```powershell
./tools/New-Publisher.ps1 -ServerUrl https://playtest.example.org:8443/ `
  -PrivateRoot D:/Private/TeamHJDDistribution -CertificatePath D:/Private/tls/ca-cert.pem
```

`publisher.dpapi`는 이 Windows 계정/PC에 묶인 서명 키입니다. 덮어쓰지 말고 안전한 별도 백업 정책으로 관리하세요. `invite.json`은 신뢰할 수 있는 팀 채널로, 접근 암호는 별도로 전달하세요. 초대 파일에 서버 주소·공개키·선택적인 CA 공개 인증서만 들어갑니다.

## 2. 정본 ZIP 발행

게임 프로젝트의 `Builds/latest.json` schema 1을 입력합니다. `archive`, `packageManifest`는 **그 latest.json이 있는 Builds 폴더 기준 상대 경로**입니다. 수정 시각, 예전 `Windows.zip`, `Playtest`나 Archive에서 최신 빌드를 추측하지 않습니다.

```powershell
./tools/Publish-GameArchive.ps1 -LatestPointer D:/Games/Project/Builds/latest.json -VerifyOnly
./tools/Publish-GameArchive.ps1 -LatestPointer D:/Games/Project/Builds/latest.json `
  -PrivateRoot D:/Private/TeamHJDDistribution
```

포인터와 package manifest의 버전/빌드/구성/wire/GUID/크기/ZIP SHA256, ZIP 전체 항목의 크기와 SHA256, 내장 build-manifest의 runtime 목록을 확인합니다. 같은 ZIP을 private packages에 복사하고 별도의 P-256 서명 catalog를 발행합니다. Unity를 빌드하거나 원본 파일을 수정하지 않습니다. 다른 게임의 압축 해제 빌드를 포장할 때는 `Publish-Game.ps1`을 사용합니다.

catalog 기본 유효기간은 7일입니다. 기간이 끝나기 전에 같은 정본 ZIP을 다시 발행하면 게임 패키지는 재사용되고 목록의 sequence·기간·서명이 갱신됩니다. 파일 해시가 같은 설치본은 중복 설치하지 않습니다.

## 3. 서버 실행과 외부 연결

```powershell
python server/distribution_server.py --root D:/Private/TeamHJDDistribution `
  --bind 0.0.0.0 --port 8443 `
  --cert D:/Private/tls/server-chain.pem --key D:/Private/tls/server-key.pem
```

먼저 `--bind 127.0.0.1`로 로컬 HTTPS를 확인하세요. 외부 배포 단계에서 해당 서버 PC의 TCP8443 방화벽 허용과 공유기의 TCP8443 포트포워딩을 구체적인 내부 IP로 설정합니다. 이후 같은 LAN이 아닌 실제 외부 네트워크에서 초대 정보로 목록·다운로드를 확인합니다. 이 저장소의 도구는 방화벽, 공유기, 서비스 자동 시작을 자동 설정하지 않습니다.

루트 목록·키·암호 파일은 HTTP로 노출하지 않습니다. 서버가 허용하는 경로는 `/catalog.json`, `/packages/<sha256>.zip`뿐입니다. 잘못된 암호 시도는 제한합니다. TLS가 없는 인터넷 HTTP는 클라이언트와 서버 모두 허용하지 않습니다. `--test-http`는 명시적인 loopback 시험에만 사용합니다.

## 로컬 검증

`verify-local.ps1`은 임의 시험 암호, 2일짜리 localhost 인증서와 합성 실행 파일을 생성해 HTTPS·서명·이어받기·업데이트·실행 중 차단·신뢰하지 않는 TLS 거부를 확인하고 서버를 종료합니다. 생성물은 `.local/`에만 남습니다. 실제 외부 배포 서버가 만들어졌다는 의미가 아닙니다.
