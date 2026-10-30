#requires -Version 7.5
[CmdletBinding()]
param(
    [string]$PrivateRoot=(Join-Path (Split-Path $PSScriptRoot) '.local/production-distribution'),
    [switch]$SkipPublicCheck,
    [switch]$ExportCsv,
    [switch]$GenerateOnly
)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'HostingCommon.ps1')
$root=[IO.Path]::GetFullPath($PrivateRoot);Assert-HostingPath $root
if(-not $GenerateOnly){
    $gamePort=8443
    foreach($settingsFile in 'hosting-launch.json','server-process.json'){
        $settingsPath=Join-Path $root $settingsFile
        if([IO.File]::Exists($settingsPath)){
            $settings=Read-HostingJson $settingsPath 16384
            if([int]$settings.Port -lt 1024 -or [int]$settings.Port -gt 65535){throw 'Invalid saved game port.'}
            $gamePort=[int]$settings.Port;break
        }
    }
    $control=& (Join-Path $PSScriptRoot 'Start-HostingDashboardControl.ps1') -PrivateRoot $root -GamePort $gamePort
}
$result=& (Join-Path $PSScriptRoot 'Update-HostingDashboard.ps1') -PrivateRoot $PrivateRoot -SkipPublicCheck:$SkipPublicCheck -ExportCsv:$ExportCsv
if(-not $GenerateOnly){
    . (Join-Path $PSScriptRoot 'HostingCommon.ps1')
    $root=[IO.Path]::GetFullPath($PrivateRoot)
    $handoff=[Convert]::ToHexString([Security.Cryptography.RandomNumberGenerator]::GetBytes(32)).ToLowerInvariant()
    $digest=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::ASCII.GetBytes($handoff))).ToLowerInvariant()
    Write-HostingJson (Join-Path $root 'dashboard-open.json') @{sha256=$digest;expiresAt=[DateTimeOffset]::UtcNow.AddSeconds(60).ToUnixTimeSeconds();used=$false}
    Start-Process -FilePath ('http://127.0.0.1:'+ $control.Port +'/open/'+$handoff)|Out-Null
}
$result
