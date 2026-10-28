param(
    [string]$PrivateRoot=(Join-Path (Split-Path $PSScriptRoot) '.local/private-distribution'),
    [Parameter(Mandatory)][string]$ServerUrl,
    [string]$PasswordEnvironmentVariable='TEAMHJD_DISTRIBUTION_PASSWORD',
    [string]$CertificatePath
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath($PrivateRoot)
if(Test-Path -LiteralPath (Join-Path $root 'publisher.dpapi')){throw 'Publisher already exists. Preserve its key; do not overwrite it.'}
$uri=[Uri]$ServerUrl
if($uri.Scheme -ne 'https' -and -not ($uri.IsLoopback -and $uri.Scheme -eq 'http')){throw 'Use HTTPS. HTTP is only for isolated loopback tests.'}
if($uri.AbsolutePath -ne '/' -or $uri.UserInfo -or $uri.Query -or $uri.Fragment){throw 'Use a root server URL without credentials, query or fragment.'}
$password=[Environment]::GetEnvironmentVariable($PasswordEnvironmentVariable)
if([string]::IsNullOrEmpty($password)){throw "Set the $PasswordEnvironmentVariable process environment variable without placing a password in command history."}
if([Text.Encoding]::UTF8.GetByteCount($password) -gt 1024){throw 'Password too long.'}
New-Item -ItemType Directory -Path $root,(Join-Path $root 'packages') -Force | Out-Null
$key=[Security.Cryptography.ECDsa]::Create([Security.Cryptography.ECCurve+NamedCurves]::nistP256)
try {
    $private=$key.ExportPkcs8PrivateKey()
    $blob=[Security.Cryptography.ProtectedData]::Protect($private,$null,[Security.Cryptography.DataProtectionScope]::CurrentUser)
    [IO.File]::WriteAllBytes((Join-Path $root 'publisher.dpapi'),$blob)
    [Array]::Clear($private,0,$private.Length)
    $public=$key.ExportParameters($false)
    $publicKey='p256:'+[Convert]::ToBase64String([byte[]]($public.Q.X+$public.Q.Y))
    $salt=[Security.Cryptography.RandomNumberGenerator]::GetBytes(32)
    $iterations=210000
    $hash=[Security.Cryptography.Rfc2898DeriveBytes]::Pbkdf2([Text.Encoding]::UTF8.GetBytes($password),$salt,$iterations,[Security.Cryptography.HashAlgorithmName]::SHA256,32)
    @{salt=[Convert]::ToBase64String($salt);hash=[Convert]::ToBase64String($hash);iterations=$iterations}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $root 'auth.json') -Encoding utf8NoBOM
    $invite=@{schema=1;server=$uri.AbsoluteUri;publisherKey=$publicKey}
    if($CertificatePath){$invite.certificate=[IO.File]::ReadAllText([IO.Path]::GetFullPath($CertificatePath))}
    $invite|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $root 'invite.json') -Encoding utf8NoBOM
    Write-Output "Publisher created in private storage. Share invite.json through a trusted team channel; share the password separately."
} finally { $key.Dispose(); $password=$null }
