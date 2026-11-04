#Requires -Version 7.0
param([Parameter(Mandatory)][string]$GoodBundle,[Parameter(Mandatory)][string]$BadBundle)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'HostingCommon.ps1')
$project=[IO.Path]::GetFullPath((Split-Path $PSScriptRoot))
$good=[IO.Path]::GetFullPath($GoodBundle);$bad=[IO.Path]::GetFullPath($BadBundle)
foreach($path in @($good,$bad)){if(-not $path.StartsWith($project+'\.local\installer-',[StringComparison]::OrdinalIgnoreCase)){throw 'Only explicit local synthetic installer bundles are accepted.'};Assert-HostingPath $path}
$run=Join-Path $project ('.local/installer-health-rollback-'+[DateTime]::Now.ToString('yyyyMMdd-HHmmss-fff'))
$root=Join-Path $run 'TeamHJD Game Test Launcher';$profile=Join-Path $run 'profile'
New-Item -ItemType Directory -Path $profile -Force|Out-Null
$preserved=Join-Path $profile 'synthetic-game-and-settings.txt';[IO.File]::WriteAllText($preserved,'existing private fixture data',[Text.Encoding]::UTF8)
$before=(Get-FileHash -LiteralPath $preserved -Algorithm SHA256).Hash
function Invoke-Helper([string]$Bundle,[bool]$Health,[int]$Expected,[string]$Step){
    $exe=Join-Path $Bundle 'TeamHJDLauncher.exe'
    $arguments='--install-to "'+$root+'" --data-root "'+$profile+'" --noninteractive-test'
    if($Health){$arguments+=' --require-health'}
    $process=Start-Process -FilePath $exe -ArgumentList $arguments -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $run ($Step+'.stdout.log')) -RedirectStandardError (Join-Path $run ($Step+'.stderr.log'))
    if(-not $process.WaitForExit(90000) -or $process.ExitCode -ne $Expected){throw ('Unexpected native installer result: '+$Step)}
}
Invoke-Helper $good $false 0 'initial'
Invoke-Helper $good $true 0 'good-health'
$prior=Read-HostingJson (Join-Path $root 'launcher/active.json') 16384
$marker=[IO.File]::ReadAllBytes((Join-Path $root 'teamhjd-install.json'))
$bootstrap=(Get-FileHash -LiteralPath (Join-Path $root 'TeamHJDLauncher.exe') -Algorithm SHA256).Hash
Start-Sleep -Seconds 3 # The bounded healthy fixture exits by itself.
Invoke-Helper $bad $true 1 'bad-health'
$active=Read-HostingJson (Join-Path $root 'launcher/active.json') 16384
$failure=Read-HostingJson (Join-Path $root 'launcher/failed-release.json') 16384
$running=Read-HostingJson (Join-Path $root 'launcher/running.json') 16384
if($active.current -cne $prior.current -or $running.folder -cne $prior.current -or $failure.reason -cne 'health'){throw 'Failed health did not restore and restart the exact prior slot.'}
if([Convert]::ToBase64String([IO.File]::ReadAllBytes((Join-Path $root 'teamhjd-install.json'))) -cne [Convert]::ToBase64String($marker) -or (Get-FileHash -LiteralPath (Join-Path $root 'TeamHJDLauncher.exe') -Algorithm SHA256).Hash -cne $bootstrap -or (Get-FileHash -LiteralPath $preserved -Algorithm SHA256).Hash -cne $before){throw 'Rollback changed the prior ownership marker, bootstrap or synthetic data.'}
Write-HostingJson (Join-Path $run 'result.json') @{result='PASS';nativeInstallerHealthGate=$true;syntheticClientHealth=$true;failedNewClientRejected=$true;priorSlotRestarted=$true;ownershipAndBootstrapRestored=$true;failureLatchWritten=$true;privateDataPreserved=$true;realProductGui=$false;humanEmailAuthentication=$false}
Write-Output ('PASS native installer failed-health rollback with isolated synthetic clients: '+$run)
