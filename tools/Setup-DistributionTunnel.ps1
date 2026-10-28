#Requires -Version 7.0
<#
.SYNOPSIS
Creates a locally managed Cloudflare Tunnel and a private runtime configuration.
.DESCRIPTION
Run only after the matching Access application/policy exists. This command opens
the Cloudflare account authorization page, creates a tunnel if needed, and adds
its DNS route. It never starts the connector or overwrites a conflicting DNS
record. Existing tunnel credentials are reused; their contents are never printed.

cloudflared 2026.10.5 tunnel login always writes ~/.cloudflared/cert.pem;
--origincert is a management INPUT option, not a login OUTPUT option. A child-only
HOME confines that temporary certificate to PrivateRoot/cloudflare while the
real Windows USERPROFILE is preserved for the helper's browser launch.
The newly created management profile is removed on exit. This removes the local
certificate, but does not revoke its associated Cloudflare API token.

References:
https://github.com/cloudflare/cloudflared/blob/2026.10.5/cmd/cloudflared/tunnel/login.go
https://developers.cloudflare.com/cloudflare-one/networks/connectors/cloudflare-tunnel/do-more-with-tunnels/local-management/tunnel-permissions/
https://developers.cloudflare.com/cloudflare-one/networks/connectors/cloudflare-tunnel/configure-tunnels/origin-parameters/
#>
param(
    [Parameter(Mandatory)][string]$PrivateRoot,
    [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$AccessAudience,
    [string]$CloudflaredPath=(Join-Path (Split-Path $PSScriptRoot) '.deps/cloudflared/2026.10.5/cloudflared.exe'),
    [ValidatePattern('^[a-zA-Z0-9.-]+$')][string]$Hostname='playtest.teamhjd.com',
    [ValidatePattern('^[a-zA-Z0-9-]+$')][string]$TeamName='morning-bonus-da07',
    [ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$TunnelName='TeamHJD-GameTests',
    [ValidatePattern('^[a-fA-F0-9]{32}$')][string]$ExpectedAccountId='4ac44a3cf5b94099358469a36f0fc88c'
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
if(-not $IsWindows){throw 'This setup template requires Windows.'}

function Assert-NoReparse([string]$Path){
    $cursor=[IO.Path]::GetFullPath($Path)
    while($cursor){
        $item=Get-Item -LiteralPath $cursor -Force -ErrorAction SilentlyContinue
        if($item -and ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Refusing a reparse-point path.'}
        $parent=[IO.Directory]::GetParent($cursor)
        $cursor=if($parent){$parent.FullName}else{$null}
    }
}
function Set-PrivateDirectory([string]$Path){
    Assert-NoReparse $Path
    [IO.Directory]::CreateDirectory($Path)|Out-Null
    if(-not ('TeamHjd.TunnelPrivateDacl' -as [type])){
        Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
namespace TeamHjd {
    public static class TunnelPrivateDacl {
        [DllImport("advapi32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
        static extern bool ConvertStringSecurityDescriptorToSecurityDescriptorW(string text, uint revision, out IntPtr descriptor, out uint size);
        [DllImport("advapi32.dll", SetLastError=true)]
        static extern bool GetSecurityDescriptorDacl(IntPtr descriptor, out bool present, out IntPtr dacl, out bool defaulted);
        [DllImport("advapi32.dll", CharSet=CharSet.Unicode)]
        static extern uint SetNamedSecurityInfoW(string name, uint kind, uint information, IntPtr owner, IntPtr group, IntPtr dacl, IntPtr sacl);
        [DllImport("kernel32.dll")] static extern IntPtr LocalFree(IntPtr pointer);
        [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
        static extern bool RemoveDirectoryW(string path);
        public static void RemoveDirectoryLink(string path) {
            if(!RemoveDirectoryW(path)) throw new Win32Exception(Marshal.GetLastWin32Error());
        }
        public static void Apply(string path, string sid) {
            IntPtr descriptor;
            uint size;
            if(!ConvertStringSecurityDescriptorToSecurityDescriptorW("D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;FA;;;"+sid+")", 1, out descriptor, out size))
                throw new Win32Exception(Marshal.GetLastWin32Error());
            try {
                bool present, defaulted;
                IntPtr dacl;
                if(!GetSecurityDescriptorDacl(descriptor, out present, out dacl, out defaulted) || !present)
                    throw new Win32Exception(Marshal.GetLastWin32Error());
                // SE_FILE_OBJECT, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION.
                // Do not request owner, group or SACL changes/SeSecurityPrivilege.
                uint result=SetNamedSecurityInfoW(path, 1, 0x80000004, IntPtr.Zero, IntPtr.Zero, dacl, IntPtr.Zero);
                if(result!=0) throw new Win32Exception((int)result);
            } finally {LocalFree(descriptor);}
        }
    }
}
'@
    }
    [TeamHjd.TunnelPrivateDacl]::Apply($Path,[Security.Principal.WindowsIdentity]::GetCurrent().User.Value)
}
function New-HelperStartInfo([string[]]$Arguments){
    $info=[Diagnostics.ProcessStartInfo]::new($executable)
    $info.UseShellExecute=$false
    $info.CreateNoWindow=$true
    $info.WorkingDirectory=$cloudflareRoot
    foreach($argument in $Arguments){$info.ArgumentList.Add($argument)}
    # Never change the calling process or user's global environment.
    foreach($key in @($info.Environment.Keys)){
        if($key -match '^(HOME|CFDPATH|TUNNEL_.*|CLOUDFLARED_.*|NO_AUTOUPDATE)$'){$info.Environment.Remove($key)|Out-Null}
    }
    $info.Environment['HOME']=$setupProfile
    $info.Environment['CFDPATH']=$setupProfile
    return $info
}
function Invoke-Helper([string]$Stage,[string[]]$Arguments,[int]$TimeoutSeconds=90){
    $info=New-HelperStartInfo $Arguments
    $info.RedirectStandardOutput=$true
    $info.RedirectStandardError=$true
    $process=[Diagnostics.Process]::new()
    $process.StartInfo=$info
    $started=$false
    try{
        if(-not $process.Start()){throw "Unable to start cloudflared during $Stage."}
        $started=$true
        $stdout=$process.StandardOutput.ReadToEndAsync()
        $stderr=$process.StandardError.ReadToEndAsync()
        $deadline=[DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
        while(-not $process.WaitForExit(100)){
            if([DateTime]::UtcNow -ge $deadline){throw "cloudflared timed out during $Stage."}
        }
        # Never emit streams: login stderr contains a private authorization URL.
        # Only fixed, whitelisted diagnostic categories may leave this process.
        $outputText=$stdout.GetAwaiter().GetResult()+$stderr.GetAwaiter().GetResult()
        if($process.ExitCode -ne 0){
            $cause=switch -Regex ($outputText){
                '(?i)(lookup |getaddrinfo|no such host|name resolution)' {'DNS lookup failed.';break}
                '(?i)(dial tcp|dial udp|connectex|network is unreachable|connection refused|connection reset|i/o timeout)' {'Network connection failed.';break}
                '(?i)(permission denied|access is denied)' {'Local file or process access was denied.';break}
                '(?i)(x509:|TLS handshake|certificate signed by unknown authority)' {'TLS certificate or handshake validation failed.';break}
                '(?i)(flag provided but not defined|unknown flag|incorrect usage)' {'The helper rejected a command option.';break}
                default {'The helper returned an unclassified error.'}
            }
            $outputText=$null
            throw "cloudflared failed during $Stage (exit $($process.ExitCode)). $cause Raw helper output was discarded."
        }
        $outputText=$null
    }finally{
        if($started -and -not $process.HasExited){$process.Kill();$process.WaitForExit()}
        $process.Dispose()
    }
}
function Read-TunnelIdentity([string]$Path){
    Assert-NoReparse $Path
    $credential=Get-Content -LiteralPath $Path -Raw|ConvertFrom-Json
    if($credential.AccountTag -cne $ExpectedAccountId){throw 'Tunnel credentials belong to a different account.'}
    $id=[Guid]::Empty
    if(-not [Guid]::TryParse([string]$credential.TunnelID,[ref]$id) -or $id -eq [Guid]::Empty){throw 'Tunnel credentials have an invalid UUID.'}
    if(-not $credential.TunnelSecret){throw 'Tunnel credentials have no secret.'}
    return $id.ToString()
}
function Quote-Yaml([string]$Value){return "'"+$Value.Replace("'","''")+"'"}
function Find-InternalJunctions([string]$Directory,[string]$OwnedRoot,[Collections.Generic.List[string]]$Links){
    Assert-NoReparse $Directory
    foreach($entry in Get-ChildItem -LiteralPath $Directory -Force){
        if($entry.Attributes -band [IO.FileAttributes]::ReparsePoint){
            if($entry.LinkType -cne 'Junction'){throw 'Unexpected reparse point in the setup directory.'}
            $target=$entry.ResolveLinkTarget($true)
            if(-not $target){throw 'The setup junction target could not be resolved.'}
            $resolved=[IO.Path]::GetFullPath($target.FullName)
            $prefix=[IO.Path]::GetFullPath($OwnedRoot).TrimEnd('\')+'\'
            if(-not $resolved.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase) -and $resolved -cne $prefix.TrimEnd('\')){throw 'The setup junction points outside the owned directory.'}
            $Links.Add($entry.FullName)
        }elseif($entry.PSIsContainer){Find-InternalJunctions $entry.FullName $OwnedRoot $Links}
    }
}

$root=[IO.Path]::GetFullPath($PrivateRoot)
$executable=[IO.Path]::GetFullPath($CloudflaredPath)
Assert-NoReparse $root
Assert-NoReparse $executable
if(-not(Test-Path -LiteralPath $root -PathType Container)){throw 'PrivateRoot must already exist.'}
if((Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash.ToLowerInvariant() -cne 'db4e4c413612a0916f3e1679d3caaf376f94911952d163028fb726fb2e0516a9'){throw 'cloudflared differs from the reviewed official 2026.10.5 executable.'}
$caPath=Join-Path $root 'tls/ca-cert.pem'
Assert-NoReparse $caPath
if(-not(Test-Path -LiteralPath $caPath -PathType Leaf)){throw 'The private origin CA is missing.'}
$cloudflareRoot=Join-Path $root 'cloudflare'
Set-PrivateDirectory $cloudflareRoot
$credentialsPath=Join-Path $cloudflareRoot 'tunnel-credentials.json'
$configPath=Join-Path $cloudflareRoot 'config.yml'
Assert-NoReparse $credentialsPath
Assert-NoReparse $configPath
$setupProfile=Join-Path $cloudflareRoot ('setup-'+[Guid]::NewGuid().ToString('N'))
Set-PrivateDirectory $setupProfile
$setupConfig=Join-Path $setupProfile 'empty.yml'
$managementCert=Join-Path $setupProfile '.cloudflared/cert.pem'
[IO.File]::WriteAllText($setupConfig,"{}`n",[Text.UTF8Encoding]::new($false))
$setupFailed=$false
try{
    Write-Host 'Authorize the teamhjd.com zone in the Cloudflare browser page. Waiting up to five minutes.'
    Invoke-Helper 'account authorization' @('tunnel','--config',$setupConfig,'--no-autoupdate','--loglevel','error','login') 300
    Assert-NoReparse $managementCert
    if(-not(Test-Path -LiteralPath $managementCert -PathType Leaf)){throw 'Cloudflare did not create the isolated account certificate.'}
    # Check the selected account before performing any account mutation.
    $pem=[IO.File]::ReadAllText($managementCert)
    $match=[regex]::Match($pem,'(?s)-----BEGIN ARGO TUNNEL TOKEN-----\s*(.*?)\s*-----END ARGO TUNNEL TOKEN-----')
    if(-not $match.Success){throw 'Unexpected Cloudflare account certificate format.'}
    $decoded=[Convert]::FromBase64String($match.Groups[1].Value)
    try{$account=[Text.Encoding]::UTF8.GetString($decoded)|ConvertFrom-Json;if($account.accountID -cne $ExpectedAccountId){throw 'The authorized Cloudflare account differs from the intended team account.'}}
    finally{[Array]::Clear($decoded,0,$decoded.Length);$pem=$null;$match=$null;$account=$null}
    $managementArgs=@('tunnel','--config',$setupConfig,'--no-autoupdate','--loglevel','error','--origincert',$managementCert)
    if(-not(Test-Path -LiteralPath $credentialsPath)){
        Invoke-Helper 'tunnel creation' ($managementArgs+@('create','--credentials-file',$credentialsPath,$TunnelName))
    }
    $tunnelId=Read-TunnelIdentity $credentialsPath
    $yaml=@"
tunnel: $tunnelId
credentials-file: $(Quote-Yaml $credentialsPath)
no-autoupdate: true
loglevel: info
metrics: 127.0.0.1:0
ingress:
  - hostname: $Hostname
    service: https://localhost:8443
    originRequest:
      originServerName: localhost
      caPool: $(Quote-Yaml $caPath)
      noTLSVerify: false
      access:
        required: true
        teamName: $TeamName
        audTag:
          - $AccessAudience
  - service: http_status:404
"@
    Assert-NoReparse $configPath
    if(Test-Path -LiteralPath $configPath){
        if([IO.File]::ReadAllText($configPath).TrimEnd() -cne $yaml.TrimEnd()){throw 'An existing tunnel configuration differs; it was preserved.'}
    }else{[IO.File]::WriteAllText($configPath,$yaml+"`n",[Text.UTF8Encoding]::new($false))}
    Invoke-Helper 'local ingress validation' @('tunnel','--config',$configPath,'--no-autoupdate','ingress','validate')
    # Intentionally do not use --overwrite-dns; preserve another service's record.
    Invoke-Helper 'DNS route creation' ($managementArgs+@('route','dns',$tunnelId,$Hostname))
    # Starting a connector is a separate operational step. UUID + JSON is enough;
    # the runtime must not be given the account-wide management certificate.
    [pscustomobject]@{Executable=$executable;TunnelId=$tunnelId;ConfigPath=$configPath;CredentialsPath=$credentialsPath;RunArguments=@('tunnel','--config',$configPath,'--no-autoupdate','run',$tunnelId)}
}catch{$setupFailed=$true;throw}finally{
  try{
    # Delete only this invocation's freshly created directory, after checking its
    # resolved scope and every descendant for Windows junctions/symlinks.
    $resolved=[IO.Path]::GetFullPath($setupProfile)
    $expectedPrefix=[IO.Path]::GetFullPath($cloudflareRoot).TrimEnd('\')+'\setup-'
    if(-not $resolved.StartsWith($expectedPrefix,[StringComparison]::OrdinalIgnoreCase)){throw 'Refusing cleanup outside this setup directory.'}
    Assert-NoReparse $resolved
    if(Test-Path -LiteralPath $resolved){
        $junctions=[Collections.Generic.List[string]]::new()
        Find-InternalJunctions $resolved $resolved $junctions
        foreach($junction in $junctions){
            Assert-NoReparse ([IO.Path]::GetDirectoryName($junction))
            [TeamHjd.TunnelPrivateDacl]::RemoveDirectoryLink($junction)
        }
        foreach($entry in Get-ChildItem -LiteralPath $resolved -Recurse -Force){if($entry.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Refusing cleanup containing a reparse point.'}}
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
  }catch{
    if($setupFailed){Write-Warning 'Setup also could not remove its protected temporary directory; the original setup error is preserved.'}
    else{throw}
  }
}
