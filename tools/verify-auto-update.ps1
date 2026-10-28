#Requires -Version 7.0
param(
    [Parameter(Mandatory)][string]$QtRoot,
    [string]$Executable=(Join-Path (Split-Path $PSScriptRoot) 'build/TeamHJDClient.exe')
)
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot
$run=Join-Path $project ('.local/automatic-update-'+[DateTime]::Now.ToString('yyyyMMdd-HHmmss-fff'))
$private=Join-Path $run 'private'
$tls=Join-Path $run 'tls'
$build=Join-Path $run 'synthetic-build'
$profile=Join-Path $run 'profile'
$probe=Join-Path $project 'build/LauncherCoreTests.exe'
$oldPath=$env:PATH
$oldPassword=$env:TEAMHJD_DISTRIBUTION_PASSWORD
$oldScale=$env:QT_SCALE_FACTOR
$server=$null
$client=$null
New-Item -ItemType Directory -Path $build,$profile -Force|Out-Null

function Publish-Fixture([string]$Version){
    [IO.File]::WriteAllText((Join-Path $build 'revision.txt'),"Synthetic automatic-update fixture $Version. No Unity content.`n",[Text.UTF8Encoding]::new($false))
    & (Join-Path $PSScriptRoot 'Publish-Game.ps1') -BuildDirectory $build -Id 'fixture' -Name 'Automatic Update Fixture' -Version $Version -Executable 'Fixture.exe' -Description '자동 업데이트 검증을 위한 격리된 시험 게임입니다.' -PrivateRoot $private
}
function Get-CurrentReceipt {
    $statePath=Join-Path $profile 'games/fixture/active.json'
    if(-not(Test-Path -LiteralPath $statePath)){return $null}
    $state=Get-Content -LiteralPath $statePath -Raw|ConvertFrom-Json
    if(-not $state.current){return $null}
    $receiptPath=Join-Path $profile "games/fixture/versions/$($state.current)/._teamhjd-receipt.json"
    if(-not(Test-Path -LiteralPath $receiptPath)){return $null}
    return Get-Content -LiteralPath $receiptPath -Raw|ConvertFrom-Json
}
function Wait-Version([string]$Version,[int]$Seconds){
    $deadline=[DateTime]::UtcNow.AddSeconds($Seconds)
    while([DateTime]::UtcNow -lt $deadline){
        if($client.HasExited){throw "The automatic-update client exited before reaching $Version."}
        $receipt=Get-CurrentReceipt
        if($receipt -and $receipt.version -ceq $Version){return}
        Start-Sleep -Milliseconds 200
    }
    throw "The real Controller did not automatically install $Version in $Seconds seconds."
}
function Assert-Installed([string]$Version,[string]$PreviousVersion){
    $state=Get-Content -LiteralPath (Join-Path $profile 'games/fixture/active.json') -Raw|ConvertFrom-Json
    $base=Join-Path $profile "games/fixture/versions/$($state.current)"
    $receipt=Get-Content -LiteralPath (Join-Path $base '._teamhjd-receipt.json') -Raw|ConvertFrom-Json
    if($receipt.version -cne $Version){throw 'The automatic update activated the wrong revision.'}
    foreach($file in $receipt.files){
        $path=Join-Path $base $file.path
        if((Get-Item -LiteralPath $path).Length -ne $file.bytes -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $file.sha256){throw 'The activated automatic update failed a file hash/size check.'}
    }
    $previous=Get-Content -LiteralPath (Join-Path $profile "games/fixture/versions/$($state.previous)/._teamhjd-receipt.json") -Raw|ConvertFrom-Json
    if($previous.version -cne $PreviousVersion){throw 'The prior installed revision was not preserved.'}
}
function Start-ReviewClient([string]$Name,[int]$Duration){
    $image=Join-Path $run ($Name+'.png')
    $errors=Join-Path $run ($Name+'-stderr.txt')
    $arguments=@('--data-root',('"'+$profile+'"'),'--test-allow-loopback','--capture',('"'+$image+'"'),'--quit-after-ms',"$Duration")
    return Start-Process -FilePath ([IO.Path]::GetFullPath($Executable)) -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardError $errors
}
function Assert-CleanClient([string]$Name){
    if(-not $client.WaitForExit(15000)){throw 'The isolated review client did not finish.'}
    if($client.ExitCode -ne 0){throw "The isolated client exited with $($client.ExitCode)."}
    $image=Join-Path $run ($Name+'.png')
    if(-not(Test-Path -LiteralPath $image)){throw 'The native automatic-update screenshot was not captured.'}
    $errors=[IO.File]::ReadAllText((Join-Path $run ($Name+'-stderr.txt')))
    if($errors -match 'ReferenceError|TypeError|Unable to assign|Cannot assign|failed to load|is not installed|Binding loop'){throw 'The automatic-update client reported a QML error.'}
}

try{
    $env:PATH="$QtRoot/bin;$env:PATH"
    $env:QT_SCALE_FACTOR='1'
    $env:TEAMHJD_DISTRIBUTION_PASSWORD=[Convert]::ToBase64String([Security.Cryptography.RandomNumberGenerator]::GetBytes(24))
    & (Join-Path $PSScriptRoot 'New-LocalTlsFixture.ps1') -OutputDirectory $tls
    $listener=[Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,0)
    $listener.Start();$port=([Net.IPEndPoint]$listener.LocalEndpoint).Port;$listener.Stop()
    & (Join-Path $PSScriptRoot 'New-Publisher.ps1') -PrivateRoot $private -ServerUrl "https://localhost:$port/" -CertificatePath (Join-Path $tls 'server-cert.pem')
    Copy-Item -LiteralPath (Join-Path $project 'build/LauncherFixtureGame.exe') -Destination (Join-Path $build 'Fixture.exe')
    Publish-Fixture '1.0.0'
    $python=(Get-Command python).Source
    $server=Start-Process -FilePath $python -ArgumentList @(('"'+(Join-Path $project 'server/distribution_server.py')+'"'),'--root',('"'+$private+'"'),'--bind','127.0.0.1','--port',"$port",'--cert',('"'+(Join-Path $tls 'server-cert.pem')+'"'),'--key',('"'+(Join-Path $tls 'server-key.pem')+'"')) -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $run 'server-output.txt') -RedirectStandardError (Join-Path $run 'server-errors.txt')
    $ready=$false
    for($i=0;$i -lt 60;$i++){
        if($server.HasExited){throw 'The private loopback fixture server exited.'}
        if((Test-Path -LiteralPath (Join-Path $run 'server-output.txt')) -and ((Get-Content -LiteralPath (Join-Path $run 'server-output.txt') -Raw) -match 'ready')){$ready=$true;break}
        Start-Sleep -Milliseconds 100
    }
    if(-not $ready){throw 'The private loopback fixture server did not become ready.'}
    & $probe --probe $profile (Join-Path $private 'invite.json')
    if($LASTEXITCODE){throw 'Could not seed the first verified installation.'}
    $invite=Get-Content -LiteralPath (Join-Path $private 'invite.json') -Raw|ConvertFrom-Json
    $blob=[Security.Cryptography.ProtectedData]::Protect([Text.Encoding]::UTF8.GetBytes($env:TEAMHJD_DISTRIBUTION_PASSWORD),$null,[Security.Cryptography.DataProtectionScope]::CurrentUser)
    @{server=$invite.server;publisherKey=$invite.publisherKey;certificate=[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($invite.certificate));passwordBlob=[Convert]::ToBase64String($blob);gamesRoot=(Join-Path $profile 'games');sequence=0;reduceMotion=$true}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $profile 'settings.json') -Encoding utf8NoBOM
    Copy-Item -LiteralPath (Join-Path $private 'catalog.json') -Destination (Join-Path $profile 'catalog-cache.json')

    # No UI invocation or refresh request: Controller startup discovers v2 and
    # executes its automatic download/install path against the signed HTTPS API.
    Publish-Fixture '1.1.0'
    $client=Start-ReviewClient 'startup-automatic-update' 30000
    Wait-Version '1.1.0' 20
    Assert-Installed '1.1.0' '1.0.0'
    # Do not schedule shutdown in the middle of extraction: the production UI
    # correctly refuses close while a foreground install is active.
    Start-Sleep -Seconds 4
    if(-not $client.HasExited){$null=$client.CloseMainWindow()}
    Assert-CleanClient 'startup-automatic-update'
    Write-Output 'PASS Controller startup automatically discovered, downloaded, verified and activated 1.1.0; prior version preserved.'

    # Keep the actual application running and change only the server publication.
    # This deliberately waits for the production automatic check (120s threshold
    # sampled by a 30s timer, so allow the next polling tick),
    # rather than invoking refresh() or changing production timing for the test.
    $client=Start-ReviewClient 'running-before-new-publication' 210000
    Start-Sleep -Seconds 7
    Publish-Fixture '1.2.0'
    $publishedAt=[DateTimeOffset]::UtcNow
    Write-Output 'Waiting for the running Controller automatic check; no manual refresh or install will be invoked.'
    Wait-Version '1.2.0' 175
    $observedAt=[DateTimeOffset]::UtcNow
    Assert-Installed '1.2.0' '1.1.0'
    # Request normal window close only for this process created by this script.
    if(-not $client.HasExited){$null=$client.CloseMainWindow()}
    Assert-CleanClient 'running-before-new-publication'
    # Capture the settled post-update UI as a separate observation of the same
    # verified installation; opening it does not alter the already-proven result.
    $client=Start-ReviewClient 'running-automatic-update-complete' 6500
    Assert-CleanClient 'running-automatic-update-complete'
    @{result='PASS';controllerStartupAutomaticUpdate=$true;controllerRunningAutomaticUpdate=$true;api='isolated signed HTTPS fixture';first='1.0.0';startup='1.1.0';running='1.2.0';hashesVerified=$true;previousPreserved=$true;manualRefreshInvoked=$false;publishedAt=$publishedAt.ToString('o');observedAt=$observedAt.ToString('o');secondsToRunningUpdate=[Math]::Round(($observedAt-$publishedAt).TotalSeconds,2);profile=$profile}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $run 'result.json') -Encoding utf8NoBOM
    Write-Output "PASS real running Controller automatic update, signed API, complete file hashes and prior-version retention. Evidence: $run"
}finally{
    if($client -and -not $client.HasExited){$null=$client.CloseMainWindow();if(-not $client.WaitForExit(10000)){Write-Warning 'The isolated review client remains open; it was not force terminated.'}}
    if($server -and -not $server.HasExited){Stop-Process -Id $server.Id}
    $env:PATH=$oldPath
    $env:TEAMHJD_DISTRIBUTION_PASSWORD=$oldPassword
    $env:QT_SCALE_FACTOR=$oldScale
}
