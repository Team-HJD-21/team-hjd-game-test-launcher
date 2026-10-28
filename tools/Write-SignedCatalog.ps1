#requires -Version 7.5
param([Parameter(Mandatory)][string]$PrivateRoot,[Parameter(Mandatory)][hashtable]$Game)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'CatalogCommon.ps1')
$root=[IO.Path]::GetFullPath($PrivateRoot);Assert-HostingPath $root
$lock=Open-HostingLock (Join-Path $root 'catalog-publish.lock');$key=$null
try{
    $key=Open-CatalogKey $root
    $catalogPath=Join-Path $root 'catalog.json';$games=@();$sequence=0L
    if([IO.File]::Exists($catalogPath)){$payload=Read-VerifiedCatalog $catalogPath $key;$sequence=[long]$payload.sequence;$games=@($payload.games|Where-Object id -CNE $Game.id)}
    $games+=,$Game;$now=[DateTimeOffset]::UtcNow
    $catalog=@{schema=1;sequence=$sequence+1;issuedAt=$now.ToString('o');expiresAt=$now.AddDays(7).ToString('o');games=$games}
    Save-SignedCatalog $catalogPath $catalog $key
    [pscustomobject]@{Sequence=$sequence+1;ExpiresAt=$catalog.expiresAt}
}finally{if($key){$key.Dispose()};$lock.Dispose()}
