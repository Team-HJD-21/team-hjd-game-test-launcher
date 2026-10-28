param([Parameter(Mandatory)][string]$QtRoot)
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot
$root=Join-Path $project ('.local/bootstrap-'+[DateTime]::Now.ToString('yyyyMMdd-HHmmss'))
$env:PATH="$QtRoot/bin;$env:PATH"
New-Item -ItemType Directory -Path $root -Force|Out-Null
Copy-Item -LiteralPath (Join-Path $project 'build/TeamHJDLauncher.exe') -Destination $root
function Seed([string]$version,[bool]$bad){
    $directory=Join-Path $root "launcher/versions/$version";New-Item -ItemType Directory -Path $directory -Force|Out-Null
    Copy-Item -LiteralPath (Join-Path $project 'build/LauncherFixtureGame.exe') -Destination (Join-Path $directory 'TeamHJDClient.exe')
    if($bad){'Deliberate failed startup fixture'|Set-Content -LiteralPath (Join-Path $directory 'no-health.flag')}
    $files=@(Get-ChildItem -LiteralPath $directory -File|ForEach-Object{@{path=$_.Name;bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}})
    @{version=$version;executable='TeamHJDClient.exe';files=$files}|ConvertTo-Json -Depth 10 -Compress|Set-Content -LiteralPath (Join-Path $directory '._teamhjd-receipt.json')
}
function Run-Bootstrap([string[]]$Arguments){$p=Start-Process -FilePath (Join-Path $root 'TeamHJDLauncher.exe') -ArgumentList $Arguments -PassThru -WindowStyle Hidden;$p.WaitForExit();if($p.ExitCode){throw "Bootstrap failed: $($p.ExitCode)"}}
Seed '1.0.0' $false;Seed '1.1.0' $false;Seed '1.2.0' $true
@{current='1.0.0';previous=''}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $root 'launcher/active.json')
Run-Bootstrap @('--noninteractive-test')
$running=Get-Content -LiteralPath (Join-Path $root 'launcher/running.json') -Raw|ConvertFrom-Json
Run-Bootstrap @('--activate','1.1.0','--wait-pid',"$($running.pid)",'--noninteractive-test')
$state=Get-Content -LiteralPath (Join-Path $root 'launcher/active.json') -Raw|ConvertFrom-Json
if($state.current -ne '1.1.0' -or $state.previous -ne '1.0.0'){throw 'Successful activation did not switch to new payload.'}
$running=Get-Content -LiteralPath (Join-Path $root 'launcher/running.json') -Raw|ConvertFrom-Json
Run-Bootstrap @('--activate','1.2.0','--wait-pid',"$($running.pid)",'--noninteractive-test')
$state=Get-Content -LiteralPath (Join-Path $root 'launcher/active.json') -Raw|ConvertFrom-Json
if($state.current -ne '1.1.0'){throw 'Failed startup was not rolled back.'}
if(Test-Path -LiteralPath (Join-Path $root 'pending-launcher.json')){throw 'Pending activation not cleared.'}
Start-Sleep -Seconds 3
# Simulate a helper crash after a candidate pointer was committed.
@{current='1.2.0';previous='1.1.0'}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $root 'launcher/active.json')
@{previous='1.1.0';candidate='1.2.0'}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $root 'pending-launcher.json')
Run-Bootstrap @('--noninteractive-test')
$state=Get-Content -LiteralPath (Join-Path $root 'launcher/active.json') -Raw|ConvertFrom-Json
if($state.current -ne '1.1.0'){throw 'Interrupted activation did not recover previous payload.'}
Start-Sleep -Seconds 3
@{candidate='1.0.0'}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $root 'queued-launcher.json')
Run-Bootstrap @('--noninteractive-test')
$state=Get-Content -LiteralPath (Join-Path $root 'launcher/active.json') -Raw|ConvertFrom-Json
if($state.current -ne '1.0.0' -or (Test-Path -LiteralPath (Join-Path $root 'queued-launcher.json'))){throw 'Queued next-launch update was not activated.'}
@{result='PASS';successfulActivation=$true;failedStartupRollback=$true;interruptedActivationRecovery=$true;queuedNextLaunchActivation=$true}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $root 'result.json')
Write-Output "PASS real bootstrap process activation, failed-start rollback and interrupted activation recovery: $root"
