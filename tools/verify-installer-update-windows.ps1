#Requires -Version 7.0
param([Parameter(Mandatory)][string]$VerificationResult,[Parameter(Mandatory)][string]$Installer,[switch]$ShowSetup,[switch]$ExpectRollback)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'HostingCommon.ps1')
$project=[IO.Path]::GetFullPath((Split-Path $PSScriptRoot))
$evidence=Read-HostingJson ([IO.Path]::GetFullPath($VerificationResult)) 32768
$root=[IO.Path]::GetFullPath($evidence.root);$profile=[IO.Path]::GetFullPath($evidence.profile)
if(-not $root.StartsWith($project+'\.local\installer-verification-',[StringComparison]::OrdinalIgnoreCase) -or -not $evidence.syntheticGameAndSettingsPreserved){throw 'Only the isolated installer preservation fixture is accepted.'}
Assert-HostingPath $root;Assert-HostingPath $profile
$setup=[IO.Path]::GetFullPath($Installer)
if(-not $setup.EndsWith('-Setup-test.exe',[StringComparison]::Ordinal)){throw 'A separate test-registration Setup is required.'}
$registry='HKCU:/Software/Microsoft/Windows/CurrentVersion/Uninstall/TeamHJDGameTestLauncher-Test'
if((Get-ItemProperty -LiteralPath $registry).InstallLocation -ine $root){throw 'Unexpected test installation registration.'}
function Owned-Client {
    $running=Read-HostingJson (Join-Path $root 'launcher/running.json') 16384
    $exe=[IO.Path]::GetFullPath((Join-Path $root ('launcher/versions/'+$running.folder+'/'+$running.executable)))
    $ticks=[DateTime]::FromFileTimeUtc([long]$running.stamp).Ticks.ToString()
    Get-OwnedHostingProcess @{pid=$running.pid;startedTicks=$ticks} $exe @('--launcher-root',$root.Replace('\','/'),'--data-root',$profile.Replace('\','/'),'--health-file')
}
function Wait-Client([string]$OldFolder='') {
    $deadline=[DateTime]::UtcNow.AddSeconds(45)
    do {
        try {
            $state=Read-HostingJson (Join-Path $root 'launcher/active.json') 16384
            if(-not $OldFolder -or $state.current -cne $OldFolder){$owned=Owned-Client;if($owned){return $owned}}
        }catch {if([DateTime]::UtcNow -ge $deadline){throw}}
        Start-Sleep -Milliseconds 150
    }while([DateTime]::UtcNow -lt $deadline)
    throw 'No fresh owned client health receipt.'
}
$beforeSettings=(Get-FileHash -LiteralPath (Join-Path $profile 'settings.json') -Algorithm SHA256).Hash
$beforeGame=(Get-FileHash -LiteralPath (Join-Path $profile 'games/hose-for-hire/versions/1.0.0/Fixture.exe') -Algorithm SHA256).Hash
$beforeUninstaller=(Get-FileHash -LiteralPath (Join-Path $root 'Uninstall.exe') -Algorithm SHA256).Hash
if(Test-Path -LiteralPath (Join-Path $root 'launcher/running.json')){$existing=Owned-Client;if($existing){$existing.Kill();$existing.WaitForExit()}}
$start=Start-Process -FilePath (Join-Path $root 'TeamHJDLauncher.exe') -ArgumentList ('--data-root "'+$profile.Replace('\','/')+'" --noninteractive-test') -PassThru -WindowStyle Hidden
if(-not $start.WaitForExit(45000) -or $start.ExitCode -ne 0){throw 'Stable bootstrap failed before update.'}
$client=Wait-Client
$retainedParent=$client.SafeHandle;$parentHandleRetained=$false
$retainedParent.DangerousAddRef([ref]$parentHandleRetained)
try {
    $original=Read-HostingJson (Join-Path $root 'launcher/active.json') 16384
    $running=Read-HostingJson (Join-Path $root 'launcher/running.json') 16384
    $arguments=('/UPDATE /WAITPID='+$client.Id+' /WAITSTAMP='+$running.stamp+' /PROFILE="'+$profile+'" /D='+$root)
    if(-not $ShowSetup){$arguments='/S '+$arguments}
    $update=Start-Process -FilePath $setup -ArgumentList $arguments -PassThru -WindowStyle Hidden
    Start-Sleep -Milliseconds 500
    $owned=Owned-Client;if(-not $owned){throw 'Update parent disappeared before controlled shutdown.'}
    $owned.Kill();$owned.WaitForExit()
    if(-not $update.WaitForExit(180000)){throw 'Automatic-mode Setup timed out.'}
    if($ExpectRollback){
        if($update.ExitCode -eq 0){throw 'The bad-health package unexpectedly succeeded.'}
        $client=Wait-Client
        $active=Read-HostingJson (Join-Path $root 'launcher/active.json') 16384
        if($active.current -cne $original.current -or -not(Test-Path -LiteralPath (Join-Path $root 'launcher/failed-release.json'))){throw 'Failed Setup did not restore its previous version and record the failure latch.'}
        if((Get-FileHash -LiteralPath (Join-Path $profile 'settings.json') -Algorithm SHA256).Hash -cne $beforeSettings -or (Get-FileHash -LiteralPath (Join-Path $profile 'games/hose-for-hire/versions/1.0.0/Fixture.exe') -Algorithm SHA256).Hash -cne $beforeGame -or (Get-FileHash -LiteralPath (Join-Path $root 'Uninstall.exe') -Algorithm SHA256).Hash -cne $beforeUninstaller){throw 'Failed update changed preserved user data or the prior uninstaller.'}
        Write-HostingJson (Join-Path (Split-Path $root) 'failed-update-result.json') @{result='PASS';badHealthRejected=$true;previousClientRestarted=$true;activeSlotRestored=$true;failedReleaseLatch=$true;humanEmailAuthentication=$false}
        Write-Output 'PASS bad-health Setup rejected; exact prior GUI restarted and failure latch recorded.'
        return
    }
    if($update.ExitCode -ne 0){throw ('Automatic-mode Setup failed, exit='+$update.ExitCode)}
    $client=Wait-Client $original.current
    $active=Read-HostingJson (Join-Path $root 'launcher/active.json') 16384
    if($active.previous -cne $original.current){throw 'Automatic update lost its rollback slot.'}
    foreach($name in 'bootstrap-update.json','pending-launcher.json','launcher/failed-release.json'){if(Test-Path -LiteralPath (Join-Path $root $name)){throw 'Update left pending or failed health metadata.'}}
    $receipt=Read-HostingJson (Join-Path $root ('launcher/versions/'+$active.current+'/._teamhjd-receipt.json')) 2097152
    if((Get-ItemProperty -LiteralPath $registry).DisplayVersion -cne $receipt.version){throw 'Installed Apps version was not updated.'}
    Write-Output 'PASS real Setup update waits for its exact parent, appends a slot, starts the new GUI and commits its health acknowledgment.'
} finally {
    $owned=Owned-Client;if($owned){$owned.Kill();$owned.WaitForExit()}
    if($parentHandleRetained){$retainedParent.DangerousRelease()}
}
if((Get-FileHash -LiteralPath (Join-Path $profile 'settings.json') -Algorithm SHA256).Hash -cne $beforeSettings -or (Get-FileHash -LiteralPath (Join-Path $profile 'games/hose-for-hire/versions/1.0.0/Fixture.exe') -Algorithm SHA256).Hash -cne $beforeGame){throw 'Automatic maintenance changed a preexisting profile or game.'}
Write-HostingJson (Join-Path (Split-Path $root) 'update-result.json') @{result='PASS';realInstalledGui=$true;exactParentWait=$true;setupAppliesAndRestarts=$true;healthAcknowledged=$true;previousSlotPreserved=$true;installedAppsVersion=$true;existingGameAndSettingsPreserved=$true;humanEmailAuthentication=$false}
