#Requires -Version 7.0
param([Parameter(Mandatory)][string]$RollbackResult,[Parameter(Mandatory)][string]$GoodBundle)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'HostingCommon.ps1')
$project=[IO.Path]::GetFullPath((Split-Path $PSScriptRoot));$run=[IO.Path]::GetFullPath((Split-Path $RollbackResult))
if(-not $run.StartsWith($project+'\.local\installer-health-rollback-',[StringComparison]::OrdinalIgnoreCase) -or (Read-HostingJson $RollbackResult 16384).result -cne 'PASS'){throw 'An exact isolated rollback fixture is required.'}
$root=Join-Path $run 'TeamHJD Game Test Launcher';$profile=Join-Path $run 'profile';$helper=Join-Path ([IO.Path]::GetFullPath($GoodBundle)) 'TeamHJDLauncher.exe'
Assert-HostingPath $root;Assert-HostingPath $helper
$sentinel=Join-Path $profile 'synthetic-game-and-settings.txt';$before=(Get-FileHash -LiteralPath $sentinel -Algorithm SHA256).Hash
function Run-Helper([string]$Mode,[int]$Expected,[string]$Step){
    $p=Start-Process -FilePath $helper -ArgumentList ($Mode+' "'+$root+'" --data-root "'+$profile+'" --noninteractive-test') -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $run ($Step+'.stdout.log')) -RedirectStandardError (Join-Path $run ($Step+'.stderr.log'))
    if(-not $p.WaitForExit(90000) -or $p.ExitCode -ne $Expected){throw ('Unexpected maintenance result: '+$Step)}
}
$bootstrap=Start-Process -FilePath (Join-Path $root 'TeamHJDLauncher.exe') -ArgumentList ('--data-root "'+$profile+'" --noninteractive-test') -PassThru -WindowStyle Hidden
if(-not $bootstrap.WaitForExit(15000) -or $bootstrap.ExitCode -ne 0){throw 'Synthetic client bootstrap failed.'}
Run-Helper '--uninstall-from' 1 'busy-removal'
if(-not(Test-Path -LiteralPath (Join-Path $root 'TeamHJDLauncher.exe'))){throw 'Busy removal changed the installed launcher.'}
Start-Sleep -Seconds 3
$active=Read-HostingJson (Join-Path $root 'launcher/active.json') 16384
$receipt=Join-Path $root ('launcher/versions/'+$active.current+'/._teamhjd-receipt.json');$bytes=[IO.File]::ReadAllBytes($receipt)
try {
    $newer=Read-HostingJson $receipt 2097152;$newer.version='9.0.0';Write-HostingJson $receipt $newer
    Run-Helper '--install-to' 1 'downgrade'
    if((Read-HostingJson (Join-Path $root 'launcher/active.json') 16384).current -cne $active.current){throw 'Rejected downgrade changed the active slot.'}
} finally {[IO.File]::WriteAllBytes($receipt,$bytes)}
$untrusted=Join-Path $root 'launcher/versions/0.2.0-untrusted-fixture';New-Item -ItemType Directory -Path $untrusted -Force|Out-Null
Write-HostingJson (Join-Path $untrusted '._teamhjd-receipt.json') @{version='0.2.0';executable='TeamHJDClient.exe';files=@(@{path='../../../../profile/synthetic-game-and-settings.txt';bytes=1;sha256=('a'*64)})}
Run-Helper '--uninstall-from' 0 'preserve-untrusted'
if(-not(Test-Path -LiteralPath (Join-Path $untrusted '._teamhjd-receipt.json')) -or (Get-FileHash -LiteralPath $sentinel -Algorithm SHA256).Hash -cne $before){throw 'Uninstall followed or deleted untrusted metadata.'}
Run-Helper '--install-to' 0 'reinstall-with-preserved-files'
if(-not(Test-Path -LiteralPath (Join-Path $root 'TeamHJDLauncher.exe')) -or (Get-FileHash -LiteralPath $sentinel -Algorithm SHA256).Hash -cne $before){throw 'Reinstallation changed preserved data.'}
Write-HostingJson (Join-Path $run 'negative-result.json') @{result='PASS';busyUninstallRejected=$true;downgradeRejected=$true;malformedSlotPreserved=$true;traversingReceiptNotFollowed=$true;reinstallWithUnknownFiles=$true;existingDataPreserved=$true;syntheticOnly=$true}
Write-Output 'PASS busy removal and downgrade rejected; untrusted/traversing receipt preserved; reinstallation and user data verified.'
