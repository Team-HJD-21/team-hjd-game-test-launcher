#Requires -Version 7.0
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'HostingCommon.ps1')
$script:fixtureProcess=$null;$script:fixtureCommand=''
function Get-Process {param([int]$Id) $script:fixtureProcess}
function Get-CimInstance {param([string]$ClassName,[string]$Filter) @{CommandLine=$script:fixtureCommand;CreationDate=$script:fixtureCreation;Name=($script:fixtureProcess.ProcessName+'.exe')}}
$exe='C:\Fixture\Python\python.exe';$time=[DateTime]::UtcNow
$state=@{pid=12345;startedTicks=$time.Ticks.ToString()}
$script:fixtureCreation=$time.AddSeconds(1)
$script:fixtureProcess=[pscustomobject]@{Id=12345;ProcessName='svchost';StartTime=$null;Path=$null}
if($null -ne (Get-OwnedHostingProcess $state $exe @('server.py'))){throw 'A reused protected system PID was adopted.'}
$script:fixtureProcess.ProcessName='python'
$rejected=$false;try{Get-OwnedHostingProcess $state $exe @('server.py')|Out-Null}catch{$rejected=$true}
if(-not $rejected){throw 'An inaccessible matching-name process must remain fail-closed.'}
$script:fixtureProcess.StartTime=$time;$script:fixtureProcess.Path=$exe
$script:fixtureCommand='"C:\Fixture\Python\python.exe" "D:\Fixture\server.py" --root "D:\Fixture\private" --bind 127.0.0.1 --port 8443'
$required=@('D:\Fixture\server.py','--root','D:\Fixture\private','--bind','127.0.0.1','--port','8443')
if($null -eq (Get-OwnedHostingProcess $state $exe $required)){throw 'The fully matching owned process was rejected.'}
$rejected=$false;try{Get-OwnedHostingProcess $state 'C:\Fixture\different.exe' $required|Out-Null}catch{$rejected=$true}
if(-not $rejected){throw 'An exact creation match with the wrong image must stay a conflict.'}
$script:fixtureProcess.StartTime=$time.AddSeconds(-1)
if($null -ne (Get-OwnedHostingProcess $state $exe $required)){throw 'A reused matching-name PID with a different creation time was adopted.'}
$script:fixtureProcess.StartTime=$time
$script:fixtureCommand=$script:fixtureCommand.Replace('D:\Fixture\private','D:\Fixture\other')
$rejected=$false;try{Get-OwnedHostingProcess $state $exe $required|Out-Null}catch{$rejected=$true}
if(-not $rejected){throw 'The same executable with different server arguments was adopted.'}
Write-Output 'PASS 6 hosting process identity regressions; no real processes were stopped.'
