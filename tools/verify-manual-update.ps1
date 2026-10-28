#Requires -Version 7.0
param(
    [Parameter(Mandatory)][string]$QtRoot,
    [string]$Harness=(Join-Path (Split-Path $PSScriptRoot) 'build/LauncherControllerUpdateTests.exe'),
    [string]$Probe=(Join-Path (Split-Path $PSScriptRoot) 'build/LauncherCoreTests.exe'),
    [string]$Fixture=(Join-Path (Split-Path $PSScriptRoot) 'build/LauncherFixtureGame.exe')
)
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot
$run=Join-Path $project ('.local/manual-update-'+[DateTime]::Now.ToString('yyyyMMdd-HHmmss-fff'))
$private=Join-Path $run 'private'
$tls=Join-Path $run 'tls'
$build=Join-Path $run 'synthetic-build'
$profile=Join-Path $run 'profile'
$oldPath=$env:PATH
$oldPassword=$env:TEAMHJD_DISTRIBUTION_PASSWORD
$server=$null
New-Item -ItemType Directory -Path $build,$profile -Force|Out-Null

function Publish-Fixture([string]$Version){
    [IO.File]::WriteAllText((Join-Path $build 'revision.txt'),"Synthetic manual-update fixture $Version. No Unity content.`n",[Text.UTF8Encoding]::new($false))
    & (Join-Path $PSScriptRoot 'Publish-Game.ps1') -BuildDirectory $build -Id 'fixture' -Name 'Manual Update Fixture' -Version $Version -Executable 'Fixture.exe' -Description '자동 감지와 수동 업데이트를 검증하는 격리된 시험 게임입니다.' -PrivateRoot $private
}

try{
    $env:PATH="$QtRoot/bin;$env:PATH"
    $env:TEAMHJD_DISTRIBUTION_PASSWORD=[Convert]::ToBase64String([Security.Cryptography.RandomNumberGenerator]::GetBytes(24))
    & (Join-Path $PSScriptRoot 'New-LocalTlsFixture.ps1') -OutputDirectory $tls
    $listener=[Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,0)
    $listener.Start();$port=([Net.IPEndPoint]$listener.LocalEndpoint).Port;$listener.Stop()
    & (Join-Path $PSScriptRoot 'New-Publisher.ps1') -PrivateRoot $private -ServerUrl "https://localhost:$port/" -CertificatePath (Join-Path $tls 'server-cert.pem')
    Copy-Item -LiteralPath ([IO.Path]::GetFullPath($Fixture)) -Destination (Join-Path $build 'Fixture.exe')
    Publish-Fixture '1.0.0'
    $python=(Get-Command python).Source
    $arguments=@(('"'+(Join-Path $project 'server/distribution_server.py')+'"'),'--root',('"'+$private+'"'),'--bind','127.0.0.1','--port',"$port",'--cert',('"'+(Join-Path $tls 'server-cert.pem')+'"'),'--key',('"'+(Join-Path $tls 'server-key.pem')+'"'))
    $server=Start-Process -FilePath $python -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $run 'server-output.txt') -RedirectStandardError (Join-Path $run 'server-errors.txt')
    $ready=$false
    for($i=0;$i -lt 60;$i++){
        if($server.HasExited){throw 'The isolated HTTPS fixture server exited.'}
        if((Test-Path -LiteralPath (Join-Path $run 'server-output.txt')) -and ((Get-Content -LiteralPath (Join-Path $run 'server-output.txt') -Raw) -match 'ready')){$ready=$true;break}
        Start-Sleep -Milliseconds 100
    }
    if(-not $ready){throw 'The isolated HTTPS fixture server did not become ready.'}
    & ([IO.Path]::GetFullPath($Probe)) --probe $profile (Join-Path $private 'invite.json')
    if($LASTEXITCODE){throw 'Could not seed the verified 1.0.0 fixture.'}
    $invite=Get-Content -LiteralPath (Join-Path $private 'invite.json') -Raw|ConvertFrom-Json
    $blob=[Security.Cryptography.ProtectedData]::Protect([Text.Encoding]::UTF8.GetBytes($env:TEAMHJD_DISTRIBUTION_PASSWORD),$null,[Security.Cryptography.DataProtectionScope]::CurrentUser)
    @{server=$invite.server;publisherKey=$invite.publisherKey;certificate=[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($invite.certificate));passwordBlob=[Convert]::ToBase64String($blob);gamesRoot=(Join-Path $profile 'games');sequence=0;reduceMotion=$true}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $profile 'settings.json') -Encoding utf8NoBOM
    Copy-Item -LiteralPath (Join-Path $private 'catalog.json') -Destination (Join-Path $profile 'catalog-cache.json')
    Publish-Fixture '1.1.0'
    & ([IO.Path]::GetFullPath($Harness)) --profile $profile --catalog (Join-Path $private 'catalog.json') --result (Join-Path $run 'result.json')
    if($LASTEXITCODE){throw 'Controller manual-update integration failed.'}
    $result=Get-Content -LiteralPath (Join-Path $run 'result.json') -Raw|ConvertFrom-Json
    if($result.result -cne 'PASS'){throw 'The Controller fixture did not produce a passing result.'}
    # Preserve a separate old-installed/new-available profile for native UI review.
    # A reviewer may restart only this synthetic loopback server; the test itself
    # still stops its own server in finally and never leaves a background daemon.
    $pendingProfile=Join-Path $run 'ui-pending-profile'
    Copy-Item -LiteralPath $profile -Destination $pendingProfile -Recurse
    $pendingSettings=Get-Content -LiteralPath (Join-Path $pendingProfile 'settings.json') -Raw|ConvertFrom-Json
    $pendingSettings.gamesRoot=Join-Path $pendingProfile 'games'
    $pendingSettings|ConvertTo-Json -Depth 12|Set-Content -LiteralPath (Join-Path $pendingProfile 'settings.json') -Encoding utf8NoBOM
    $activePath=Join-Path $pendingProfile 'games/fixture/active.json'
    $active=Get-Content -LiteralPath $activePath -Raw|ConvertFrom-Json
    @{current=$active.previous;previous=$active.current}|ConvertTo-Json|Set-Content -LiteralPath $activePath -Encoding utf8NoBOM
    $result|Add-Member -NotePropertyName uiPendingProfile -NotePropertyValue $pendingProfile
    $result|ConvertTo-Json -Depth 12|Set-Content -LiteralPath (Join-Path $run 'result.json') -Encoding utf8NoBOM
    Write-Output "PASS actual Controller automatic detection, old-version play, explicit Update, rollback and manual reapply. Evidence: $run"
}finally{
    if($server -and -not $server.HasExited){Stop-Process -Id $server.Id}
    $env:PATH=$oldPath
    $env:TEAMHJD_DISTRIBUTION_PASSWORD=$oldPassword
}
