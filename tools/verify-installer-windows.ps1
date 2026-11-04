#Requires -Version 7.0
param([Parameter(Mandatory)][string]$Installer,[Parameter(Mandatory)][string]$Fixture)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'HostingCommon.ps1')
$project=Split-Path $PSScriptRoot
$run=Join-Path $project ('.local/installer-verification-'+[DateTime]::Now.ToString('yyyyMMdd-HHmmss-fff'))
$root=Join-Path $run 'TeamHJD Game Test Launcher'
$profile=Join-Path $run 'profile'
$registry='HKCU:/Software/Microsoft/Windows/CurrentVersion/Uninstall/TeamHJDGameTestLauncher-Test'
if(Test-Path -LiteralPath $registry){throw 'A prior test registration exists. Inspect it before starting another fixture.'}
$setup=[IO.Path]::GetFullPath($Installer)
if(-not $setup.EndsWith('-Setup-test.exe',[StringComparison]::Ordinal)){throw 'Only an explicitly built test installer can use this isolated registry harness.'}
New-Item -ItemType Directory -Path $profile -Force|Out-Null
$fixturePath=[IO.Path]::GetFullPath($Fixture)
$game=Join-Path $profile 'games/hose-for-hire/versions/1.0.0'
New-Item -ItemType Directory -Path $game -Force|Out-Null
Copy-Item -LiteralPath $fixturePath -Destination (Join-Path $game 'Fixture.exe')
Write-HostingJson (Join-Path $game '._teamhjd-receipt.json') @{version='1.0.0';executable='Fixture.exe';platform='windows-x64';files=@(@{path='Fixture.exe';bytes=(Get-Item -LiteralPath $fixturePath).Length;sha256=(Get-FileHash -LiteralPath $fixturePath -Algorithm SHA256).Hash.ToLowerInvariant()})}
Write-HostingJson (Join-Path $profile 'games/hose-for-hire/active.json') @{current='1.0.0';previous=''}
Write-HostingJson (Join-Path $profile 'settings.json') @{syntheticPreservationMarker='installer-test-only';gamesRoot=(Join-Path $profile 'games')}
$before=@{};Get-ChildItem -LiteralPath $profile -Recurse -File|ForEach-Object {$before[$_.FullName]=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
function Assert-ProfilePreserved{
    foreach($path in $before.Keys){if(-not(Test-Path -LiteralPath $path) -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -cne $before[$path]){throw 'A synthetic game/profile file was changed by program maintenance.'}}
}
function Install-Test{
    $process=Start-Process -FilePath $setup -ArgumentList @('/S',('/D='+$root)) -PassThru -WindowStyle Hidden
    if(-not $process.WaitForExit(180000) -or $process.ExitCode -ne 0){throw 'Silent test installation failed.'}
    $marker=Read-HostingJson (Join-Path $root 'teamhjd-install.json') 16384
    if($marker.product -cne 'com.teamhjd.game-test-launcher'){throw 'Install ownership marker differs.'}
    $active=Read-HostingJson (Join-Path $root 'launcher/active.json') 16384
    $receipt=Read-HostingJson (Join-Path $root ('launcher/versions/'+$active.current+'/._teamhjd-receipt.json')) 2097152
    if($receipt.version -cne $marker.version){throw 'Installed version differs from the package.'}
    foreach($file in $receipt.files){$path=Join-Path $root ('launcher/versions/'+$active.current+'/'+$file.path);Assert-HostingPath $path;if((Get-Item -LiteralPath $path).Length -ne $file.bytes -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $file.sha256){throw 'Installed file hash differs.'}}
    $registration=Get-ItemProperty -LiteralPath $registry
    if($registration.DisplayVersion -cne $receipt.version -or $registration.InstallLocation -ine $root -or $registration.UninstallString -notmatch 'Uninstall\.exe'){throw 'Installed Apps registration differs.'}
    Assert-ProfilePreserved
    return $active.current
}
$first=Install-Test
$second=Install-Test
if($first -ceq $second){throw 'Repair installation overwrote the existing version slot.'}
$state=Read-HostingJson (Join-Path $root 'launcher/active.json') 16384
if($state.previous -cne $first){throw 'Repair installation lost the prior version.'}
$unknown=Join-Path $root 'user-extra-file.txt';[IO.File]::WriteAllText($unknown,'preserve-unknown-user-file',[Text.Encoding]::UTF8)
$uninstaller=Join-Path $root 'Uninstall.exe'
$process=Start-Process -FilePath $uninstaller -ArgumentList '/S' -PassThru -WindowStyle Hidden
if(-not $process.WaitForExit(180000)){throw 'Uninstaller did not finish.'}
$deadline=[DateTime]::UtcNow.AddSeconds(45)
while((Test-Path -LiteralPath $registry) -or (Test-Path -LiteralPath (Join-Path $root 'TeamHJDLauncher.exe'))){if([DateTime]::UtcNow -ge $deadline){throw 'Uninstaller left the application or registration active.'};Start-Sleep -Milliseconds 200}
Assert-ProfilePreserved
if([IO.File]::ReadAllText($unknown) -cne 'preserve-unknown-user-file'){throw 'Uninstaller removed an unknown user file.'}
$third=Install-Test
Assert-ProfilePreserved
$proof=@{result='PASS';version=(Read-HostingJson (Join-Path $root 'teamhjd-install.json') 16384).version;install=$true;repairAppendsNewSlot=$true;previousRetained=$true;uninstall=$true;reinstall=$true;registeredInInstalledApps=$true;syntheticGameAndSettingsPreserved=$true;unknownRootFilePreserved=$true;humanEmailAuthentication=$false;root=$root;profile=$profile}
Write-HostingJson (Join-Path $run 'result.json') $proof
Write-Output ('PASS installation, repair, uninstallation and reinstallation: '+$run)
