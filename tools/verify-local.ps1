param([Parameter(Mandatory)][string]$QtRoot,[switch]$IncludeCanonicalGame)
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot
$run=Join-Path $project ('.local/verification-'+[DateTime]::Now.ToString('yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $run -Force|Out-Null
$env:PATH="$QtRoot/bin;$env:PATH"
$env:TEAMHJD_DISTRIBUTION_PASSWORD=[Convert]::ToBase64String([Security.Cryptography.RandomNumberGenerator]::GetBytes(24))
$private=Join-Path $run 'private';$tls=Join-Path $run 'tls';$build=Join-Path $run 'synthetic-build';$profile=Join-Path $run 'profile'
New-Item -ItemType Directory -Path $build,$profile -Force|Out-Null
& (Join-Path $PSScriptRoot 'New-LocalTlsFixture.ps1') -OutputDirectory $tls
# Bind an available loopback port; never configure a firewall or router.
$listener=[Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,0);$listener.Start();$port=([Net.IPEndPoint]$listener.LocalEndpoint).Port;$listener.Stop()
& (Join-Path $PSScriptRoot 'New-Publisher.ps1') -PrivateRoot $private -ServerUrl "https://localhost:$port/" -CertificatePath (Join-Path $tls 'server-cert.pem')
Copy-Item -LiteralPath (Join-Path $project 'build/LauncherFixtureGame.exe') -Destination (Join-Path $build 'Fixture.exe')
'Synthetic fixture revision 1. No Unity content.'|Set-Content -LiteralPath (Join-Path $build 'revision.txt')
& (Join-Path $PSScriptRoot 'Publish-Game.ps1') -BuildDirectory $build -Id 'fixture' -Name 'Launcher Verification' -Version '1.0.0' -Executable 'Fixture.exe' -Description '격리된 업데이트 검증용 시험 게임입니다.' -PrivateRoot $private
$python=(Get-Command python).Source
$server=Start-Process -FilePath $python -ArgumentList @(('"'+(Join-Path $project 'server/distribution_server.py')+'"'),'--root',('"'+$private+'"'),'--bind','127.0.0.1','--port',"$port",'--cert',('"'+(Join-Path $tls 'server-cert.pem')+'"'),'--key',('"'+(Join-Path $tls 'server-key.pem')+'"')) -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $run 'server-output.txt') -RedirectStandardError (Join-Path $run 'server-errors.txt')
try{
    $ready=$false;for($i=0;$i -lt 50;$i++){Start-Sleep -Milliseconds 100;if(Test-Path -LiteralPath (Join-Path $run 'server-output.txt')){if((Get-Content -LiteralPath (Join-Path $run 'server-output.txt') -Raw) -match 'ready'){$ready=$true;break}}}
    if(-not $ready){throw 'Local HTTPS server did not start.'}
    $probe=Join-Path $project 'build/LauncherCoreTests.exe'
    & $probe --probe $profile (Join-Path $private 'invite.json') --resume --launch
    if($LASTEXITCODE){throw 'Initial HTTPS install/resume/launch probe failed.'}
    'Synthetic fixture revision 2. No Unity content.'|Set-Content -LiteralPath (Join-Path $build 'revision.txt')
    & (Join-Path $PSScriptRoot 'Publish-Game.ps1') -BuildDirectory $build -Id 'fixture' -Name 'Launcher Verification' -Version '1.1.0' -Executable 'Fixture.exe' -Description '격리된 업데이트 검증용 시험 게임입니다.' -PrivateRoot $private
    & $probe --probe $profile (Join-Path $private 'invite.json') --resume
    if($LASTEXITCODE){throw 'HTTPS update probe failed.'}
    $state=Get-Content -LiteralPath (Join-Path $profile 'games/fixture/active.json') -Raw|ConvertFrom-Json
    if(-not $state.previous -or -not(Test-Path -LiteralPath (Join-Path $profile "games/fixture/versions/$($state.previous)/Fixture.exe"))){throw 'Previous version was not retained.'}
    # A corrupt or untrusted CA must never be silently accepted.
    $invite=Get-Content -LiteralPath (Join-Path $private 'invite.json') -Raw|ConvertFrom-Json
    $bad=@{schema=1;server=$invite.server;publisherKey=$invite.publisherKey}
    $bad|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $run 'untrusted-invite.json')
    & $probe --probe (Join-Path $run 'untrusted-profile') (Join-Path $run 'untrusted-invite.json') 2> (Join-Path $run 'expected-tls-rejection.txt')
    if($LASTEXITCODE -eq 0){throw 'Untrusted TLS certificate was accepted.'}
    # A UI profile contains only synthetic credentials encrypted with Windows DPAPI.
    $blob=[Security.Cryptography.ProtectedData]::Protect([Text.Encoding]::UTF8.GetBytes($env:TEAMHJD_DISTRIBUTION_PASSWORD),$null,[Security.Cryptography.DataProtectionScope]::CurrentUser)
    @{server=$invite.server;publisherKey=$invite.publisherKey;certificate=[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($invite.certificate));passwordBlob=[Convert]::ToBase64String($blob);gamesRoot=(Join-Path $profile 'games');sequence=0;reduceMotion=$false}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $profile 'settings.json')
    Copy-Item -LiteralPath (Join-Path $private 'catalog.json') -Destination (Join-Path $profile 'catalog-cache.json')
    if($IncludeCanonicalGame){
        & (Join-Path $PSScriptRoot 'Publish-GameArchive.ps1') -LatestPointer 'D:/Dev/Unity/Unity_3D_HoseForHire/Builds/latest.json' -PrivateRoot $private
        & $probe --probe (Join-Path $run 'canonical-game-profile') (Join-Path $private 'invite.json') --resume --game-id 'hose-for-hire'
        if($LASTEXITCODE){throw 'Canonical game private HTTPS installation failed.'}
    }
    @{result='PASS';httpsInstall=$true;resume=$true;update=$true;previousPreserved=$true;launchAndProcessBlock=$true;untrustedTlsRejected=$true;canonicalGame=[bool]$IncludeCanonicalGame;profile=$profile;privateRoot=$private}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $run 'result.json')
    Write-Output "PASS local HTTPS publication, download/resume, install, process lock, update, TLS rejection. Evidence: $run"
}finally{if(-not $server.HasExited){Stop-Process -Id $server.Id};$env:TEAMHJD_DISTRIBUTION_PASSWORD=$null}
