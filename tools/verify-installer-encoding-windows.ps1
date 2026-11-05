#Requires -Version 7.0
param([Parameter(Mandatory)][string]$Executable,[string]$QtRoot)
$ErrorActionPreference='Stop'
if(-not $IsWindows){throw 'Windows native installer encoding acceptance is required.'}
. (Join-Path $PSScriptRoot 'HostingCommon.ps1')
$project=Split-Path $PSScriptRoot
$exe=[IO.Path]::GetFullPath($Executable);Assert-HostingPath $exe
if($QtRoot){$env:PATH=(Join-Path ([IO.Path]::GetFullPath($QtRoot)) 'bin')+';'+$env:PATH}
$run=Join-Path $project ('.local/installer-encoding-'+[DateTime]::Now.ToString('yyyyMMdd-HHmmss-fff'))
$root=Join-Path $run 'TeamHJD Game Test Launcher'
New-Item -ItemType Directory -Path (Join-Path $root 'launcher') -Force|Out-Null
Write-HostingJson (Join-Path $root 'launcher/active.json') @{current='0.2.2'}
$start=[Diagnostics.ProcessStartInfo]::new($exe)
$start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.ErrorDialog=$false;$start.RedirectStandardError=$true
$stamp=[Diagnostics.Process]::GetCurrentProcess().StartTime.ToUniversalTime().ToFileTimeUtc()
foreach($arg in @('--wait-client',$root,'--wait-pid',([string]$PID),'--wait-stamp',([string]$stamp),'--noninteractive-test','--installer-unicode-output')){$start.ArgumentList.Add($arg)}
$process=[Diagnostics.Process]::Start($start)
if(-not $process.WaitForExit(20000)){$process.Kill();$process.WaitForExit();throw 'The owned noninteractive helper timed out.'}
$memory=[IO.MemoryStream]::new()
try{$process.StandardError.BaseStream.CopyTo($memory);$bytes=$memory.ToArray()}finally{$memory.Dispose()}
if($process.ExitCode -ne 1 -or $bytes.Length -lt 4 -or $bytes.Length%2 -ne 0 -or $bytes[0] -ne 255 -or $bytes[1] -ne 254){throw 'Installer errors must be even-length BOM-prefixed UTF-16LE.'}
$strictUnicode=[Text.UnicodeEncoding]::new($false,$true,$true)
$text=$strictUnicode.GetString($bytes,2,$bytes.Length-2)
if(-not $text.Contains('업데이트를 요청한 런처를 확인하지 못했습니다.') -or -not $text.EndsWith("`n")){throw 'The Korean native error or its line ending was corrupted.'}
Write-HostingJson (Join-Path $run 'result.json') @{result='PASS';encoding='UTF-16LE with BOM';bytes=$bytes.Length;evenByteCount=$true;strictUnicodeDecoded=$true;wrongLiveImageRejected=$true;nativeKoreanTextExact=$true;lineEndingPreserved=$true}
Write-Output ('PASS wrong live image rejected; native Korean error and line ending preserved: '+$run)
