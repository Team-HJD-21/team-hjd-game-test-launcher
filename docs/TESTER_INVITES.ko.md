# 개인별 테스트 초대

테스터마다 **만료되는 초대 파일 한 개**를 발행합니다. 운영자 공용 암호를 팀원 모두에게 나눠줄 필요가 없고, 추가 테스터도 새 파일을 받아 가져오면 됩니다. 발행·회수 도구는 PowerShell **7.5 이상**을 사용합니다. 게임을 실행하는 팀원에게 PowerShell은 필요하지 않습니다.

## 발행

기존 `New-Publisher.ps1`로 준비한 private root의 `invite.json`을 사용합니다. 실제 암호나 토큰을 명령줄에 입력하지 않습니다.

```powershell
./tools/New-TestInvite.ps1 -PrivateRoot D:/Private/TeamHJDDistribution -Label '테스터 A'
./tools/New-TestInvite.ps1 -PrivateRoot D:/Private/TeamHJDDistribution -Label '추가 테스터 B' -Days 2
```

기본 유효기간은 7일이며 1~30일 범위에서 정합니다. 출력은 생성된 파일의 전체 경로와 `TokenId`뿐입니다. 파일은 `team-invites/tester-<ID>.hjdinvite`에 생성됩니다. 서명키·운영 암호는 읽거나 복사하지 않습니다.

생성된 **각자의 `.hjdinvite` 파일**을 신뢰할 수 있는 개인 채널로 전달하세요. 개인 초대 가져오기를 지원하는 런처에서 파일을 가져오면 서버 주소, 배포자 공개키, 선택적인 CA 인증서와 개인 접근 토큰을 함께 적용할 수 있습니다. 초대 파일은 접근 권한을 담으므로 공개 GitHub, 공개 게시판 또는 여러 사람이 쓰는 공용 첨부함에 올리지 않습니다.

## 회수

```powershell
./tools/Revoke-TestInvite.ps1 -PrivateRoot D:/Private/TeamHJDDistribution -TokenId <발행할_때_출력된_ID>
```

서버 재시작 없이 다음 요청부터 거절됩니다. 다른 테스터의 초대와 기존 운영자 암호는 그대로 동작합니다. 만료된 테스터에게는 새 초대를 발행하고 이전 ID를 회수하면 됩니다. 이미 회수된 ID에 다시 실행해도 같은 회수 상태를 유지합니다.

회수·만료는 **새 목록/다운로드 요청에 적용**됩니다. 이미 승인되어 진행 중인 한 번의 다운로드를 강제로 끊거나, 이미 받은 게임·저장 데이터를 지우지는 않습니다. 기존 오프라인 실행 기능도 유지됩니다.

## 저장과 확인

- `testers.json` schema 1의 `testers` 배열에는 `tokenId`, `tokenSha256`, `expiresAt`, `revoked`와 수신자 라벨·발행/회수 시각을 저장합니다. **원본 토큰은 저장하지 않고 전체 토큰 UTF-8의 SHA-256만 보관**합니다.
- 각 초대에는 `schema: 1`, 기존 서버·배포자 키·선택 인증서, `accessToken`, `tokenId`, UTC `expiresAt`, `recipientLabel`이 들어갑니다. 원본 토큰은 32바이트 난수 비밀값을 포함하는 `hjd1.<ID>.<secret>` 형식입니다.
- 발행/회수는 같은 `testers.lock` 파일을 독점 열어 운영자 간 충돌을 막고, 새 JSON을 완성한 후 원자적으로 교체합니다. 기존 테스터와 추가 메타데이터를 보존합니다. 다른 작업이 잠금을 잡고 있으면 변경하지 않고 종료하므로 해당 작업이 끝난 뒤 다시 실행하세요.
- 서버는 매 요청 레지스트리를 제한된 크기로 다시 읽고, 해시를 상수 시간 비교한 뒤 UTC 만료와 회수 상태를 검사합니다. 파일이 없거나 손상·중복 ID·형식 오류가 있으면 개인 토큰은 거절합니다. 레지스트리는 최대 2 MiB, 최대 2,000개 항목입니다.
- 초대 파일과 레지스트리는 HTTP 경로로 노출하지 않습니다. 서버 로그에도 토큰, 운영 암호 또는 수신자 이름을 출력하지 않습니다. 기존 잘못된 암호 시도 제한도 개인 토큰 요청에 적용됩니다.

`python tests/test_server.py`는 운영 폴더와 무관한 `.local/server-test-UUID` 합성 자료로 유효/만료/회수/오류 토큰, 서버 재시작 없는 레지스트리 교체, 크기/개수 제한, 발행·회수 도구와 잠금, 메타데이터 보존을 검증합니다.
