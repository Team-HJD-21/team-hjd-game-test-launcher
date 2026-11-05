#requires -Version 7.5
[CmdletBinding()]
param(
    [string]$PrivateRoot=(Join-Path (Split-Path $PSScriptRoot) '.local/production-distribution'),
    [switch]$SkipPublicCheck,
    [switch]$ExportCsv
)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'CatalogCommon.ps1')
. (Join-Path $PSScriptRoot 'HostingDashboardAnalytics.ps1')
$root=[IO.Path]::GetFullPath($PrivateRoot);Assert-HostingPath $root
if(-not [IO.Directory]::Exists($root)){throw 'Private hosting root does not exist.'}
$project=Split-Path $PSScriptRoot
$directory=Join-Path $root 'dashboard';Assert-HostingPath $directory
[IO.Directory]::CreateDirectory($directory)|Out-Null
$lock=Open-HostingLock (Join-Path $root 'dashboard-update.lock')
try{
    $zone=[TimeZoneInfo]::FindSystemTimeZoneById('Korea Standard Time')
    function ConvertTo-DashboardHtml([object]$Value){[Net.WebUtility]::HtmlEncode([string]$Value)}
    function Time-Cell([object]$Value){
        try{
            $utc=[DateTimeOffset]::Parse([string]$Value,[Globalization.CultureInfo]::InvariantCulture)
            if($utc.Offset -ne [TimeSpan]::Zero){throw 'Not UTC'}
            '<time datetime="'+(ConvertTo-DashboardHtml $utc.ToString('o'))+'" title="UTC '+(ConvertTo-DashboardHtml $utc.ToString('yyyy-MM-dd HH:mm:ss'))+'">'+(ConvertTo-DashboardHtml ([TimeZoneInfo]::ConvertTime($utc,$zone).ToString('yyyy.MM.dd HH:mm:ss')))+'</time><small>UTC '+(ConvertTo-DashboardHtml $utc.ToString('yyyy-MM-dd HH:mm:ss'))+'</small>'
        }catch{'<span class="muted">기록 없음</span>'}
    }
    function Process-View([string]$Kind,[string]$Relative){
        $view=@{state='stopped';label='중지 / 미등록';tone='muted';pid='—';started='';port=0}
        try{
            $path=Join-Path $root $Relative
            if(-not [IO.File]::Exists($path)){return $view}
            $state=Read-HostingJson $path 16384
            switch($Kind){
                'origin'{
                    $executable=[IO.Path]::GetFullPath([string]$(if($state.executable){$state.executable}else{$state.python}))
                    if([IO.Path]::GetFileName($executable) -ine 'python.exe'){throw 'Wrong origin executable'}
                    $script=Join-Path $project 'server/distribution_server.py'
                    if($state.script -and $state.script -ine $script){throw 'Wrong origin script'}
                    if([int]$state.port -lt 1024 -or [int]$state.port -gt 65535 -or $state.bind -ne '127.0.0.1'){throw 'Wrong origin bind'}
                    $required=@($script,'--root',$root,'--bind','127.0.0.1','--port',[string]$state.port)
                }
                'tunnel'{
                    $executable=[IO.Path]::GetFullPath([string]$state.executable)
                    if([IO.Path]::GetFileName($executable) -ine 'cloudflared.exe'){throw 'Wrong tunnel executable'}
                    Assert-HostingPath $executable
                    if((Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash -ine 'db4e4c413612a0916f3e1679d3caaf376f94911952d163028fb726fb2e0516a9'){throw 'Unverified tunnel executable'}
                    $config=[IO.Path]::GetFullPath([string]$state.config)
                    if($config -ine (Join-Path $root 'cloudflare/config.yml') -and $config -ine (Join-Path $root 'cloudflare/managed-connector.yml')){throw 'Wrong tunnel config'}
                    $required=@('tunnel','--config',$config,'run')
                }
                'monitor'{
                    $executable=if($state.executable){[IO.Path]::GetFullPath([string]$state.executable)}else{(Get-Process -Id $PID).Path}
                    if([IO.Path]::GetFileName($executable) -ine 'pwsh.exe' -or $state.script -ine (Join-Path $PSScriptRoot 'Start-PlaytestHosting.ps1')){throw 'Wrong monitor executable/script'}
                    $required=@($state.script,'-PrivateRoot',$root,'-Monitor')
                }
            }
            Assert-HostingPath $executable
            $owned=Get-OwnedHostingProcess $state $executable $required
            if($owned){
                $view.state='running';$view.label='실행 확인';$view.tone='ok';$view.pid=$owned.Id;$view.started=$owned.StartTime.ToUniversalTime().ToString('o')
                if($Kind -eq 'origin'){$view.port=[int]$state.port}
            }
        }catch{$view.state='unverified';$view.label='확인 불가';$view.tone='warn'}
        return $view
    }
    $origin=Process-View 'origin' 'server-process.json'
    $tunnel=Process-View 'tunnel' 'cloudflare/tunnel-process.json'
    $monitor=Process-View 'monitor' 'hosting-monitor.json'
    $tcpLabel='원본 프로세스 확인 필요';$tcpTone='muted'
    if($origin.state -eq 'running'){
        $tcp=[Net.Sockets.TcpClient]::new()
        try{
            $ownedPort=(Get-NetTCPConnection -State Listen -LocalPort $origin.port -ErrorAction SilentlyContinue).OwningProcess -contains $origin.pid
            if($ownedPort -and $tcp.ConnectAsync('127.0.0.1',$origin.port).Wait(700) -and $tcp.Connected){$tcpLabel='루프백 TCP 연결 확인';$tcpTone='ok'}else{$tcpLabel='루프백 TCP 응답 없음';$tcpTone='warn'}
        }catch{$tcpLabel='루프백 TCP 확인 실패';$tcpTone='warn'}finally{$tcp.Dispose()}
    }
    $publicLabel='미검사';$publicTone='muted'
    if(-not $SkipPublicCheck){
        $handler=[Net.Http.HttpClientHandler]::new();$handler.AllowAutoRedirect=$false;$handler.UseCookies=$false
        $http=[Net.Http.HttpClient]::new($handler);$http.Timeout=[TimeSpan]::FromSeconds(2)
        $request=[Net.Http.HttpRequestMessage]::new([Net.Http.HttpMethod]::Head,'https://playtest.teamhjd.com/catalog.json')
        try{
            $response=$http.Send($request,[Net.Http.HttpCompletionOption]::ResponseHeadersRead)
            try{
                $code=[int]$response.StatusCode
                if($code -in 302,303,307,308){
                    $redirectOk=$true;$accessPath=Join-Path $root 'cloudflare-access.json'
                    if([IO.File]::Exists($accessPath)){
                        $access=Read-HostingJson $accessPath 16384;$location=$response.Headers.Location
                        $redirectOk=($access.enabled -eq $true -and $access.issuer -cmatch '^https://[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\.cloudflareaccess\.com$' -and $location -and $location.IsAbsoluteUri -and $location.Scheme -eq 'https' -and $location.Port -eq 443 -and -not $location.UserInfo -and $location.IdnHost -ieq ([Uri]$access.issuer).IdnHost)
                    }
                    if($redirectOk){$publicLabel='배포 인증 연결 확인';$publicTone='ok'}else{$publicLabel='인증 연결 설정 확인 필요';$publicTone='warn'}
                }elseif($code -eq 200){$publicLabel='인증 없이 응답 · 설정 확인 필요';$publicTone='warn'}else{$publicLabel='배포 주소 응답 · 확인 필요';$publicTone='warn'}
            }finally{$response.Dispose()}
        }catch{$publicLabel='연결 / TLS 확인 실패';$publicTone='warn'}finally{$request.Dispose();$http.Dispose();$handler.Dispose()}
    }
    $catalogLabel='목록 없음';$catalogTone='muted';$sequence='—';$expiry='';$gameRows='';$gameCount=0;$totalBytes=0L;$key=$null
    try{
        if([IO.File]::Exists((Join-Path $root 'catalog.json'))){
            # Verify using only the public publisher point; never decrypt the private signing key.
            $template=Read-HostingJson (Join-Path $root 'invite.json') (256*1024)
            if($template.publisherKey -cnotmatch '^p256:[A-Za-z0-9+/]+={0,2}$'){throw 'Missing publisher public key'}
            $pointBytes=[Convert]::FromBase64String($template.publisherKey.Substring(5));if($pointBytes.Length -ne 64){throw 'Wrong publisher point'}
            $point=[Security.Cryptography.ECPoint]::new();$point.X=[byte[]]$pointBytes[0..31];$point.Y=[byte[]]$pointBytes[32..63]
            $parameters=[Security.Cryptography.ECParameters]::new();$parameters.Curve=[Security.Cryptography.ECCurve+NamedCurves]::nistP256;$parameters.Q=$point
            $key=[Security.Cryptography.ECDsa]::Create();$key.ImportParameters($parameters)
            $catalog=Read-VerifiedCatalog (Join-Path $root 'catalog.json') $key
            $sequence=[long]$catalog.sequence;$expiry=[string]$catalog.expiresAt
            $expires=[DateTimeOffset]::Parse($expiry,[Globalization.CultureInfo]::InvariantCulture)
            if($expires.Offset -ne [TimeSpan]::Zero){throw 'Catalog expiry is not UTC'}
            $catalogLabel=if($expires -gt [DateTimeOffset]::UtcNow){'서명 검증됨'}else{'유효기간 만료'}
            $catalogTone=if($expires -gt [DateTimeOffset]::UtcNow){'ok'}else{'warn'}
            $rows=[Text.StringBuilder]::new()
            foreach($game in $catalog.games){
                $gameCount++
                $variants=@(@{data=$game;label='Windows x64'})
                if($game.platforms){foreach($platform in $game.platforms.Keys){$variants+=@{data=$game.platforms[$platform];label=[string]$platform}}}
                foreach($variant in $variants){
                    $data=$variant.data;$bytes=[long]$data.bytes;if($bytes -lt 0){throw 'Invalid package size'}
                    $totalBytes+=$bytes
                    [void]$rows.Append('<tr><td><strong>'+(ConvertTo-DashboardHtml $data.name)+'</strong><small>'+(ConvertTo-DashboardHtml $game.id)+' · '+(ConvertTo-DashboardHtml $variant.label)+'</small></td><td>'+(ConvertTo-DashboardHtml $data.version)+'</td><td class="number">'+(ConvertTo-DashboardHtml $bytes.ToString('N0',[Globalization.CultureInfo]::InvariantCulture))+' bytes</td></tr>')
                }
            }
            $gameRows=$rows.ToString()
        }
    }catch{$catalogLabel='서명 / 목록 확인 불가';$catalogTone='warn';$sequence='—';$expiry='';$gameRows='';$gameCount=0;$totalBytes=0}finally{if($key){$key.Dispose()}}
    $users=@();$usersLabel='인증된 이메일만 표시';$userRows=''
    $now=[DateTimeOffset]::UtcNow
    $analytics=Get-DashboardAnalytics @() $now
    try{
        $users=@(& (Join-Path $PSScriptRoot 'Get-VerifiedTesters.ps1') -PrivateRoot $root)
        $analytics=Get-DashboardAnalytics $users $now
        $rows=[Text.StringBuilder]::new()
        foreach($record in $analytics.Records){
            $user=$record.User;$count=[long]$user.LoginCount
            $maskedEmail=[regex]::Replace([string]$user.Email,'^(.).*(@.*)$','$1***$2')
            $activity=if($record.Recent){'<span class="badge ok">5분 내 요청</span>'}elseif($record.Today){'<span class="badge">오늘 요청</span>'}else{'<span class="muted">최근 요청 없음</span>'}
            $first=if($null -ne $record.First){$record.First.ToUnixTimeSeconds()}else{0}
            $last=if($null -ne $record.Last){$record.Last.ToUnixTimeSeconds()}else{0}
            [void]$rows.Append('<tr data-tester data-first="'+$first+'" data-last="'+$last+'" data-sessions="'+$count+'" data-today="'+$record.Today.ToString().ToLowerInvariant()+'" data-active7="'+$record.Active7.ToString().ToLowerInvariant()+'" data-active30="'+$record.Active30.ToString().ToLowerInvariant()+'" data-recent="'+$record.Recent.ToString().ToLowerInvariant()+'"><td class="email" data-label="이메일">'+'<span class="email-full">'+(ConvertTo-DashboardHtml $user.Email)+'</span><span class="email-masked">'+(ConvertTo-DashboardHtml $maskedEmail)+'</span></td><td data-label="최근 활동">'+$activity+'</td><td data-label="최초 인증 확인">'+(Time-Cell $user.FirstVerifiedAt)+'</td><td data-label="최근 새 세션">'+(Time-Cell $user.LastVerifiedAt)+'</td><td data-label="최근 요청">'+(Time-Cell $user.LastAccessAt)+'</td><td class="number" data-label="세션 수">'+$count+'</td><td><button class="detail-button" data-person-detail type="button">상세</button></td></tr>')
        }
        $userRows=$rows.ToString()
        if($ExportCsv){& (Join-Path $PSScriptRoot 'Get-VerifiedTesters.ps1') -PrivateRoot $root -CsvPath (Join-Path $directory 'verified-testers.csv') -Force|Out-Null}
    }catch{$users=@();$analytics=Get-DashboardAnalytics @() $now;$userRows='';$usersLabel='인증 기록 확인 불가'}
    $joinChart=[Text.StringBuilder]::new();$chartMaximum=[Math]::Max(1,($analytics.Days.Values|Measure-Object -Maximum).Maximum)
    foreach($day in $analytics.Days.Keys){
        $count=$analytics.Days[$day]
        [void]$joinChart.Append('<div class="chart-row" data-count="'+$count+'"><time datetime="'+$day+'">'+$day.Substring(5).Replace('-','/')+'</time><meter min="0" max="'+$chartMaximum+'" value="'+$count+'" aria-label="'+$day+' 신규 인증 '+$count+'명">'+$count+'</meter><strong>'+$count+'</strong></div>')
    }
    $comparison=$analytics.New7-$analytics.Previous7
    $comparisonLabel=if($comparison -gt 0){'직전 7일보다 '+$comparison+'명 증가'}elseif($comparison -lt 0){'직전 7일보다 '+[Math]::Abs($comparison)+'명 감소'}else{'직전 7일과 같음'}
    $dataWarning=if($usersLabel -eq '인증 기록 확인 불가'){'<p class="notice warn" role="alert">인증 기록을 읽지 못했습니다. 이전에 표시한 테스터 데이터는 유지하며 처음 여는 경우 숫자를 표시하지 않습니다. 비공개 운영 기록을 확인하세요.</p>'}elseif($analytics.InvalidDates){'<p class="notice warn" role="status">시각을 확인할 수 없는 항목 '+$analytics.InvalidDates+'개는 기간별 집계에서 제외했습니다.</p>'}else{''}
    if(-not $gameRows){$gameRows='<tr><td colspan="3" class="empty">확인된 배포 게임이 없습니다.</td></tr>'}
    if(-not $userRows){$userRows='<tr><td colspan="7" class="empty">'+(ConvertTo-DashboardHtml $(if($usersLabel -eq '인증 기록 확인 불가'){$usersLabel}else{'아직 인증 후 접속한 테스터가 없습니다.'}))+'</td></tr>'}
    $csvLink='';$csvPath=Join-Path $directory 'verified-testers.csv';Assert-HostingPath $csvPath
    if([IO.File]::Exists($csvPath)){$csvLink='<a class="button" href="verified-testers.csv" download>CSV 내려받기</a><span class="muted">내보낸 시각 '+(ConvertTo-DashboardHtml ([TimeZoneInfo]::ConvertTime([DateTimeOffset](Get-Item -LiteralPath $csvPath).LastWriteTimeUtc,$zone).ToString('MM.dd HH:mm')))+' KST</span>'}
    $formAction="'none'";$controls='';$mode='LOCAL · PRIVATE · READ ONLY';$exportControl=''
    $autoLabel='자동 시작 확인 필요';$autoTone='warn';$autoEnabled=$false
    try{
        $auto=& (Join-Path $PSScriptRoot 'Enable-HostingAutostart.ps1') -PrivateRoot $root -Action Status
        $autoEnabled=($auto.Enabled -and $auto.RegisteredForThisRoot)
        $autoLabel=if($autoEnabled){'Windows 로그인 시 자동 시작 켜짐'}else{'Windows 로그인 시 자동 시작 꺼짐'}
        $autoTone=if($autoEnabled){'ok'}else{'muted'}
    }catch{}
    try{
        $control=Read-HostingJson (Join-Path $root 'dashboard-control.json') 16384
        $controlScript=Join-Path $project 'server/hosting_control.py'
        if($control.script -ine $controlScript -or $control.privateRoot -ine $root -or [int]$control.port -lt 1024 -or [int]$control.port -gt 65535 -or [IO.Path]::GetFileName([string]$control.executable) -ine 'python.exe'){throw 'Unexpected control identity'}
        $controlProcess=Get-OwnedHostingProcess $control ([string]$control.executable) @($controlScript,'--root',$root,'--port',[string]$control.port)
        if(-not $controlProcess -or -not ((Get-NetTCPConnection -State Listen -LocalPort $control.port -ErrorAction SilentlyContinue).OwningProcess -contains $controlProcess.Id)){throw 'Control listener is unavailable'}
        $tokenPath=Join-Path $root 'dashboard-control-token.txt';Assert-HostingPath $tokenPath
        if((Get-Item -LiteralPath $tokenPath).Length -gt 128){throw 'Invalid management capability'}
        $controlToken=[IO.File]::ReadAllText($tokenPath).Trim()
        if($controlToken -cnotmatch '^[0-9a-f]{64}$'){throw 'Invalid management capability'}
        $controlOrigin='http://127.0.0.1:'+([int]$control.port)
        $formAction=$controlOrigin
        function Control-Form([string]$Action,[string]$Label,[bool]$Disabled=$false,[string]$Kind=''){
            '<form data-control method="post" action="'+$controlOrigin+'/control"><input type="hidden" name="token" value="'+$controlToken+'"><input type="hidden" name="action" value="'+$Action+'"><button class="button '+$Kind+'" type="submit"'+$(if($Disabled){' disabled'}else{''})+'>'+ (ConvertTo-DashboardHtml $Label)+'</button></form>'
        }
        $allRunning=($origin.state -eq 'running' -and $tunnel.state -eq 'running' -and $monitor.state -eq 'running')
        $allStopped=($origin.state -eq 'stopped' -and $tunnel.state -eq 'stopped' -and $monitor.state -eq 'stopped')
        $controls='<section class="panel controls"><div class="section-head"><h2>서버 제어</h2><span class="badge '+$autoTone+'">'+$autoLabel+'</span></div><div class="control-buttons">'+(Control-Form 'start' '서버 시작' $allRunning 'primary')+(Control-Form 'stop' '서버 중지' $allStopped 'danger')+(Control-Form 'restart' '다시 시작' $allStopped)+'</div><p class="muted">중지하면 다운로드가 끊길 수 있습니다. 이 관리 화면은 계속 열어둘 수 있습니다.</p><details class="settings"><summary>로그인 자동 시작 설정</summary><p class="muted">Windows 사용자 로그인 후 시작합니다. 절전·로그아웃·인터넷 단절 중에는 배포되지 않습니다.</p>'+(Control-Form $(if($autoEnabled){'auto-disable'}else{'auto-enable'}) $(if($autoEnabled){'자동 시작 끄기'}else{'자동 시작 켜기'}))+'</details></section>'
        $exportControl=Control-Form 'export' 'CSV 만들기'
        $mode='LOCAL · PRIVATE · 이 PC에서만 제어'
    }catch{$controls='<section class="panel"><h2>서버 제어 준비 필요</h2><p class="muted">Open-HostingDashboard.ps1로 대시보드를 열면 이 PC 전용 제어가 준비됩니다. 자동 시작 상태: '+$autoLabel+'</p></section>'}
    function Process-Card([string]$Kind,[string]$Title,[hashtable]$View){
        '<article class="card" data-component="'+$Kind+'" data-state="'+$View.state+'"><div class="card-label">'+$Title+'</div><span class="badge '+$View.tone+'">'+$View.label+'</span><p class="process">PID '+(ConvertTo-DashboardHtml $View.pid)+'</p><div class="muted">'+$(if($View.started){'시작 '+(Time-Cell $View.started)}else{'저장된 PID · 시작 시각 · 실행 파일 · 인수를 대조합니다.'})+'</div></article>'
    }
    $processCards=(Process-Card 'origin' '원본 서버' $origin)+(Process-Card 'tunnel' 'Cloudflare 터널' $tunnel)+(Process-Card 'monitor' '자동 복구 감시' $monitor)
    $generated=[DateTimeOffset]::UtcNow
    $testersValid=if($usersLabel -eq '인증 기록 확인 불가'){'false'}else{'true'}
    if($testersValid -eq 'false'){
        foreach($metric in 'Total','Today','Returning','New7','Recent','Active7','Active30','NewToday','Sessions'){$analytics[$metric]='—'}
        $joinChart='<p class="muted">인증 기록을 읽지 못해 추이를 표시하지 않습니다.</p>';$comparisonLabel='기록 확인 필요'
    }
    $summaryLabel=if($origin.state -eq 'running' -and $tunnel.state -eq 'running' -and $monitor.state -eq 'running' -and $tcpTone -eq 'ok'){'서버 구성 요소 실행 중'}elseif($origin.state -eq 'stopped' -and $tunnel.state -eq 'stopped' -and $monitor.state -eq 'stopped'){'서버 중지됨'}else{'서버 상태 확인 필요'}
    $summaryTone=if($summaryLabel -eq '서버 구성 요소 실행 중'){'ok'}elseif($summaryLabel -eq '서버 중지됨'){'muted'}else{'warn'}
    $css=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'hosting-dashboard/styles.css'),[Text.Encoding]::UTF8).Replace("`r`n","`n")
    $js=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'hosting-dashboard/app.js'),[Text.Encoding]::UTF8).Replace("`r`n","`n")
    $styleHash=[Convert]::ToBase64String([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($css)))
    $scriptHash=[Convert]::ToBase64String([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($js)))
    $iconPath=Join-Path $project 'website/assets/favicons/v1/launcher/favicon-32.png';Assert-HostingPath $iconPath
    $iconBase64=[Convert]::ToBase64String([IO.File]::ReadAllBytes($iconPath))
    $html=@"
<!doctype html><html lang="ko"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><noscript><meta http-equiv="refresh" content="15"></noscript><meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src 'sha256-$styleHash'; script-src 'sha256-$scriptHash'; img-src data:; base-uri 'none'; form-action $formAction; connect-src $formAction; object-src 'none'"><title>TeamHJD Game Tester 관리</title><link rel="icon" type="image/png" sizes="32x32" href="data:image/png;base64,$iconBase64"><style>$css</style></head><body><a class="skip-link" href="#content">본문으로 건너뛰기</a><main id="content">
<header><div><div class="brand">TEAMHJD / GAME TESTER</div><h1>TeamHJD Game Tester 관리</h1><p class="muted">테스터와 게임 배포를 한곳에서 관리합니다. <span>$mode</span></p></div><div class="stamp" id="generated" data-live><div class="muted">마지막 성공 갱신 · KST</div>$(Time-Cell $generated.ToString('o'))</div></header>
<div class="toolbar"><div id="server-summary" data-live><span class="badge $summaryTone">$summaryLabel</span></div><button class="primary js-only" id="refresh-now" type="button">지금 새로고침</button><button id="pause-refresh" class="js-only" aria-pressed="false" type="button">자동 갱신 일시 정지</button><span class="spacer"></span><span id="refresh-status" class="muted" role="status">30초마다 자동 갱신</span></div>
<div id="feedback" data-live><!--CONTROL-FEEDBACK--></div><div id="data-warning" data-live>$dataWarning</div>
<noscript><p class="no-script">스크립트가 꺼져 있어 전체 목록을 표시합니다. 15초마다 새로고침하며 서버 제어는 일반 폼으로 동작합니다.</p></noscript>
<nav class="view-tabs" aria-label="관리 메뉴"><a href="#overview" data-nav="overview" aria-current="page">요약·통계</a><a href="#testers" data-nav="testers">테스터 목록</a><a href="#games" data-nav="games">게임 배포</a><a href="#hosting" data-nav="hosting">서버 관리</a></nav>
<section data-view="overview" id="overview" aria-labelledby="overview-title"><h2 id="overview-title">테스트 현황</h2>
<div class="grid" id="tester-metrics" data-live><article class="card"><div class="card-label">보관 중인 인증 테스터</div><strong class="metric-value">$($analytics.Total)<small>명</small></strong><small>최근 접속 후180일 보관 · 인증된 API 요청</small><a href="#testers" data-nav="testers" data-period="all">테스터 보기 →</a></article><article class="card"><div class="card-label">오늘 요청한 테스터</div><strong class="metric-value">$($analytics.Today)<small>명</small></strong><small>KST 기준 · 재방문 $($analytics.Returning)명</small><a href="#testers" data-nav="testers" data-period="today">오늘 요청 목록 →</a></article><article class="card"><div class="card-label">최근 7일 신규 인증</div><strong class="metric-value">$($analytics.New7)<small>명</small></strong><small>$comparisonLabel · 최근 168시간</small></article><article class="card"><div class="card-label">최근 5분 내 요청</div><strong class="metric-value">$($analytics.Recent)<small>명</small></strong><small>실시간 동시 접속자 수와 다릅니다</small><a href="#testers" data-nav="testers" data-period="recent">최근 요청 목록 →</a></article></div>
<div class="insight-grid"><section class="panel"><div class="section-head"><h2>신규 인증 추이</h2><label class="inline-label">기간<select id="growth-range"><option value="7">최근 7일</option><option value="30">최근 30일</option></select></label></div><p class="muted" id="chart-total"></p><div class="chart" id="join-chart" data-live>$joinChart</div><p class="muted">최초 인증이 확인된 날짜별 인원 · KST</p></section><section class="panel"><h2>요청·세션 현황</h2><div class="split-stats" id="activity-metrics" data-live><div><span>최근 7일 요청</span><strong>$($analytics.Active7)명</strong></div><div><span>최근 30일 요청</span><strong>$($analytics.Active30)명</strong></div><div><span>오늘 신규 인증</span><strong>$($analytics.NewToday)명</strong></div><div><span>관측된 인증 세션</span><strong>$($analytics.Sessions)</strong></div></div><p class="scope-note">기간별 요청 인원은 각 이메일의 마지막 요청 시각으로 계산합니다. 게임 플레이·다운로드 완료·과거 일별 접속 횟수는 수집하지 않습니다.</p></section></div>
<p class="scope-note">현재 보관 중인 인증 기록 기준입니다. Cloudflare 인증만 끝내고 게임 API에 요청하지 않은 사용자는 포함되지 않습니다. 인증 대기자와 메일 발송 수는 이 도구에서 확인할 수 없습니다.</p></section>
<section data-view="testers" data-valid="$testersValid" id="testers" aria-labelledby="testers-title"><div class="panel"><div class="section-head"><div><h2 id="testers-title">인증된 테스터</h2><p class="muted">검색과 필터는 화면 목록에 적용합니다. CSV는 현재 보관 중인 인증 명단입니다. 최근 접속 후180일이 지나거나 삭제 요청으로 정리된 기록은 포함하지 않습니다.</p></div><div class="foot-links" id="csv-controls" data-live>$exportControl $csvLink</div></div>
<div class="filters js-only"><label>이메일 검색<input type="search" id="tester-search" placeholder="이메일로 검색" autocomplete="off"></label><label>최근 요청<select id="tester-period"><option value="all">전체</option><option value="recent">5분 내</option><option value="today">오늘 · KST</option><option value="active7">최근 7일</option><option value="active30">최근 30일</option></select></label><label>정렬<select id="tester-sort"><option value="last">최근 요청순</option><option value="first">최근 가입순</option><option value="sessions">세션 많은 순</option><option value="email">이메일순</option></select></label><button id="reset-filters" type="button">초기화</button><button id="reveal-emails" type="button" aria-pressed="false">이메일 전체 보기</button></div>
<div class="table-wrap tester-table"><table><thead><tr><th>이메일</th><th>최근 활동</th><th>최초 인증 확인</th><th>최근 새 세션</th><th>최근 요청</th><th class="number">세션 수</th><th>상세</th></tr></thead><tbody id="tester-rows" data-live>$userRows</tbody></table><p id="filter-empty" class="empty" hidden>조건에 맞는 테스터가 없습니다. 검색어나 기간을 바꿔보세요.</p></div><div class="pagination js-only"><span id="list-count" class="muted" aria-live="polite"></span><button id="previous-page" type="button">이전</button><button id="next-page" type="button">다음</button></div><p class="scope-note">시각은 KST, 작은 글씨는 UTC 원본입니다. 세션 수는 서로 다른 인증 세션이며 요청 횟수나 기기 수가 아닙니다.</p></div></section>
<section data-view="games" id="games" aria-labelledby="games-title"><div class="panel" id="game-data" data-live><div class="section-head"><h2 id="games-title">게임 배포 목록</h2><span class="badge $catalogTone">$catalogLabel</span></div><p class="muted">게임 $gameCount개 · 목록 순서 $sequence · 유효기간 $(Time-Cell $expiry)</p><div class="table-wrap"><table><thead><tr><th>게임 · 플랫폼</th><th>버전</th><th class="number">패키지 크기</th></tr></thead><tbody>$gameRows</tbody></table></div><p class="scope-note">공개키로 검증한 배포 목록입니다. 서명된 목록에 있는 버전이 표시되며 실제 설치·게임 실행 상태는 수집하지 않습니다.</p></div></section>
<section data-view="hosting" id="hosting" aria-labelledby="hosting-title"><h2 id="hosting-title">서버 관리</h2><div id="hosting-controls" data-live>$controls</div><div class="grid components" id="process-cards" data-live>$processCards</div><div class="connections" id="connection-status" data-live><span class="badge $tcpTone">$tcpLabel</span><span class="badge $publicTone">$publicLabel</span></div><p class="scope-note">인증 게이트 응답은 로그인 후 다운로드 성공을 의미하지 않습니다. 서버·터널·감시의 실행 정보와 원본 TCP를 각각 확인합니다.</p><details class="panel"><summary>운영 도움말</summary><p>PC에 같은 Windows 사용자로 로그인되어 있고, 절전이 해제되어 있으며 인터넷이 연결되어 있어야 합니다. 서버 중지는 게임 다운로드만 중단하며 관리 화면은 유지합니다.</p><p class="muted">기록: private root의 verified-users.json · 운영 로그: runtime-logs/ · 관리 서비스: 127.0.0.1:8844</p></details></section>
<footer><span>TeamHJD · 이 PC 전용 비공개 관리 도구</span><span>이메일·CSV·생성 HTML은 외부에 공유하지 마세요.</span></footer>
<dialog id="person-dialog" aria-labelledby="person-name"><div class="dialog-head"><h2 id="person-name">테스터 상세</h2><form method="dialog"><button>닫기</button></form></div><dl id="person-fields"></dl><p class="scope-note">인증된 요청 기록입니다. 게임 실행 여부와 기기 정보는 포함하지 않습니다.</p></dialog>
<dialog id="confirm-dialog" aria-labelledby="confirm-title"><h2 id="confirm-title">관리 작업 확인</h2><p id="confirm-message"></p><div class="actions"><form method="dialog"><button>취소</button></form><button type="button" class="danger" id="confirm-apply">계속 진행</button></div></dialog>
</main><script>$js</script></body></html>
"@
    $path=Join-Path $directory 'index.html';Assert-HostingPath $path
    $temporary=$path+'.'+[Guid]::NewGuid().ToString('N')+'.tmp'
    try{[IO.File]::WriteAllText($temporary,$html,[Text.UTF8Encoding]::new($false));[IO.File]::Move($temporary,$path,$true)}finally{if([IO.File]::Exists($temporary)){[IO.File]::Delete($temporary)}}
    [pscustomobject]@{DashboardPath=$path;GeneratedAt=$generated.ToString('o');VerifiedTesterCount=$users.Count;GameCount=$gameCount}
}finally{$lock.Dispose()}
