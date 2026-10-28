#requires -Version 7.5
[CmdletBinding()]
param(
    [string]$PrivateRoot=(Join-Path (Split-Path $PSScriptRoot) '.local/production-distribution'),
    [string]$PythonPath,
    [string]$CloudflaredPath=(Join-Path (Split-Path $PSScriptRoot) '.deps/cloudflared/2026.10.5/cloudflared.exe'),
    [string]$TokenFile,
    [ValidatePattern('^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$')][string]$TunnelId='888292a7-043a-468f-bba1-4b914592bfff',
    [ValidateRange(1024,65535)][int]$Port=8443,
    [switch]$NoWatch,
    [switch]$ValidateOnly,
    [switch]$Monitor
)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'HostingCommon.ps1')
$root=[IO.Path]::GetFullPath($PrivateRoot);Assert-HostingPath $root
$originArguments=@{PrivateRoot=$root;BindAddress='127.0.0.1';Port=$Port}
if($PythonPath){$originArguments.PythonPath=[IO.Path]::GetFullPath($PythonPath)}
$originPlan=& (Join-Path $PSScriptRoot 'Start-Distribution.ps1') @originArguments -ValidateOnly
if(-not $originPlan.CloudflareEnabled){throw 'Public hosting requires a valid enabled cloudflare-access.json.'}
$helper=[IO.Path]::GetFullPath($CloudflaredPath);Assert-HostingPath $helper
if(-not [IO.File]::Exists($helper) -or (Get-FileHash -LiteralPath $helper -Algorithm SHA256).Hash -ine 'db4e4c413612a0916f3e1679d3caaf376f94911952d163028fb726fb2e0516a9'){throw 'Use the pinned cloudflared 2026.10.5 helper from Get-Cloudflared.ps1.'}
$cfRoot=Join-Path $root 'cloudflare';Assert-HostingPath $cfRoot
if(-not [IO.Directory]::Exists($cfRoot)){throw 'Missing private cloudflare directory; configure the tunnel first.'}
if(-not $TokenFile){$TokenFile=Join-Path $cfRoot 'tunnel-token.txt'}
$tokenPath=[IO.Path]::GetFullPath($TokenFile);Assert-HostingPath $tokenPath
$managed=[IO.File]::Exists($tokenPath)
$configPath=Join-Path $cfRoot $(if($managed){'managed-connector.yml'}else{'config.yml'})
if($managed){
    if((Get-Item -LiteralPath $tokenPath).Length -lt 16 -or (Get-Item -LiteralPath $tokenPath).Length -gt 16384){throw 'Invalid private tunnel token file size.'}
}else{
    if($PSBoundParameters.ContainsKey('TokenFile')){throw 'Explicit tunnel token file is missing.'}
    $credentials=Read-HostingJson (Join-Path $cfRoot 'tunnel-credentials.json') 16384
    if($credentials.TunnelID -cnotmatch '^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$'){throw 'Invalid tunnel ID in private credentials.'}
    $TunnelId=$credentials.TunnelID
    Assert-HostingPath $configPath
    if(-not [IO.File]::Exists($configPath) -or (Get-Item -LiteralPath $configPath).Length -gt 65536){throw 'Missing or oversized local tunnel configuration.'}
}
$tunnelStatePath=Join-Path $cfRoot 'tunnel-process.json'
$watcherStatePath=Join-Path $root 'hosting-monitor.json'
$self=[IO.Path]::GetFullPath($PSCommandPath)
$pwsh=(Get-Process -Id $PID).Path
if($ValidateOnly){return [pscustomobject]@{PrivateRoot=$root;Python=$originPlan.Python;Bind='127.0.0.1';Port=$Port;Cloudflared=$helper;TunnelId=$TunnelId;ManagedTokenFile=$managed;ConfigPath=$configPath;WatchEverySeconds=30;CatalogCheckHours=24}}

function Start-OwnedTunnel {
    if([IO.File]::Exists($tunnelStatePath)){
        $state=Read-HostingJson $tunnelStatePath 16384
        $required=@($configPath);if($managed){$required+=@('--token-file',$tokenPath)}else{$required+=,$TunnelId}
        $existing=Get-OwnedHostingProcess $state $helper $required
        if($existing){return [pscustomobject]@{Pid=$existing.Id;Reused=$true;StatePath=$tunnelStatePath}}
    }
    if($managed){
        # Use an explicit config, avoiding unrelated per-user cloudflared defaults.
        Assert-HostingPath $configPath
        [IO.File]::WriteAllText($configPath,"no-autoupdate: true`nloglevel: error`nmetrics: 127.0.0.1:0`n",[Text.UTF8Encoding]::new($false))
    }
    $logs=New-HostingLogPaths $root 'tunnel'
    $arguments=@('tunnel','--config',(Quote-HostingArgument $configPath),'--no-autoupdate','--loglevel','error','--metrics','127.0.0.1:0','run')
    if($managed){$arguments+=@('--token-file',(Quote-HostingArgument $tokenPath))}else{$arguments+=,$TunnelId}
    # Clear token/credential environment overrides only in this child process.
    $environment=@{TUNNEL_TOKEN='';TUNNEL_CRED_CONTENTS='';TUNNEL_TOKEN_FILE='';TUNNEL_LOGLEVEL='error';TUNNEL_MANAGEMENT_OUTPUT=''}
    $process=Start-Process -FilePath $helper -ArgumentList $arguments -Environment $environment -PassThru -WindowStyle Hidden -RedirectStandardOutput $logs.stdout -RedirectStandardError $logs.stderr
    $state=@{pid=$process.Id;startedTicks=$process.StartTime.ToUniversalTime().Ticks.ToString();executable=$helper;tunnelId=$TunnelId;config=$configPath;managedTokenFile=$managed;stdout=$logs.stdout;stderr=$logs.stderr;stage='running'}
    try{
        Write-HostingJson $tunnelStatePath $state
        Start-Sleep -Milliseconds 500;$process.Refresh()
        if($process.HasExited){throw 'Tunnel connector exited during startup; inspect its private runtime log.'}
    }catch{$process.Refresh();if(-not $process.HasExited){$process.Kill()};throw}
    # Running is a process health signal. Public HTTPS is checked separately.
    [pscustomobject]@{Pid=$process.Id;Reused=$false;StatePath=$tunnelStatePath}
}
function Ensure-HostingRuntime([bool]$CheckCatalog){
    $runtimeLock=Open-HostingLock (Join-Path $root 'hosting-start.lock')
    try{
        if($CheckCatalog){& (Join-Path $PSScriptRoot 'Renew-DistributionCatalog.ps1') -PrivateRoot $root|Out-Null}
        $origin=& (Join-Path $PSScriptRoot 'Start-Distribution.ps1') @originArguments
        $tunnel=Start-OwnedTunnel
        [pscustomobject]@{OriginPid=$origin.Pid;TunnelPid=$tunnel.Pid;PrivateRoot=$root;Bind='127.0.0.1';Port=$Port}
    }finally{$runtimeLock.Dispose()}
}
function Update-PrivateDashboard {
    try{& (Join-Path $PSScriptRoot 'Update-HostingDashboard.ps1') -PrivateRoot $root|Out-Null}
    catch{Write-Warning ([DateTime]::UtcNow.ToString('o')+' Private dashboard refresh failed; hosting continues.')}
}
if($Monitor){
    $monitorLock=Open-HostingLock (Join-Path $root 'hosting-monitor.lock')
    try{
        $nextCatalog=[DateTime]::MinValue
        while($true){
            $checkCatalog=[DateTime]::UtcNow -ge $nextCatalog
            try{
                Ensure-HostingRuntime $checkCatalog|Out-Null
                if($checkCatalog){$nextCatalog=[DateTime]::UtcNow.AddHours(24)}
            }catch{
                # Do not persist raw exception text, claims, tokens or request URLs.
                Write-Warning ([DateTime]::UtcNow.ToString('o')+' Hosting check failed; inspect private state and component logs. Retrying in 30 seconds.')
            }
            Update-PrivateDashboard
            Start-Sleep -Seconds 30
        }
    }finally{$monitorLock.Dispose()}
    return
}
$runtime=Ensure-HostingRuntime $true
if(-not $NoWatch){
    $startLock=Open-HostingLock (Join-Path $root 'hosting-monitor-start.lock')
    try{
        $existing=$null
        if([IO.File]::Exists($watcherStatePath)){$state=Read-HostingJson $watcherStatePath 16384;$existing=Get-OwnedHostingProcess $state $pwsh @($self,$root,'-Monitor')}
        if(-not $existing){
            $logs=New-HostingLogPaths $root 'hosting-monitor'
            $arguments=@('-NoLogo','-NoProfile','-File',(Quote-HostingArgument $self),'-Monitor','-PrivateRoot',(Quote-HostingArgument $root),'-CloudflaredPath',(Quote-HostingArgument $helper),'-Port',"$Port",'-TunnelId',$TunnelId)
            if($managed){$arguments+=@('-TokenFile',(Quote-HostingArgument $tokenPath))}
            if($PythonPath){$arguments+=@('-PythonPath',(Quote-HostingArgument $originPlan.Python))}
            $existing=Start-Process -FilePath $pwsh -ArgumentList $arguments -PassThru -WindowStyle Hidden -RedirectStandardOutput $logs.stdout -RedirectStandardError $logs.stderr
            try{
                Write-HostingJson $watcherStatePath @{pid=$existing.Id;startedTicks=$existing.StartTime.ToUniversalTime().Ticks.ToString();executable=$pwsh;script=$self;privateRoot=$root;stdout=$logs.stdout;stderr=$logs.stderr}
                Start-Sleep -Milliseconds 300;$existing.Refresh()
                if($existing.HasExited){throw 'Hosting monitor exited during startup; inspect its private runtime log.'}
            }catch{$existing.Refresh();if(-not $existing.HasExited){$existing.Kill()};throw}
        }
        $runtime|Add-Member NoteProperty MonitorPid $existing.Id
    }finally{$startLock.Dispose()}
}
Update-PrivateDashboard
$runtime
