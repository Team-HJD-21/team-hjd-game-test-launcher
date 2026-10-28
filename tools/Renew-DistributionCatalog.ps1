#requires -Version 7.5
[CmdletBinding()]
param(
    [string]$PrivateRoot=(Join-Path (Split-Path $PSScriptRoot) '.local/production-distribution'),
    [ValidateRange(1,6)][int]$MinimumRemainingDays=2,
    [switch]$Force
)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'CatalogCommon.ps1')
$root=[IO.Path]::GetFullPath($PrivateRoot);Assert-HostingPath $root
$lock=Open-HostingLock (Join-Path $root 'catalog-publish.lock');$key=$null
try{
    $key=Open-CatalogKey $root;$path=Join-Path $root 'catalog.json'
    $payload=Read-VerifiedCatalog $path $key
    $expiry=[DateTimeOffset]::Parse($payload.expiresAt,[Globalization.CultureInfo]::InvariantCulture)
    if($expiry.Offset -ne [TimeSpan]::Zero){throw 'Catalog expiry must be UTC.'}
    $now=[DateTimeOffset]::UtcNow
    if(-not $Force -and ($expiry-$now).TotalDays -ge $MinimumRemainingDays){return [pscustomobject]@{Renewed=$false;Sequence=$payload.sequence;ExpiresAt=$payload.expiresAt}}
    # Preserve game URLs, file hashes, versions and unknown catalog metadata.
    $payload.sequence=[long]$payload.sequence+1;$payload.issuedAt=$now.ToString('o');$payload.expiresAt=$now.AddDays(7).ToString('o')
    Save-SignedCatalog $path $payload $key
    [pscustomobject]@{Renewed=$true;Sequence=$payload.sequence;ExpiresAt=$payload.expiresAt}
}finally{if($key){$key.Dispose()};$lock.Dispose()}
