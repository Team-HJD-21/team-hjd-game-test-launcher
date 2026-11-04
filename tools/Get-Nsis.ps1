#Requires -Version 7.0
param([string]$CacheRoot=(Join-Path (Split-Path $PSScriptRoot) '.deps/nsis'),[switch]$Offline)
$ErrorActionPreference='Stop'
$arguments=@((Join-Path $PSScriptRoot 'get-nsis.py'),'--cache',[IO.Path]::GetFullPath($CacheRoot))
if($Offline){$arguments+='--offline'}
& python @arguments
if($LASTEXITCODE){throw 'Pinned official NSIS verification failed. No compiler was executed.'}
