#Requires -Version 7.0
param([Parameter(Mandatory)][string]$PrivateRoot,[string]$Email,[switch]$Apply,
      [string]$Python=(Join-Path (Split-Path $PSScriptRoot) '.deps/server-venv/Scripts/python.exe'))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'HostingCommon.ps1')
$root=[IO.Path]::GetFullPath($PrivateRoot);Assert-HostingPath $root
if(-not(Test-Path -LiteralPath $Python -PathType Leaf)){throw 'Use the project server virtual environment with server/requirements.txt.'}
$start=[Diagnostics.ProcessStartInfo]::new([IO.Path]::GetFullPath($Python))
$start.UseShellExecute=$false;$start.CreateNoWindow=$true
$start.RedirectStandardInput=$true;$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
foreach($argument in @('-X','utf8',(Join-Path (Split-Path $PSScriptRoot) 'server/manage_verified_users.py'),'--root',$root)){$start.ArgumentList.Add($argument)}
if($Apply){$start.ArgumentList.Add('--apply')}
if($PSBoundParameters.ContainsKey('Email')){$start.ArgumentList.Add('--delete-email-stdin')}
$process=[Diagnostics.Process]::Start($start)
if($PSBoundParameters.ContainsKey('Email')){$process.StandardInput.WriteLine($Email)}
$process.StandardInput.Close()
$stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
$process.WaitForExit();$raw=$stdout.GetAwaiter().GetResult();$errorText=$stderr.GetAwaiter().GetResult()
if($process.ExitCode -ne 0){throw 'Verified-user maintenance failed. Existing records were preserved; inspect the private storage and operator lock.'}
if($raw.Length -gt 4096){throw 'Unexpected maintenance result.'}
$result=$raw|ConvertFrom-Json -AsHashtable
if($Apply -and $result.applied -and (Test-Path -LiteralPath (Join-Path $root 'dashboard/index.html'))){
    & (Join-Path $PSScriptRoot 'Update-HostingDashboard.ps1') -PrivateRoot $root | Out-Null
}
# Contains counts only. No email, JWT, session hash or authentication material.
$result|ConvertTo-Json -Compress
