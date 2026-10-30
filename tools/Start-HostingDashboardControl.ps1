#requires -Version 7.5
[CmdletBinding()]
param(
    [string]$PrivateRoot=(Join-Path (Split-Path $PSScriptRoot) '.local/production-distribution'),
    [ValidateRange(1024,65535)][int]$Port=8844,
    [ValidateRange(1024,65535)][int]$GamePort=8443,
    [switch]$ValidateOnly
)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'HostingCommon.ps1')
$root=[IO.Path]::GetFullPath($PrivateRoot);Assert-HostingPath $root
if(-not [IO.Directory]::Exists($root)){throw 'Private hosting root does not exist.'}
$script=Join-Path (Split-Path $PSScriptRoot) 'server/hosting_control.py'
$statePath=Join-Path $root 'dashboard-control.json'
$tokenPath=Join-Path $root 'dashboard-control-token.txt'
$python=(Get-Command python -ErrorAction Stop).Source;Assert-HostingPath $python
$pwsh=(Get-Process -Id $PID).Path
if($ValidateOnly){return [pscustomobject]@{PrivateRoot=$root;Bind='127.0.0.1';Port=$Port;GamePort=$GamePort;ValidationOnly=$true}}
$baseOutput=& $python -c 'import json,sys;print(json.dumps(sys._base_executable))' 2>$null
if($LASTEXITCODE -ne 0){throw 'Cannot determine the local dashboard Python runtime.'}
$executable=[IO.Path]::GetFullPath(($baseOutput|ConvertFrom-Json));Assert-HostingPath $executable
$required=@($script,'--root',$root,'--port',[string]$Port,'--game-port',[string]$GamePort,'--pwsh',$pwsh)
$lock=Open-HostingLock (Join-Path $root 'dashboard-control-start.lock')
try{
    if([IO.File]::Exists($statePath)){
        $state=Read-HostingJson $statePath 16384
        $existing=Get-OwnedHostingProcess $state $executable $required
        if($existing){
            if(-not ((Get-NetTCPConnection -State Listen -LocalPort $Port -ErrorAction SilentlyContinue).OwningProcess -contains $existing.Id)){throw 'Owned dashboard control is alive without its listener.'}
            return [pscustomobject]@{Port=$Port;Pid=$existing.Id;Reused=$true;PrivateRoot=$root}
        }
    }
    if(Get-NetTCPConnection -State Listen -LocalPort $Port -ErrorAction SilentlyContinue){throw 'Dashboard control port is occupied; no existing process was stopped.'}
    $token=[Convert]::ToHexString([Security.Cryptography.RandomNumberGenerator]::GetBytes(32)).ToLowerInvariant()
    Assert-HostingPath $tokenPath
    [IO.File]::WriteAllText($tokenPath,$token,[Text.UTF8Encoding]::new($false))
    $logs=New-HostingLogPaths $root 'dashboard-control'
    $arguments=@((Quote-HostingArgument $script),'--root',(Quote-HostingArgument $root),'--port',[string]$Port,'--game-port',[string]$GamePort,'--pwsh',(Quote-HostingArgument $pwsh))
    $process=Start-Process -FilePath $executable -ArgumentList $arguments -PassThru -WindowStyle Hidden -RedirectStandardOutput $logs.stdout -RedirectStandardError $logs.stderr
    try{
        Write-HostingJson $statePath @{pid=$process.Id;startedTicks=$process.StartTime.ToUniversalTime().Ticks.ToString();executable=$executable;script=$script;privateRoot=$root;port=$Port;gamePort=$GamePort;pwsh=$pwsh;stdout=$logs.stdout;stderr=$logs.stderr}
        $ready=$false;$deadline=[DateTime]::UtcNow.AddSeconds(10)
        while([DateTime]::UtcNow -lt $deadline){
            Start-Sleep -Milliseconds 150;$process.Refresh()
            if($process.HasExited){throw 'Private dashboard control exited during startup.'}
            if((Get-NetTCPConnection -State Listen -LocalPort $Port -ErrorAction SilentlyContinue).OwningProcess -contains $process.Id){$ready=$true;break}
        }
        if(-not $ready){throw 'Private dashboard listener was not ready.'}
        [pscustomobject]@{Port=$Port;Pid=$process.Id;Reused=$false;PrivateRoot=$root}
    }catch{$process.Refresh();if(-not $process.HasExited){$process.Kill()};throw}
}finally{$lock.Dispose()}
