#requires -Version 7.5
[CmdletBinding()]
param(
    [string]$PrivateRoot=(Join-Path (Split-Path $PSScriptRoot) '.local/private-distribution'),
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{16}$')][string]$TokenId
)
$ErrorActionPreference='Stop'
$OutputEncoding=[Text.UTF8Encoding]::new($false)
[Console]::OutputEncoding=$OutputEncoding
if($TokenId -cnotmatch '^[0-9a-f]{16}$'){throw 'Use the exact lower-case invitation ID.'}
$root=[IO.Path]::GetFullPath($PrivateRoot)
function Assert-OrdinaryPath([string]$Path){
    $current=[IO.Path]::GetFullPath($Path)
    while($current){
        if(Test-Path -LiteralPath $current){if((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Invitation storage cannot use linked paths.'}}
        $parent=[IO.Path]::GetDirectoryName($current)
        if($parent -eq $current){break};$current=$parent
    }
}
$registryPath=Join-Path $root 'testers.json';$lockPath=Join-Path $root 'testers.lock'
Assert-OrdinaryPath $registryPath;Assert-OrdinaryPath $lockPath
$lock=$null;$temporary=$null
try{
    try{$lock=[IO.FileStream]::new($lockPath,[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)}catch [IO.IOException]{throw 'Another invitation update is running. Retry after it finishes.'}
    if(-not [IO.File]::Exists($registryPath) -or (Get-Item -LiteralPath $registryPath).Length -gt 2MB){throw 'A bounded tester registry is required.'}
    $registry=Get-Content -LiteralPath $registryPath -Raw|ConvertFrom-Json -AsHashtable -Depth 32 -DateKind String
    if($registry.schema -ne 1 -or $registry.testers -isnot [Collections.IList] -or $registry.testers.Count -gt 2000){throw 'Unsupported tester registry.'}
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal);$selected=$null
    foreach($entry in $registry.testers){
        if($entry -isnot [Collections.IDictionary] -or [string]$entry.tokenId -cnotmatch '^[0-9a-f]{16}$' -or -not $seen.Add([string]$entry.tokenId)){throw 'Tester registry contains invalid or duplicate IDs.'}
        if($entry.tokenId -ceq $TokenId){$selected=$entry}
    }
    if(-not $selected){throw 'Invitation ID was not found.'}
    if($selected.revoked -ne $true){$selected.revoked=$true;$selected.revokedAt=[DateTimeOffset]::UtcNow.ToString('o')}
    $json=$registry|ConvertTo-Json -Depth 32 -Compress
    $bytes=[Text.Encoding]::UTF8.GetBytes($json)
    if($bytes.Length -gt 2MB){throw 'Tester registry exceeds the 2 MiB limit.'}
    $temporary=Join-Path $root ('testers-'+[Guid]::NewGuid().ToString('N')+'.tmp')
    $stream=[IO.FileStream]::new($temporary,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try{$stream.Write($bytes,0,$bytes.Length);$stream.Flush($true)}finally{$stream.Dispose()}
    [IO.File]::Move($temporary,$registryPath,$true)
    Write-Output "TokenId=$TokenId"
}finally{
    if($temporary -and [IO.File]::Exists($temporary)){[IO.File]::Delete($temporary)}
    if($lock){$lock.Dispose()}
}
