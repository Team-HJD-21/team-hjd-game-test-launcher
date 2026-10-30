#requires -Version 7.5
[CmdletBinding()]
param(
    [string]$PrivateRoot=(Join-Path (Split-Path $PSScriptRoot) '.local/production-distribution'),
    [switch]$ValidateOnly
)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'HostingCommon.ps1')
$root=[IO.Path]::GetFullPath($PrivateRoot);Assert-HostingPath $root
if(-not [IO.Directory]::Exists($root)){throw 'Private hosting root does not exist.'}
$locks=@();$owned=@()
try{
    # Same locks as startup: stop the watcher before origin/tunnel, preserving the admin service.
    foreach($name in 'hosting-monitor-start.lock','hosting-start.lock'){
        if(-not $ValidateOnly){$locks+=Open-HostingLock (Join-Path $root $name)}
    }
    foreach($component in @(
        @{kind='monitor';file='hosting-monitor.json'},
        @{kind='origin';file='server-process.json'},
        @{kind='tunnel';file='cloudflare/tunnel-process.json'}
    )){
        $path=Join-Path $root $component.file
        if(-not [IO.File]::Exists($path)){continue}
        $state=Read-HostingJson $path 16384
        $executable=[IO.Path]::GetFullPath([string]$(if($state.executable){$state.executable}else{$state.python}))
        Assert-HostingPath $executable
        switch($component.kind){
            'monitor'{
                $script=Join-Path $PSScriptRoot 'Start-PlaytestHosting.ps1'
                if([IO.Path]::GetFileName($executable) -ine 'pwsh.exe' -or $state.script -ine $script){throw 'Unexpected monitor identity; no process was stopped.'}
                $arguments=@($script,'-Monitor','-PrivateRoot',$root)
            }
            'origin'{
                $script=Join-Path (Split-Path $PSScriptRoot) 'server/distribution_server.py'
                if([IO.Path]::GetFileName($executable) -ine 'python.exe' -or $state.bind -ne '127.0.0.1' -or [int]$state.port -lt 1024 -or [int]$state.port -gt 65535){throw 'Unexpected origin identity; no process was stopped.'}
                if($state.script -and $state.script -ine $script){throw 'Unexpected origin script; no process was stopped.'}
                $arguments=@($script,'--root',$root,'--bind','127.0.0.1','--port',[string]$state.port)
            }
            'tunnel'{
                if([IO.Path]::GetFileName($executable) -ine 'cloudflared.exe' -or (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash -ine 'db4e4c413612a0916f3e1679d3caaf376f94911952d163028fb726fb2e0516a9'){throw 'Unexpected tunnel executable; no process was stopped.'}
                $config=[IO.Path]::GetFullPath([string]$state.config)
                if($config -ine (Join-Path $root 'cloudflare/config.yml') -and $config -ine (Join-Path $root 'cloudflare/managed-connector.yml')){throw 'Unexpected tunnel config; no process was stopped.'}
                $arguments=@('tunnel','--config',$config,'run')
                if($state.managedTokenFile){$arguments+='--token-file'}else{
                    if($state.tunnelId -cnotmatch '^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$'){throw 'Unexpected tunnel ID; no process was stopped.'}
                    $arguments+=[string]$state.tunnelId
                }
            }
        }
        $process=Get-OwnedHostingProcess $state $executable $arguments
        if($process){$owned+=@{kind=$component.kind;state=$state;exe=$executable;arguments=$arguments;process=$process}}
    }
    if(-not $ValidateOnly){
        foreach($item in $owned){
            $process=Get-OwnedHostingProcess $item.state $item.exe $item.arguments
            if($process){$process.Kill();if(-not $process.WaitForExit(5000)){throw 'Owned process did not stop in time.'}}
        }
        & (Join-Path $PSScriptRoot 'Update-HostingDashboard.ps1') -PrivateRoot $root -SkipPublicCheck|Out-Null
    }
    [pscustomobject]@{PrivateRoot=$root;Components=@($owned|ForEach-Object {$_.kind});ValidationOnly=[bool]$ValidateOnly;AutostartPreserved=$true;DashboardControlPreserved=$true}
}finally{foreach($lock in $locks){$lock.Dispose()}}
