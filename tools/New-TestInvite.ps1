#requires -Version 7.5
[CmdletBinding()]
param(
    [string]$PrivateRoot=(Join-Path (Split-Path $PSScriptRoot) '.local/private-distribution'),
    [Parameter(Mandatory)][ValidateLength(1,120)][string]$Label,
    [ValidateRange(1,30)][int]$Days=7
)
$ErrorActionPreference='Stop'
$OutputEncoding=[Text.UTF8Encoding]::new($false)
[Console]::OutputEncoding=$OutputEncoding
$root=[IO.Path]::GetFullPath($PrivateRoot)
$recipient=$Label.Trim()
if(-not $recipient){throw 'A recipient label is required.'}
function Assert-OrdinaryPath([string]$Path){
    $current=[IO.Path]::GetFullPath($Path)
    while($current){
        if(Test-Path -LiteralPath $current){if((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Invitation storage cannot use linked paths.'}}
        $parent=[IO.Path]::GetDirectoryName($current)
        if($parent -eq $current){break};$current=$parent
    }
}
function Write-AtomicRegistry([hashtable]$Registry,[string]$Path){
    $json=$Registry|ConvertTo-Json -Depth 32 -Compress
    $bytes=[Text.Encoding]::UTF8.GetBytes($json)
    if($bytes.Length -gt 2MB){throw 'Tester registry exceeds the 2 MiB limit.'}
    $temporary=Join-Path $root ('testers-'+[Guid]::NewGuid().ToString('N')+'.tmp')
    try{
        $stream=[IO.FileStream]::new($temporary,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
        try{$stream.Write($bytes,0,$bytes.Length);$stream.Flush($true)}finally{$stream.Dispose()}
        [IO.File]::Move($temporary,$Path,$true)
    }finally{if([IO.File]::Exists($temporary)){[IO.File]::Delete($temporary)}}
}
Assert-OrdinaryPath $root
$templatePath=Join-Path $root 'invite.json'
Assert-OrdinaryPath $templatePath
if(-not [IO.File]::Exists($templatePath) -or (Get-Item -LiteralPath $templatePath).Length -gt 256KB){throw 'Initialize a valid private publisher invite before creating tester invitations.'}
$template=Get-Content -LiteralPath $templatePath -Raw|ConvertFrom-Json -AsHashtable -Depth 32 -DateKind String
if($template.schema -ne 1 -or -not $template.server -or -not $template.publisherKey){throw 'Publisher invite is missing schema, server or public key.'}
$server=[Uri]$template.server
if(($server.Scheme -ne 'https' -and -not($server.IsLoopback -and $server.Scheme -eq 'http')) -or $server.UserInfo -or $server.Query -or $server.Fragment){throw 'Publisher invite must contain an HTTPS server URL.'}
$publicKey=[string]$template.publisherKey
if(-not $publicKey.StartsWith('p256:') -or [Convert]::FromBase64String($publicKey.Substring(5)).Length -ne 64){throw 'Publisher invite has an invalid public key.'}
if($template.Contains('certificate') -and [Text.Encoding]::UTF8.GetByteCount([string]$template.certificate) -gt 128KB){throw 'Publisher certificate is too large.'}
$registryPath=Join-Path $root 'testers.json'
$lockPath=Join-Path $root 'testers.lock'
$inviteDirectory=Join-Path $root 'team-invites'
Assert-OrdinaryPath $registryPath;Assert-OrdinaryPath $lockPath;Assert-OrdinaryPath $inviteDirectory
$lock=$null;$createdPath=$null;$registered=$false;$secret=$null
try{
    try{$lock=[IO.FileStream]::new($lockPath,[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)}catch [IO.IOException]{throw 'Another invitation update is running. Retry after it finishes.'}
    $registry=@{schema=1;testers=@()}
    if([IO.File]::Exists($registryPath)){
        if((Get-Item -LiteralPath $registryPath).Length -gt 2MB){throw 'Tester registry exceeds the 2 MiB limit.'}
        $registry=Get-Content -LiteralPath $registryPath -Raw|ConvertFrom-Json -AsHashtable -Depth 32 -DateKind String
        if($registry.schema -ne 1 -or $registry.testers -isnot [Collections.IList]){throw 'Unsupported tester registry.'}
    }
    $testers=@($registry.testers)
    if($testers.Count -ge 2000){throw 'Tester registry has reached the 2000 invitation limit.'}
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach($entry in $testers){if($entry -isnot [Collections.IDictionary] -or [string]$entry.tokenId -cnotmatch '^[0-9a-f]{16}$' -or -not $seen.Add([string]$entry.tokenId)){throw 'Tester registry contains invalid or duplicate IDs.'}}
    do{$tokenId=[Convert]::ToHexString([Security.Cryptography.RandomNumberGenerator]::GetBytes(8)).ToLowerInvariant()}while($seen.Contains($tokenId))
    $secret=[Security.Cryptography.RandomNumberGenerator]::GetBytes(32)
    $secretText=[Convert]::ToBase64String($secret).TrimEnd('=').Replace('+','-').Replace('/','_')
    $token='hjd1.'+$tokenId+'.'+$secretText
    $now=[DateTimeOffset]::UtcNow
    $expires=$now.AddDays($Days).ToString('o')
    $hash=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($token))).ToLowerInvariant()
    $entry=@{tokenId=$tokenId;recipientLabel=$recipient;tokenSha256=$hash;createdAt=$now.ToString('o');expiresAt=$expires;revoked=$false}
    $invite=@{schema=1;server=[string]$template.server;publisherKey=$publicKey;accessToken=$token;tokenId=$tokenId;expiresAt=$expires;recipientLabel=$recipient}
    if($template.Contains('certificate')){$invite.certificate=$template.certificate}
    New-Item -ItemType Directory -Path $inviteDirectory -Force|Out-Null
    $createdPath=Join-Path $inviteDirectory ('tester-'+$tokenId+'.hjdinvite')
    $inviteBytes=[Text.Encoding]::UTF8.GetBytes(($invite|ConvertTo-Json -Depth 16 -Compress))
    $inviteStream=[IO.FileStream]::new($createdPath,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try{$inviteStream.Write($inviteBytes,0,$inviteBytes.Length);$inviteStream.Flush($true)}finally{$inviteStream.Dispose()}
    $registry.testers=$testers+@($entry)
    Write-AtomicRegistry $registry $registryPath
    $registered=$true
    Write-Output "InvitePath=$createdPath"
    Write-Output "TokenId=$tokenId"
}finally{
    if($secret){[Array]::Clear($secret,0,$secret.Length)}
    $token=$null;$secretText=$null
    if(-not $registered -and $createdPath -and [IO.File]::Exists($createdPath)){[IO.File]::Delete($createdPath)}
    if($lock){$lock.Dispose()}
}
