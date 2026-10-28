#Requires -Version 7.0
# Compatibility entry point. Since 0.1.2 game updates are detected automatically
# and applied only after the explicit Update action; the old unattended-install
# assertions are intentionally replaced by the current Controller regression.
param(
    [Parameter(Mandatory)][string]$QtRoot,
    [string]$Executable
)
$ErrorActionPreference='Stop'
if($Executable){Write-Warning 'The legacy Executable parameter is unused; the current check runs the dedicated Controller integration harness.'}
& (Join-Path $PSScriptRoot 'verify-manual-update.ps1') -QtRoot $QtRoot
