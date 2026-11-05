#Requires -Version 7.0
param([string]$PrivateRoot=(Join-Path (Split-Path $PSScriptRoot) '.local/production-distribution'),
      [ValidateSet('List','Revoke')][string]$Action='List',
      [ValidatePattern('^[A-Za-z0-9_-]{32}\z')][string]$DeviceId,
      [switch]$Apply,[switch]$ShowEmails,[switch]$AsJson,
      [string]$Python=(Join-Path (Split-Path $PSScriptRoot) '.deps/server-venv/Scripts/python.exe'))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'HostingCommon.ps1')
$root=[IO.Path]::GetFullPath($PrivateRoot);Assert-HostingPath $root
if(-not(Test-Path -LiteralPath $Python -PathType Leaf)){throw 'Use the project server virtual environment.'}
if($Action -eq 'Revoke' -and -not $DeviceId){throw 'Select the exact DeviceId from List before revocation.'}
if($Action -eq 'List' -and ($DeviceId -or $Apply)){throw 'List is read-only.'}
$start=[Diagnostics.ProcessStartInfo]::new([IO.Path]::GetFullPath($Python))
$start.UseShellExecute=$false;$start.CreateNoWindow=$true
$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
foreach($argument in @('-X','utf8',(Join-Path (Split-Path $PSScriptRoot) 'server/device_auth.py'),'--root',$root,'--action',$Action.ToLowerInvariant())){$start.ArgumentList.Add($argument)}
if($DeviceId){$start.ArgumentList.Add('--device-id='+$DeviceId)}
if($Apply){$start.ArgumentList.Add('--apply')}
if($ShowEmails){$start.ArgumentList.Add('--show-emails')}
$process=[Diagnostics.Process]::Start($start)
$stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
$process.WaitForExit();$raw=$stdout.GetAwaiter().GetResult();$privateError=$stderr.GetAwaiter().GetResult()
if($process.ExitCode -ne 0){throw 'Device management failed. Check the private registry and operator lock.'}
if($raw.Length -gt 4*1024*1024){throw 'Unexpected device management result.'}
$result=$raw|ConvertFrom-Json
if($AsJson){$raw}else{$result}
