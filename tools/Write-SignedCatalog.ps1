#requires -Version 7.5
param([Parameter(Mandatory)][string]$PrivateRoot,[Parameter(Mandatory)][hashtable]$Game)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'CatalogCommon.ps1')
$root=[IO.Path]::GetFullPath($PrivateRoot);Assert-HostingPath $root
$lock=Open-HostingLock (Join-Path $root 'catalog-publish.lock');$key=$null
try{
    $key=Open-CatalogKey $root
    $catalogPath=Join-Path $root 'catalog.json';$games=@();$sequence=0L
    $previous=$null
    if([IO.File]::Exists($catalogPath)){$payload=Read-VerifiedCatalog $catalogPath $key;$sequence=[long]$payload.sequence;$previous=@($payload.games|Where-Object id -CEQ $Game.id);if($previous.Count -gt 1){throw 'Duplicate existing game id.'};$previous=if($previous.Count){$previous[0]}else{$null};$games=@($payload.games|Where-Object id -CNE $Game.id)}
    $platform=if($Game.platform){[string]$Game.platform}else{'windows-x64'}
    if($platform -in 'macos-arm64','macos-x64','macos-universal'){
        if(-not $previous -or ($previous.platform -and $previous.platform -cne 'windows-x64') -or $previous.name -cne $Game.name){throw 'Mac variants require the same existing Windows game id/name; Windows publication is preserved.'}
        if($Game.ContainsKey('platforms')){throw 'Nested platform variants are not allowed.'}
        if(-not $previous.platforms){$previous.platforms=@{}}
        $previous.platforms[$platform]=$Game
        $games+=,$previous
    }elseif($platform -ceq 'windows-x64'){
        # Old Windows launchers continue to read the original top-level game.
        # Updating it must not silently delete independently published Mac builds.
        if($previous -and $previous.platforms -and -not $Game.ContainsKey('platforms')){$Game.platforms=$previous.platforms}
        $games+=,$Game
    }else{throw 'Unsupported game platform.'}
    $now=[DateTimeOffset]::UtcNow
    $catalog=@{schema=1;sequence=$sequence+1;issuedAt=$now.ToString('o');expiresAt=$now.AddDays(7).ToString('o');games=$games}
    Save-SignedCatalog $catalogPath $catalog $key
    [pscustomobject]@{Sequence=$sequence+1;ExpiresAt=$catalog.expiresAt}
}finally{if($key){$key.Dispose()};$lock.Dispose()}
