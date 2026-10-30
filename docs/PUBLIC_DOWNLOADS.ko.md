# 공개 런처 다운로드 사이트

## 역할과 진입점

`https://testlauncher.teamhjd.com/`은 Windows x64, Apple Silicon Mac, Intel Mac용 **공개 런처 ZIP**을 받는 정적 페이지다. 대표 사이트 `https://teamhjd.com/`에서는 **푸터 → 관련 사이트 → Game Test Launcher**에만 연결한다. 기존 GitHub Notify·팀 개발 문서 항목, header·hero·본문은 보존한다. 한국어/English/简体中文의 `?lang=ko/en/zh-CN` 선택을 전달한다.

푸터만을 진입 링크로 제공하는 것은 접근 인증 정책이 아니다. 다운로드 페이지는 공개이며 주소를 알면 직접 열 수 있다. 검색 노출은 `noindex,nofollow`로 억제하고 sitemap에는 넣지 않는다. 공개 런처를 받는 데 이메일 인증을 요구하지 않는다. **비공개 게임 설치·업데이트는 런처에서 기존 이메일 인증을 거친다.**

## 두 도메인을 혼동하지 않는다

| 주소 | 용도 |
| --- | --- |
| `testlauncher.teamhjd.com` | GitHub Pages의 공개 런처 소개·다운로드. 운영 PC가 꺼져도 제공됨 |
| `playtest.teamhjd.com/catalog.json` | 기존 Cloudflare Access/Tunnel을 거치는 서명된 게임 카탈로그 |
| `playtest.teamhjd.com/packages/<sha256>.zip` | 인증된 정본 게임 ZIP과 이어받기 |
| `playtest.teamhjd.com/`, `/downloads` | 수동 게임 다운로드 HTML 종료. 인증 후 빈 HTTP410, 링크/redirect 없음 |

`playtest`의 DNS·Access 앱·Tunnel을 삭제하거나 `DefaultChannel.h`의 서버를 공개 사이트로 바꾸지 않는다. 루트 HTML을 쓰지 않는 cloudflared 로그인 경계와 회귀 근거는 [운영 가이드](PLAYTEST_HOSTING.ko.md)에 있다. 익명 요청은 여전히 edge의 OTP 로그인으로302이므로, 페이지 종료 여부는 인증된 응답까지 확인한다. 기존 서명된 catalog와 게임 API는 별도로 검증한다.

## 최신 파일 연결 계약

세 버튼은 다음 공식 링크를 사용한다. 버전 번호를 HTML에 고정하지 않는다.

```text
https://github.com/Team-HJD-21/team-hjd-game-test-launcher/releases/latest/download/TeamHJD-GameTestLauncher-Windows-x64.zip
https://github.com/Team-HJD-21/team-hjd-game-test-launcher/releases/latest/download/TeamHJD-GameTestLauncher-macOS-arm64.zip
https://github.com/Team-HJD-21/team-hjd-game-test-launcher/releases/latest/download/TeamHJD-GameTestLauncher-macOS-x64.zip
```

이 `latest/download/<고정 파일명>` 계약은 [GitHub 공식 안내](https://docs.github.com/en/repositories/releasing-projects-on-github/linking-to-releases)를 따른다. 새 stable Release는 이 세 portable ZIP을 **모두** 포함해야 한다. 자동 업데이트용 `LauncherPayload-*`는 웹 다운로드 버튼으로 제공하지 않는다. 파일명을 변경하거나 일부 플랫폼을 생략하면 최신 링크가 깨질 수 있으므로 [릴리즈 체크리스트](RELEASING.ko.md)를 따른다.

페이지에는 제품명, 플레이테스트용 런처라는 한 줄 설명, 세 다운로드 버튼과 버전·게시일만 표시한다. 버튼 아래에는 **런처의 이메일 인증 시 인증된 이메일 주소와 인증·접속 시각이 수집·보관된다**는 안내를 둔다. 마케팅 문구·앱 모형·설치 절차·FAQ를 추가하지 않는다. 이 간결한 구성은 2026-10-31 사용자가 직접 요청했다.

페이지 JS는 공개 latest API에서 stable 태그, 정확한 세 파일명·공식 URL·크기를 검증하고 버전·게시일을 `textContent`로 표시한다. 토큰/쿠키를 GitHub API에 보내지 않으며 **이 웹페이지 자체는** 이메일이나 로그를 수집하지 않는다. 위 수집 안내의 대상은 런처의 이메일 인증이다. API 제한·오프라인·JS 비활성화가 있어도 기본 다운로드 링크는 HTML에 남아 작동한다. 버전 표시를 얻기 위해 비공개 인증정보를 브라우저에 넣지 않는다.

## 소스와 배포

- 런처 저장소 `website/`: HTML/CSS/JS, robots, 도메인 표기와 기존 회사 로고 두 개. 게임 파일이나 스크린샷 원본은 넣지 않는다.
- `tools/check-download-site.mjs`: 공개 파일 allowlist, 세 최신 portable 링크, 비공개/업데이터 링크 유입, 토큰 없는 metadata rendering 검증.
- `.github/workflows/download-site.yml`: `website/` 관련 main push 또는 수동 실행으로 검사한 **website 폴더만** GitHub Pages 배포. 기존 native Mac 릴리즈 workflow와 별개다.
- Pages custom domain은 **런처 저장소**에 `testlauncher.teamhjd.com`. 대표 사이트 저장소 `team-hjd-site`의 `cname=teamhjd.com`을 덮어쓰지 않는다.
- Cloudflare DNS의 새 `testlauncher` CNAME → `team-hjd-21.github.io`, DNS only. 인증서 승인 뒤 이 Pages 사이트의 HTTPS 강제를 켜고 실제 HTTPS/HTTP→HTTPS 응답을 검증한다. 메일/MX/TXT/다른 호스트는 변경하지 않는다.
- 대표 사이트 footer는 별도 `Team-HJD-21/team-hjd-site` 저장소다. 최신 remote main을 기준으로 변경하고, `scripts/render-team.mjs`로 팀 프로필 footer를 재생성한 뒤 기존 check/test/build를 실행한다. 이전 로컬 clone에 없던 GitHub Notify 항목을 덮어쓰지 않는다.

```powershell
node --check website/app.js
node tools/check-download-site.mjs
gh workflow run download-site.yml --ref main
# 위 dispatch가 만든 실제 run ID를 선택해 확인:
gh run list --workflow download-site.yml --branch main --limit 5 --json databaseId,headSha,status,conclusion
```

프로덕션 반영 전 loopback preview에서 데스크톱·좁은 모바일, 언어3개, 이메일 수집 안내와 실제 metadata 표시를 확인한다. 대표 사이트에서는 푸터 메뉴의 링크와 언어 전달을 확인한다. 공식 latest 링크는 리다이렉트를 따라 실제 stable 태그의 파일명·크기와 대조한다. URL만 바뀌고 파일이 없는 상태를 성공으로 표시하지 않는다. Pages·대표 사이트 배포 성공, 새 도메인 TLS와 구 포털410/기존API 성공을 각각 구분해 증거를 남긴다.

## 유지보수 주의

새 런처 Release마다 사이트의 버전 문자열을 수동 변경하거나 전체 런처를 다시 빌드하지 않는다. 사이트 코드 변경은 Pages 배포, 서버의 포털 종료 코드는 소유권을 검증한 origin 재시작이 각각 필요하다. 이미 공개한 v0.1.3 ZIP/태그를 문서 링크 변경 때문에 덮어쓰지 않는다. 예전 ZIP에 동봉된 수동 다운로드 페이지 안내는 당시 운영 기록이며 최신 웹 가이드를 우선한다.

브랜드 색과 로고는 대표 사이트에서 재사용하고 footer는 홈페이지·GitHub·개인정보처리방침 링크로 간결하게 유지한다. 상세 Mac 첫 실행 승인과 ZIP 전체 보관·안정된 bootstrap 고정 안내는 사용 가이드에 유지한다. 사이트 공개 성공을 Mac 게임 플레이·크로스플레이 검증으로 확대하지 않는다.
