#Requires -Version 7.0
param([string]$PrivateRoot=(Join-Path (Split-Path $PSScriptRoot) '.local/launcher-publisher'))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'HostingCommon.ps1')
$directory=[IO.Path]::GetFullPath($PrivateRoot)
Assert-HostingPath $directory
if(Test-Path -LiteralPath $directory){throw 'Select a new private directory. Existing signing keys are never replaced.'}
[IO.Directory]::CreateDirectory($directory)|Out-Null
$identity=[Security.Principal.WindowsIdentity]::GetCurrent().User
$security=[Security.AccessControl.DirectorySecurity]::new()
$security.SetAccessRuleProtection($true,$false)
foreach($sid in @($identity,[Security.Principal.SecurityIdentifier]::new('S-1-5-18'))){
    $rule=[Security.AccessControl.FileSystemAccessRule]::new($sid,'FullControl','ContainerInherit, ObjectInherit','None','Allow')
    $security.AddAccessRule($rule)
}
Set-Acl -LiteralPath $directory -AclObject $security
$key=[Security.Cryptography.ECDsa]::Create([Security.Cryptography.ECCurve+NamedCurves]::nistP256)
try{
    $private=$key.ExportPkcs8PrivateKey()
    try{
        $blob=[Security.Cryptography.ProtectedData]::Protect($private,$null,[Security.Cryptography.DataProtectionScope]::CurrentUser)
        [IO.File]::WriteAllBytes((Join-Path $directory 'publisher.dpapi'),$blob)
    }finally{[Array]::Clear($private,0,$private.Length)}
    $parameters=$key.ExportParameters($false)
    $public='p256:'+[Convert]::ToBase64String([byte[]]($parameters.Q.X+$parameters.Q.Y))
    Write-HostingJson (Join-Path $directory 'public.json') @{schema=1;keyId='launcher-p256-2026-11';publicKey=$public}
    Write-Output 'Launcher publisher created. Only public.json may be copied into source; keep the DPAPI file local and private.'
}finally{$key.Dispose()}
