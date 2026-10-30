#requires -Version 7.5
[CmdletBinding()]
param(
    [string]$PrivateRoot=(Join-Path (Split-Path $PSScriptRoot) '.local/production-distribution'),
    [switch]$SkipPublicCheck,
    [switch]$ExportCsv
)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'CatalogCommon.ps1')
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
    $users=@();$usersLabel='인증된 이메일만 표시';$userRows='';$sessionCount=0L;$recentCount=0
    try{
        $users=@(& (Join-Path $PSScriptRoot 'Get-VerifiedTesters.ps1') -PrivateRoot $root)
        $rows=[Text.StringBuilder]::new()
        foreach($user in $users){
            $count=[long]$user.LoginCount;if($count -lt 0){throw 'Invalid login count'};$sessionCount+=$count
            $activity='<span class="muted">최근 요청 없음</span>'
            try{
                $last=[DateTimeOffset]::Parse([string]$user.LastAccessAt,[Globalization.CultureInfo]::InvariantCulture)
                $now=[DateTimeOffset]::UtcNow
                if($last.Offset -eq [TimeSpan]::Zero -and $last -le $now -and $last -ge $now.AddMinutes(-5)){$recentCount++;$activity='<span class="badge ok">5분 내 요청</span>'}
            }catch{}
            [void]$rows.Append('<tr><td class="email">'+(ConvertTo-DashboardHtml $user.Email)+'</td><td>'+$activity+'</td><td>'+(Time-Cell $user.FirstVerifiedAt)+'</td><td>'+(Time-Cell $user.LastVerifiedAt)+'</td><td>'+(Time-Cell $user.LastAccessAt)+'</td><td class="number">'+(ConvertTo-DashboardHtml $count)+'</td></tr>')
        }
        $userRows=$rows.ToString()
        if($ExportCsv){& (Join-Path $PSScriptRoot 'Get-VerifiedTesters.ps1') -PrivateRoot $root -CsvPath (Join-Path $directory 'verified-testers.csv') -Force|Out-Null}
    }catch{$users=@();$sessionCount=0;$recentCount=0;$userRows='';$usersLabel='인증 기록 확인 불가'}
    if(-not $gameRows){$gameRows='<tr><td colspan="3" class="empty">확인된 배포 게임이 없습니다.</td></tr>'}
    if(-not $userRows){$userRows='<tr><td colspan="6" class="empty">'+(ConvertTo-DashboardHtml $(if($usersLabel -eq '인증 기록 확인 불가'){$usersLabel}else{'아직 인증 후 접속한 테스터가 없습니다.'}))+'</td></tr>'}
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
            '<form method="post" action="'+$controlOrigin+'/control"><input type="hidden" name="token" value="'+$controlToken+'"><input type="hidden" name="action" value="'+$Action+'"><button class="button '+$Kind+'" type="submit"'+$(if($Disabled){' disabled'}else{''})+'>'+ (ConvertTo-DashboardHtml $Label)+'</button></form>'
        }
        $allRunning=($origin.state -eq 'running' -and $tunnel.state -eq 'running' -and $monitor.state -eq 'running')
        $allStopped=($origin.state -eq 'stopped' -and $tunnel.state -eq 'stopped' -and $monitor.state -eq 'stopped')
        $controls='<section class="panel controls"><div class="section-head"><h2>서버 제어</h2><span class="badge '+$autoTone+'">'+$autoLabel+'</span></div><div class="control-buttons">'+(Control-Form 'start' '서버 시작' $allRunning 'primary')+(Control-Form 'stop' '서버 중지' $allStopped)+(Control-Form 'restart' '다시 시작' $allStopped)+(Control-Form 'refresh' '상태 새로고침')+(Control-Form $(if($autoEnabled){'auto-disable'}else{'auto-enable'}) $(if($autoEnabled){'자동 시작 끄기'}else{'자동 시작 켜기'}))+'</div><p class="muted">기본은 Windows 로그인 후 자동 시작 · PC 종료 시 종료입니다. 수동 중지는 서버·터널·감시를 함께 멈추며, 이 관리 화면은 계속 사용할 수 있습니다. 다음 로그인 자동 시작 여부는 위 설정을 따릅니다.</p></section>'
        $exportControl=Control-Form 'export' 'CSV 만들기'
        $mode='LOCAL · PRIVATE · 이 PC에서만 제어'
    }catch{$controls='<section class="panel"><h2>서버 제어 준비 필요</h2><p class="muted">Open-HostingDashboard.ps1로 대시보드를 열면 이 PC 전용 제어가 준비됩니다. 자동 시작 상태: '+$autoLabel+'</p></section>'}
    function Process-Card([string]$Kind,[string]$Title,[hashtable]$View){
        '<article class="card" data-component="'+$Kind+'" data-state="'+$View.state+'"><div class="card-label">'+$Title+'</div><span class="badge '+$View.tone+'">'+$View.label+'</span><p class="process">PID '+(ConvertTo-DashboardHtml $View.pid)+'</p><div class="muted">'+$(if($View.started){'시작 '+(Time-Cell $View.started)}else{'저장된 PID · 시작 시각 · 실행 파일 · 인수를 대조합니다.'})+'</div></article>'
    }
    $processCards=(Process-Card 'origin' '원본 서버' $origin)+(Process-Card 'tunnel' 'Cloudflare 터널' $tunnel)+(Process-Card 'monitor' '자동 복구 감시' $monitor)
    $generated=[DateTimeOffset]::UtcNow
    $css=@'
:root{color-scheme:dark;font-family:"Malgun Gothic","Segoe UI",sans-serif;background:#14181b;color:#e8ecec}*{box-sizing:border-box}body{margin:0}main{max-width:1320px;margin:auto;padding:32px 36px 44px}header{display:flex;justify-content:space-between;align-items:center;gap:24px;border-bottom:1px solid #30373d;padding-bottom:24px}.brand{font-size:12px;letter-spacing:.14em;color:#e8c76a}.private{color:#9da9b0;font-size:11px;letter-spacing:.1em}h1{font-size:28px;letter-spacing:-.04em;margin:10px 0}h2{font-size:17px;margin:0}p{line-height:1.65}small{display:block;font-size:10px;line-height:1.6;color:#77868f}time{white-space:nowrap}.muted{font-size:12px;color:#95a3ab;line-height:1.7}.stamp{text-align:right}.stamp time{font-size:12px}.grid{display:grid;gap:16px;grid-template-columns:repeat(3,minmax(0,1fr));margin:24px 0}.card,.panel{background:#1d2429;border:1px solid #323d44;border-radius:10px}.card{padding:22px}.card-label{font-size:12px;color:#a9b4bb;margin-bottom:14px}.badge{display:inline-block;font-size:11px;border-radius:20px;padding:5px 9px;background:#2c343a;color:#a7b1b8}.ok{background:#233b32;color:#a2d0b4}.warn{background:#423a25;color:#e8cd86}.process{font-size:23px;font-weight:600;margin:14px 0 6px}.metrics{display:flex;gap:40px;margin:0 0 26px}.metric strong{display:block;font-size:27px;margin-top:5px}.metric span{font-size:11px;color:#96a5ad}.connections{display:flex;gap:14px;flex-wrap:wrap;align-items:center;margin-bottom:12px}.panel{margin-top:22px;padding:22px}.section-head{display:flex;justify-content:space-between;gap:18px;align-items:center;margin-bottom:16px}.table-wrap{overflow-x:auto}table{border-collapse:collapse;width:100%;font-size:12px}th{text-align:left;font-size:10px;font-weight:500;color:#93a3ad;padding:12px 14px;background:#182025;white-space:nowrap}td{border-bottom:1px solid #303a40;padding:15px 14px;line-height:1.7;vertical-align:top}tbody tr:last-child td{border-bottom:0}.number{text-align:right;white-space:nowrap}.email{overflow-wrap:anywhere;min-width:180px}.empty{text-align:center;padding:38px;color:#8899a3}.button{display:inline-block;border:1px solid #46545d;background:#29333a;color:#e1e9ed;text-decoration:none;border-radius:6px;padding:8px 12px;font-size:11px}footer{margin-top:25px;font-size:11px;color:#7e8e98;line-height:1.9}.foot-links{display:flex;gap:12px;align-items:center}@media(max-width:800px){main{padding:22px 18px}.grid{grid-template-columns:1fr}.stamp{text-align:left}header{align-items:flex-start;flex-direction:column}.metrics{gap:24px;flex-wrap:wrap}.section-head{align-items:flex-start;flex-direction:column}}
'@
    $css+="`n.control-buttons,.foot-links{display:flex;flex-wrap:wrap;gap:10px;align-items:center}.controls form,.foot-links form{margin:0}.button{font-family:inherit;cursor:pointer}.button.primary{background:#e8c76a;color:#1b1f22;border-color:#e8c76a;font-weight:650}.button:disabled{opacity:.4;cursor:default}.button:focus-visible{outline:2px solid #e8c76a;outline-offset:3px}.controls p{margin:16px 0 0}.control-feedback{padding:14px 18px;background:#233b32;border:1px solid #486751;border-radius:8px;color:#c7e4cf;font-size:13px}.metrics{flex-wrap:wrap}"
    $styleHash=[Convert]::ToBase64String([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($css)))
    $html=@"
<!doctype html><html lang="ko"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><meta http-equiv="refresh" content="15"><meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src 'sha256-$styleHash'; script-src 'none'; base-uri 'none'; form-action $formAction; connect-src 'none'"><title>TeamHJD · 운영 대시보드</title><style>$css</style></head><body><main>
<header><div><div class="brand">TEAMHJD / PLAYTEST</div><h1>운영 대시보드</h1><span class="private">$mode</span></div><div class="stamp"><div class="muted">마지막 생성 · 한국 시각 (KST)</div>$(Time-Cell $generated.ToString('o'))<small>감시 30초 · 브라우저 새로고침 15초</small></div></header>
<!--CONTROL-FEEDBACK-->
$controls
<div class="grid">$processCards</div>
<div class="connections"><span class="badge $tcpTone">$tcpLabel</span><span class="badge $publicTone">$publicLabel</span></div><div class="muted">서버 실행 · PC 연결 · 배포 주소 연결 상태를 따로 확인합니다.</div>
<section class="panel"><div class="metrics"><div class="metric"><span>인증된 테스터</span><strong>$(ConvertTo-DashboardHtml $users.Count)<small>명</small></strong></div><div class="metric"><span>최근 5분 내 요청</span><strong>$(ConvertTo-DashboardHtml $recentCount)<small>명 · 실시간 접속자 수와 다릅니다</small></strong></div><div class="metric"><span>관측된 로그인 세션</span><strong>$(ConvertTo-DashboardHtml $sessionCount)</strong></div><div class="metric"><span>배포 게임</span><strong>$(ConvertTo-DashboardHtml $gameCount)</strong></div><div class="metric"><span>목록 순서</span><strong>$(ConvertTo-DashboardHtml $sequence)</strong></div></div><div class="section-head"><h2>게임 배포 목록</h2><span class="badge $catalogTone">$catalogLabel</span></div><div class="muted">유효기간 $(Time-Cell $expiry)</div><div class="table-wrap"><table><thead><tr><th>게임</th><th>버전</th><th class="number">패키지 크기</th></tr></thead><tbody>$gameRows</tbody></table></div></section>
<section class="panel"><div class="section-head"><div><h2>인증된 테스터</h2><p class="muted">$(ConvertTo-DashboardHtml $usersLabel) · 표의 기본 시각은 KST, 아래 작은 글씨는 UTC 원본입니다.</p></div><div class="foot-links">$exportControl $csvLink</div></div><div class="table-wrap"><table><thead><tr><th>이메일</th><th>최근 활동</th><th>최초 인증 확인</th><th>최근 새 세션</th><th>최근 접속</th><th class="number">세션 수</th></tr></thead><tbody>$userRows</tbody></table></div></section>
<footer>이 PC에서 보는 비공개 운영 정보입니다. 감시는 약 30초마다, 관리 화면은 15초마다 갱신됩니다. 마지막 생성 시각을 확인하세요.<br>최근 활동은 인증된 HTTP 요청 기준이며 실제 동시 접속·게임 실행 여부와 다릅니다. 로그인 수는 서로 다른 인증 세션 수이며 이메일 발송 횟수가 아닙니다.</footer>
</main></body></html>
"@
    $path=Join-Path $directory 'index.html';Assert-HostingPath $path
    $temporary=$path+'.'+[Guid]::NewGuid().ToString('N')+'.tmp'
    try{[IO.File]::WriteAllText($temporary,$html,[Text.UTF8Encoding]::new($false));[IO.File]::Move($temporary,$path,$true)}finally{if([IO.File]::Exists($temporary)){[IO.File]::Delete($temporary)}}
    [pscustomobject]@{DashboardPath=$path;GeneratedAt=$generated.ToString('o');VerifiedTesterCount=$users.Count;GameCount=$gameCount}
}finally{$lock.Dispose()}
