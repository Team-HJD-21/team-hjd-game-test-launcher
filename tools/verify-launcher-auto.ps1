#Requires -Version 7.0
param(
    [Parameter(Mandatory)][string]$QtRoot,
    [string]$Executable=(Join-Path (Split-Path $PSScriptRoot) 'build/TeamHJDClient.exe'),
    [ValidatePattern('^[0-9]+\.[0-9]+\.[0-9]+$')][string]$ExpectedRelease='0.1.0'
)
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot
$run=Join-Path $project ('.local/launcher-automatic-update-'+[DateTime]::Now.ToString('yyyyMMdd-HHmmss-fff'))
$profile=Join-Path $run 'profile'
$installation=Join-Path $run 'installation'
$binaryRoot=Join-Path $run 'review-client'
$capture=Join-Path $run 'automatic-launcher-update.png'
$errors=Join-Path $run 'client-stderr.txt'
$oldPath=$env:PATH
$oldScale=$env:QT_SCALE_FACTOR
$client=$null
New-Item -ItemType Directory -Path $profile,$installation,$binaryRoot -Force|Out-Null
Copy-Item -LiteralPath ([IO.Path]::GetFullPath($Executable)) -Destination (Join-Path $binaryRoot 'TeamHJDClient.exe')

try{
    $env:PATH="$QtRoot/bin;$env:PATH"
    $env:QT_SCALE_FACTOR='1'
    $guardProfile=Join-Path $run 'guard-profile'
    $guardInstall=Join-Path $run 'guard-installation'
    $quotedProfile='"'+$guardProfile+'"'
    $quotedInstall='"'+$guardInstall+'"'
    $guardCases=@(
        @{name='missing-loopback';args=@('--data-root',$quotedProfile,'--launcher-root',$quotedInstall,'--test-update-from','0.0.9')},
        @{name='missing-launcher-root';args=@('--data-root',$quotedProfile,'--test-allow-loopback','--test-update-from','0.0.9')},
        @{name='shared-roots';args=@('--data-root',$quotedProfile,'--launcher-root',$quotedProfile,'--test-allow-loopback','--test-update-from','0.0.9')},
        @{name='invalid-comparison-version';args=@('--data-root',$quotedProfile,'--launcher-root',$quotedInstall,'--test-allow-loopback','--test-update-from','0.0.9-extra')}
    )
    foreach($case in $guardCases){
        $guard=Start-Process -FilePath (Join-Path $binaryRoot 'TeamHJDClient.exe') -ArgumentList $case.args -PassThru -WindowStyle Hidden -RedirectStandardError (Join-Path $run ($case.name+'-stderr.txt'))
        if(-not $guard.WaitForExit(10000) -or $guard.ExitCode -ne 64){throw "The isolated update-comparison guard failed: $($case.name)."}
    }
    # Read the same production endpoint, without modifying its response or
    # redirecting the app to a mock. No token or private game data is involved.
    $api='https://api.github.com/repos/Team-HJD-21/team-hjd-game-test-launcher/releases/latest'
    $release=Invoke-RestMethod -Uri $api -Headers @{Accept='application/vnd.github+json';'X-GitHub-Api-Version'='2022-11-28';'User-Agent'='TeamHJD-Launcher-Auto-Verification'} -TimeoutSec 30
    if($release.tag_name -cne "v$ExpectedRelease" -or $release.draft -or $release.prerelease){throw 'The production release differs from the expected pre-release verification target.'}
    $asset=@($release.assets|Where-Object name -CEQ 'TeamHJD-LauncherPayload-Windows-x64.zip')
    if($asset.Count -ne 1 -or $asset[0].digest -cnotmatch '^sha256:[0-9a-f]{64}$'){throw 'The production update payload/digest is missing.'}
    $expectedHash=$asset[0].digest.Substring(7)
    $expectedBytes=[long]$asset[0].size
    $expectedUrl="https://github.com/Team-HJD-21/team-hjd-game-test-launcher/releases/download/v$ExpectedRelease/TeamHJD-LauncherPayload-Windows-x64.zip"
    if($asset[0].browser_download_url -cne $expectedUrl){throw 'The production payload has an unexpected repository/tag URL.'}

    # The sole synthetic input is the comparison version. Reported application
    # version, production API, TLS, URL policy, digest and extraction stay real.
    # It is rejected unless loopback testing and isolated roots are explicit.
    $arguments=@('--data-root',('"'+$profile+'"'),'--launcher-root',('"'+$installation+'"'),'--test-allow-loopback','--test-update-from','0.0.9','--capture',('"'+$capture+'"'),'--quit-after-ms','110000')
    $client=Start-Process -FilePath (Join-Path $binaryRoot 'TeamHJDClient.exe') -ArgumentList $arguments -PassThru -WindowStyle Hidden -RedirectStandardError $errors
    $queuedPath=Join-Path $installation 'queued-launcher.json'
    $started=[DateTimeOffset]::UtcNow
    $deadline=$started.AddSeconds(100)
    while(-not(Test-Path -LiteralPath $queuedPath)){
        if($client.HasExited){throw 'The real client exited before queuing its automatically detected launcher update.'}
        if([DateTimeOffset]::UtcNow -ge $deadline){throw 'The real Controller did not queue the production update within 100 seconds.'}
        Start-Sleep -Milliseconds 200
    }
    $observed=[DateTimeOffset]::UtcNow
    $queue=Get-Content -LiteralPath $queuedPath -Raw|ConvertFrom-Json
    if($queue.candidate -cnotmatch ('^'+[regex]::Escape($ExpectedRelease)+'-[a-f0-9]{12}-[a-f0-9]{8}$')){throw 'The queued candidate is outside the expected managed version directory.'}
    $candidate=Join-Path $installation "launcher/versions/$($queue.candidate)"
    $receipt=Get-Content -LiteralPath (Join-Path $candidate '._teamhjd-receipt.json') -Raw|ConvertFrom-Json
    if($receipt.version -cne $ExpectedRelease -or $receipt.executable -cne 'TeamHJDClient.exe' -or $receipt.archiveSha256 -cne $expectedHash){throw 'The automatically staged payload does not match the live release.'}
    $archive=Join-Path $profile "downloads/launcher-$expectedHash.part"
    if((Get-Item -LiteralPath $archive).Length -ne $expectedBytes -or (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -cne $expectedHash){throw 'The actual downloaded archive differs from the GitHub API digest/size.'}
    foreach($file in $receipt.files){
        $path=Join-Path $candidate $file.path
        if((Get-Item -LiteralPath $path).Length -ne $file.bytes -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $file.sha256){throw 'The automatically staged payload failed full file hash/size verification.'}
    }
    # Allow the scheduled native capture and completion notification to settle.
    Start-Sleep -Seconds 3
    if(-not $client.HasExited){$null=$client.CloseMainWindow()}
    if(-not $client.WaitForExit(15000) -or $client.ExitCode -ne 0){throw 'The automatic launcher-update review did not close normally.'}
    if(-not(Test-Path -LiteralPath $capture)){throw 'The actual launcher-update UI was not captured.'}
    $diagnostic=[IO.File]::ReadAllText($errors)
    if($diagnostic -match 'ReferenceError|TypeError|Unable to assign|Cannot assign|failed to load|is not installed|Binding loop'){throw 'The automatic updater reported a QML error.'}
    @{result='PASS';testHookGuards=$guardCases.Count;controllerAutomaticLauncherUpdate=$true;comparisonFixture='0.0.9';reportedVersionUnchanged=$true;productionApi=$api;release=$release.tag_name;productionPayloadUrl=$expectedUrl;sha256=$expectedHash;bytes=$expectedBytes;payloadFiles=@($receipt.files).Count;fullPayloadHashesVerified=$true;queuedForNextLaunch=$true;manualCheckInvoked=$false;apiResponseReplaced=$false;secondsToQueue=[Math]::Round(($observed-$started).TotalSeconds,2);candidate=$queue.candidate;capture=$capture}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $run 'result.json') -Encoding utf8NoBOM
    Write-Output "PASS real Controller automatic public API detection, payload download/digest, full extraction and next-launch queue. Evidence: $run"
}finally{
    if($client -and -not $client.HasExited){$null=$client.CloseMainWindow();if(-not $client.WaitForExit(10000)){Write-Warning 'The isolated automatic-update review remains open; it was not force terminated.'}}
    $env:PATH=$oldPath
    $env:QT_SCALE_FACTOR=$oldScale
}
