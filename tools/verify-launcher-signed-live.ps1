#Requires -Version 7.0
param([Parameter(Mandatory)][string]$ReleaseDirectory,
      [Parameter(Mandatory)][ValidatePattern('^[0-9]+\.[0-9]+\.[0-9]+$')][string]$Version)
$ErrorActionPreference='Stop'
if(-not $IsWindows){throw 'This live GUI acceptance tool requires Windows.'}
$project=[IO.Path]::GetFullPath((Split-Path $PSScriptRoot))
. (Join-Path $project 'tools/HostingCommon.ps1')
$meta=@{dist=[IO.Path]::GetFullPath($ReleaseDirectory);version=$Version}
Assert-HostingPath $meta.dist
$payload=Get-Item -LiteralPath (Join-Path $meta.dist 'TeamHJD-LauncherPayload-Windows-x64.zip')
$asset=@{size=$payload.Length;digest='sha256:'+(Get-FileHash -LiteralPath $payload.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
$run=Join-Path $project ('.local/static-feed-live-'+[DateTime]::Now.ToString('yyyyMMdd-HHmmss-fff'))
$root=Join-Path $run 'installation';$profile=Join-Path $run 'profile'
New-Item -ItemType Directory -Path $run,$profile|Out-Null
Copy-Item -LiteralPath (Join-Path $meta.dist 'TeamHJD-GameTestLauncher-Windows-x64') -Destination $root -Recurse
$root=$root.Replace('\','/');$profile=$profile.Replace('\','/')
$initial=Read-HostingJson (Join-Path $root 'launcher/active.json') 32768
$clientExe=Join-Path $root ('launcher/versions/'+$initial.current+'/TeamHJDClient.exe')
$client=$null;$owned=$null
function Owned-Child {
    $running=Read-HostingJson (Join-Path $root 'launcher/running.json') 16384
    $exe=[IO.Path]::GetFullPath((Join-Path $root ('launcher/versions/'+$running.folder+'/'+$running.executable)))
    $ticks=[DateTime]::FromFileTimeUtc([long]$running.stamp).Ticks.ToString()
    Get-OwnedHostingProcess @{pid=$running.pid;startedTicks=$ticks} $exe @('--launcher-root',$root,'--data-root',$profile,'--health-file')
}
try {
    $arguments=@('--data-root',('"'+$profile+'"'),'--launcher-root',('"'+$root+'"'),'--test-allow-loopback','--test-update-from','0.0.9','--capture',('"'+(Join-Path $run 'updater.png')+'"'),'--quit-after-ms','180000')
    $client=Start-Process -FilePath $clientExe -ArgumentList $arguments -PassThru -WindowStyle Normal -RedirectStandardError (Join-Path $run 'client-stderr.txt')
    Write-Output 'Waiting for explicit Update approval in the isolated visible launcher; no comparison override is used in the real user installation.'
    $deadline=[DateTime]::UtcNow.AddSeconds(170)
    do {
        $state=Read-HostingJson (Join-Path $root 'launcher/active.json') 32768
        if($state.current -cne $initial.current -and (Test-Path -LiteralPath (Join-Path $root 'launcher/running.json'))){
            try{$owned=Owned-Child;if($owned){break}}catch{if([DateTime]::UtcNow -ge $deadline){throw}}
        }
        if(Test-Path -LiteralPath (Join-Path $root 'launcher/failed-release.json')){throw 'Live signed update recorded a failed health latch.'}
        Start-Sleep -Milliseconds 200
    }while([DateTime]::UtcNow -lt $deadline)
    if(-not $owned){throw 'No actual healthy signed-feed transition completed in time.'}
    if(-not $client.WaitForExit(10000) -or $client.ExitCode -ne 0){throw 'The original client did not exit normally for the automatic handoff.'}
    if($state.previous -cne $initial.current){throw 'Rollback slot was not preserved.'}
    $candidate=Join-Path $root ('launcher/versions/'+$state.current)
    $receipt=Read-HostingJson (Join-Path $candidate '._teamhjd-receipt.json') 2097152
    if($receipt.version -cne $meta.version -or $receipt.archiveSha256 -cne $asset.digest.Substring(7)){throw 'Live payload receipt differs from the published asset.'}
    $archive=Join-Path $profile ('downloads/launcher-'+$asset.digest.Substring(7)+'.part')
    if((Get-Item -LiteralPath $archive).Length -ne $asset.size -or (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -cne $asset.digest.Substring(7)){throw 'The actual live download hash/size differs.'}
    foreach($file in $receipt.files){$path=Join-Path $candidate $file.path;if((Get-Item -LiteralPath $path).Length -ne $file.bytes -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $file.sha256){throw 'The live extracted tree differs.'}}
    $actual=[IO.File]::ReadAllBytes((Join-Path $profile 'launcher-feed.json'))
    $expected=[IO.File]::ReadAllBytes((Join-Path $meta.dist 'launcher-stable.json'))
    if(-not [Linq.Enumerable]::SequenceEqual[byte]($actual,$expected)){throw 'The client accepted a different feed.'}
    foreach($name in @('pending-launcher.json','bootstrap-update.json','queued-launcher.json','launcher/failed-release.json')){if(Test-Path -LiteralPath (Join-Path $root $name)){throw 'Pending/failure state remains after healthy activation.'}}
    Write-HostingJson (Join-Path $run 'result.json') @{result='PASS';version=$meta.version;realPublicSignedFeed=$true;comparisonFixture='0.0.9';applicationVersionUnchanged=$true;downloadBytes=$asset.size;downloadSha256=$asset.digest.Substring(7);allExtractedFileHashesVerified=$true;payloadFiles=@($receipt.files).Count;automaticHelperHandoff=$true;actualNewGuiPidAndVersionHealth=$true;previousSlotPreserved=$true;humanEmailOTPTested=$false}
    Write-Output ('PASS actual public signed feed/download/full tree/automatic restart/new GUI health: '+$run)
}finally{
    if($owned){$verified=Owned-Child;if($verified){$verified.Kill();$verified.WaitForExit()}}
    if($client -and -not $client.HasExited){$null=$client.CloseMainWindow();$null=$client.WaitForExit(10000)}
}
