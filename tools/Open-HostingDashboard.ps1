#requires -Version 7.5
[CmdletBinding()]
param(
    [string]$PrivateRoot=(Join-Path (Split-Path $PSScriptRoot) '.local/production-distribution'),
    [switch]$SkipPublicCheck,
    [switch]$ExportCsv,
    [switch]$GenerateOnly
)
$ErrorActionPreference='Stop'
$result=& (Join-Path $PSScriptRoot 'Update-HostingDashboard.ps1') -PrivateRoot $PrivateRoot -SkipPublicCheck:$SkipPublicCheck -ExportCsv:$ExportCsv
if(-not $GenerateOnly){Start-Process -FilePath $result.DashboardPath|Out-Null}
$result
