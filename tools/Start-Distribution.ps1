#requires -Version 7.5
[CmdletBinding()]
param(
    [string]$PrivateRoot=(Join-Path (Split-Path $PSScriptRoot) '.local/production-distribution'),
    [string]$BindAddress='0.0.0.0',
    [ValidateRange(1024,65535)][int]$Port=8443,
    [string]$PythonPath,
    [switch]$ValidateOnly
)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'HostingCommon.ps1')
$root=[IO.Path]::GetFullPath($PrivateRoot)
Assert-HostingPath $root
$project=Split-Path $PSScriptRoot
$serverScript=Join-Path $project 'server/distribution_server.py'
$statePath=Join-Path $root 'server-process.json'
foreach($relative in 'auth.json','catalog.json','tls/server-chain.pem','tls/server-key.pem'){
    $path=Join-Path $root $relative;Assert-HostingPath $path
    if(-not [IO.File]::Exists($path)){throw "Missing distribution file: $relative"}
}
$cfEnabled=$false
$configPath=Join-Path $root 'cloudflare-access.json'
if([IO.File]::Exists($configPath)){
    $config=Read-HostingJson $configPath 16384
    if($config.enabled -isnot [bool]){throw 'Invalid Cloudflare enabled setting.'}
    $cfEnabled=$config.enabled
    if($cfEnabled -and ($config.issuer -cnotmatch '^https://[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\.cloudflareaccess\.com$' -or $config.audience -cnotmatch '^[A-Za-z0-9_-]{16,256}$')){throw 'Invalid Cloudflare issuer or audience.'}
}
if($PythonPath){$python=[IO.Path]::GetFullPath($PythonPath)}
elseif($cfEnabled){$python=Join-Path $project '.deps/server-venv/Scripts/python.exe'}
else{$python=(Get-Command python -ErrorAction Stop).Source}
Assert-HostingPath $python
if(-not [IO.File]::Exists($python)){throw 'Python is missing. For Cloudflare install server/requirements.txt in .deps/server-venv, or pass -PythonPath.'}
if($ValidateOnly){return [pscustomobject]@{PrivateRoot=$root;Python=$python;CloudflareEnabled=$cfEnabled;Bind=$BindAddress;Port=$Port;ServerScript=$serverScript;StatePath=$statePath}}
if($cfEnabled){& $python -c 'import jwt, cryptography' 2>$null;if($LASTEXITCODE -ne 0){throw 'Cloudflare dependencies are missing: install server/requirements.txt in the selected Python environment.'}}
$baseOutput=& $python -c 'import json,sys;print(json.dumps(sys._base_executable))' 2>$null
if($LASTEXITCODE -ne 0){throw 'Cannot determine the selected Python runtime.'}
$basePython=[IO.Path]::GetFullPath(($baseOutput|ConvertFrom-Json));Assert-HostingPath $basePython
if(-not [IO.File]::Exists($basePython)){throw 'Selected Python base executable is missing.'}
$lock=Open-HostingLock (Join-Path $root 'server-start.lock')
try{
    if([IO.File]::Exists($statePath)){
        $state=Read-HostingJson $statePath 16384
        $expectedExecutable=if($state.executable){[string]$state.executable}else{$python}
        if($expectedExecutable -ine $python -and $expectedExecutable -ine $basePython){throw 'Saved origin executable does not belong to the configured Python runtime.'}
        $existing=Get-OwnedHostingProcess $state $expectedExecutable @($serverScript,$root)
        if($existing){
            if($state.python -and $state.python -ine $python){throw 'Existing origin uses a different configured Python environment.'}
            if($state.bind -ne $BindAddress -or [int]$state.port -ne $Port){throw 'Existing owned origin has different bind/port settings. Stop it deliberately before switching configuration.'}
            if(-not ((Get-NetTCPConnection -State Listen -LocalPort $Port -ErrorAction SilentlyContinue).OwningProcess -contains $existing.Id)){throw 'Existing owned origin is alive but has no matching listener.'}
            return [pscustomobject]@{Pid=$existing.Id;StartedTicks=$state.startedTicks;Port=$Port;Bind=$BindAddress;Python=$python;StatePath=$statePath;Reused=$true}
        }
    }
    if(Get-NetTCPConnection -State Listen -LocalPort $Port -ErrorAction SilentlyContinue){throw 'Requested port already belongs to another process.'}
    $logs=New-HostingLogPaths $root 'origin'
    $arguments=@((Quote-HostingArgument $serverScript),'--root',(Quote-HostingArgument $root),'--bind',$BindAddress,'--port',"$Port",'--cert',(Quote-HostingArgument (Join-Path $root 'tls/server-chain.pem')),'--key',(Quote-HostingArgument (Join-Path $root 'tls/server-key.pem')))
    $process=Start-Process -FilePath $python -ArgumentList $arguments -PassThru -WindowStyle Hidden -RedirectStandardOutput $logs.stdout -RedirectStandardError $logs.stderr
    $state=@{pid=$process.Id;startedTicks=$process.StartTime.ToUniversalTime().Ticks.ToString();launcherPid=$process.Id;launcherStartedTicks=$process.StartTime.ToUniversalTime().Ticks.ToString();executable=$python;port=$Port;bind=$BindAddress;python=$python;script=$serverScript;privateRoot=$root;stage='starting';stdout=$logs.stdout;stderr=$logs.stderr}
    try{
        Write-HostingJson $statePath $state
        $deadline=[DateTime]::UtcNow.AddSeconds(12);$listener=$null
        while([DateTime]::UtcNow -lt $deadline){
            Start-Sleep -Milliseconds 150;$process.Refresh()
            if($process.HasExited){throw 'Distribution server exited during startup; inspect its private runtime log.'}
            foreach($ownerId in @((Get-NetTCPConnection -State Listen -LocalPort $Port -ErrorAction SilentlyContinue).OwningProcess|Select-Object -Unique)){
                if($ownerId -eq $process.Id){$listener=$process;break}
                # Windows venv python.exe redirects to a directly spawned base interpreter.
                $child=Get-CimInstance Win32_Process -Filter "ProcessId=$ownerId" -ErrorAction Stop
                if($child -and $child.ParentProcessId -eq $process.Id -and [string]::Equals($child.ExecutablePath,$basePython,[StringComparison]::OrdinalIgnoreCase)){
                    $candidate=Get-Process -Id $ownerId -ErrorAction Stop
                    $childState=@{pid=$ownerId;startedTicks=$candidate.StartTime.ToUniversalTime().Ticks.ToString()}
                    $listener=Get-OwnedHostingProcess $childState $basePython @($serverScript,$root)
                    if($listener){break}
                }
            }
            if($listener){break}
        }
        if(-not $listener){throw 'Distribution listener was not ready in time.'}
        $state.pid=$listener.Id;$state.startedTicks=$listener.StartTime.ToUniversalTime().Ticks.ToString();$state.executable=$listener.Path
        $state.stage='ready';Write-HostingJson $statePath $state
        [pscustomobject]@{Pid=$listener.Id;StartedTicks=$state.startedTicks;Port=$Port;Bind=$BindAddress;Python=$python;StatePath=$statePath;Reused=$false}
    }catch{
        # Only this newly created process object may be stopped on failed startup.
        $process.Refresh();if(-not $process.HasExited){$process.Kill()}
        throw
    }
}finally{$lock.Dispose()}
