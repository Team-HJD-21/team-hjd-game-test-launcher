# Cloudflare 이메일 인증과 로컬 참여 기록

이메일 허용목록을 두지 않고, Cloudflare Access가 이메일 One-time PIN 인증을 마친 사용자를 받는 구성입니다. **Cloudflare 앱의 로그인 정책은 OTP 전용으로 유지**합니다. 서버는 이메일 헤더나 JWT 내용을 읽기만 하고 신뢰하지 않으며, 원본 JWT를 운영 로그에 남기지 않습니다.

## Python과 서버 설정

Cloudflare를 사용하지 않는 서버는 기존 전역 Python으로도 실행됩니다. `cloudflare-access.json`이 없는 경우 JWT/암호화 패키지를 불러오지 않습니다. 활성화할 때에는 프로젝트 전용 가상환경을 사용하세요.

```powershell
python -m venv .deps/server-venv
./.deps/server-venv/Scripts/python.exe -m pip install --no-cache-dir -r server/requirements.txt
./.deps/server-venv/Scripts/python.exe server/distribution_server.py --root D:/Private/TeamHJDDistribution --bind 127.0.0.1 --port 8443 --cert D:/Private/tls/server-chain.pem --key D:/Private/tls/server-key.pem
```

개인 배포 폴더에 아래 **세 필드**의 `cloudflare-access.json`을 둡니다. `issuer`는 Cloudflare Zero Trust 팀 도메인, `audience`는 해당 앱의 정확한 AUD 태그입니다. `/`를 붙이거나 앱의 실제 호스트 주소로 issuer를 바꾸지 않습니다.

```json
{
  "enabled": true,
  "issuer": "https://your-team.cloudflareaccess.com",
  "audience": "APPLICATION_AUD_TAG_FROM_CLOUDFLARE"
}
```

유효한 활성 설정이 있는데 의존성이 없으면 시작 시 명확하게 실패합니다. 기존 전역 Python 서버에 설정 파일만 나중에 추가하는 대신, 의존성을 준비한 가상환경으로 정상 재시작하세요. 실행 중 설정 비활성화·삭제·오류는 다음 요청부터 Cloudflare 인증을 거절합니다. 운영자 암호와 개인 만료 초대의 기존 인증은 유지됩니다.

Cloudflare Tunnel이 origin으로 전달한 `Cf-Access-Jwt-Assertion` **헤더 하나만** 받습니다. `CF_Authorization` 쿠키와 자체 이메일 헤더를 origin에서 직접 신뢰하지 않습니다. 브라우저/클라이언트의 Cloudflare 쿠키는 edge에 전달하고, edge가 JWT 헤더를 추가하는 경로를 사용합니다.

## 검증 경계

- 고정된 HTTPS `*.cloudflareaccess.com` issuer의 `/cdn-cgi/access/certs`만 조회합니다. JWT의 `jku`, `x5u` 또는 다른 URL은 사용하지 않습니다. 키 조회의 TLS/호스트 검증은 활성이고 리다이렉트는 거절합니다.
- RS256만 허용하며 고정 issuer·정확한 단일 앱 audience·서명·`exp`/`nbf`/`iat`·`type: app`·사용자 `sub`·이메일 형식을 검증합니다. 정상 Cloudflare app JWT에는 `email_verified`가 없어도 서명된 이메일을 사용합니다. 이 필드가 명시적으로 false인 경우는 거절합니다.
- JWKS 응답은 256 KiB/16키, RSA는 2048~8192비트로 제한합니다. 키는 5분 동안 캐시하고 재조회 간격은 최소 15초, 네트워크 timeout은 5초입니다. 새 키 ID는 제한된 재조회를 유도합니다. 조회 실패 시 TTL 안의 일치 키만 계속 사용하며 만료 캐시로 통과시키지 않습니다.
- JWT는 최대 16 KiB, 시각 오차는 30초, 세션 길이는 최대 31일입니다. 서비스 토큰이나 전역 `org` 토큰은 이메일 참여자로 받지 않습니다. 유효한 이메일 JWT는 별도 운영자 암호 없이 인증됩니다.

## 로컬 사용자 기록

서명 검증에 성공한 요청만 같은 private root의 `verified-users.json`에 원자적으로 기록합니다. `verified-users.lock`은 스레드·프로세스 간 쓰기 충돌을 막습니다. 파일은 HTTP 경로로 공개되지 않습니다.

`schema: 1`, `users` 배열의 각 항목에는 다음이 들어갑니다.

- `email`: 인증된 이메일
- `firstVerifiedAt`, `lastVerifiedAt`, `lastAccessAt`: 서버가 확인한 UTC 시각
- `loginCount`: 서버가 관측한 서로 다른 유효 JWT 세션 수
- `lastSessionHash`: 마지막 세션의 SHA-256
- `source: cloudflare-access-otp`
- `activeSessions`: 유효기간이 남은 세션의 해시와 만료 시각만 보관하는 내부 중복 방지 목록

같은 JWT로 목록·다운로드를 여러 번 요청하거나 A → B → A 순서로 재사용해도 로그인 수가 부풀지 않습니다. 재시작 후에도 같은 활성 해시를 기억합니다. `loginCount`는 OTP 이메일 전송 수가 아닙니다. 원본 JWT, IP 주소 또는 다른 IdP 개인정보는 저장하지 않습니다.

기록 한도는 8 MiB/10,000명, 사용자별 활성 세션은 64개입니다. 저장 파일이 손상됐거나 쓸 수 없으면 Cloudflare 요청에 503을 반환하고 기록 없이 조용히 허용하지 않습니다. 기존 운영자/개인 초대 인증은 관리용으로 유지됩니다.

## 검사와 근거

```powershell
./.deps/server-venv/Scripts/python.exe -m unittest discover -s tests -p 'test_*.py' -v
```

합성 RSA 키와 JWKS/HTTP 서버로 서명·issuer/aud/alg·시각·이메일 누락, 키 조회 실패/캐시 만료, 외부 키 URL 무시, 원자적 참여 기록과 세션 중복 방지, 실제 origin 인증을 확인합니다. 기존 개인 초대와 운영자 인증 검사도 함께 실행합니다. 운영 설정·실제 계정 로그인·서버 재시작은 이 합성 검사에 포함되지 않습니다.

공식 근거: [Cloudflare JWT 검증](https://developers.cloudflare.com/cloudflare-one/access-controls/applications/http-apps/authorization-cookie/validating-json/), [app JWT의 인증된 이메일·시각 필드](https://developers.cloudflare.com/cloudflare-one/access-controls/applications/http-apps/authorization-cookie/application-token/). 고정 의존성은 공식 [PyJWT](https://pypi.org/project/PyJWT/2.15.1/)와 [cryptography](https://pypi.org/project/cryptography/50.0.2/) 배포를 사용합니다.
