# Signed catalog operations share one private lock with manual publication.
. (Join-Path $PSScriptRoot 'HostingCommon.ps1')
function Open-CatalogKey([string]$Root){
    $path=Join-Path $Root 'publisher.dpapi';Assert-HostingPath $path
    $protected=[IO.File]::ReadAllBytes($path)
    if($protected.Length -gt 16384){throw 'Publisher key exceeds its size limit.'}
    $private=[Security.Cryptography.ProtectedData]::Unprotect($protected,$null,[Security.Cryptography.DataProtectionScope]::CurrentUser)
    $key=[Security.Cryptography.ECDsa]::Create()
    try{$consumed=0;$key.ImportPkcs8PrivateKey($private,[ref]$consumed);if($consumed -ne $private.Length -or $key.KeySize -ne 256){throw 'Invalid publisher key.'};return $key}catch{$key.Dispose();throw}finally{[Array]::Clear($private,0,$private.Length)}
}
function Read-VerifiedCatalog([string]$Path,[Security.Cryptography.ECDsa]$Key){
    $envelope=Read-HostingJson $Path (32*1024*1024)
    $data=[Convert]::FromBase64String($envelope.payload);$signature=[Convert]::FromBase64String($envelope.signature)
    if(-not $Key.VerifyData($data,$signature,[Security.Cryptography.HashAlgorithmName]::SHA256,[Security.Cryptography.DSASignatureFormat]::IeeeP1363FixedFieldConcatenation)){throw 'Existing catalog signature is invalid; preserved without changes.'}
    $payload=[Text.UTF8Encoding]::new($false,$true).GetString($data)|ConvertFrom-Json -AsHashtable -DateKind String -Depth 64
    if($payload.schema -ne 1 -or $payload.sequence -isnot [long] -and $payload.sequence -isnot [int] -or [long]$payload.sequence -lt 1 -or [long]$payload.sequence -ge 9007199254740991 -or $payload.games -isnot [array]){throw 'Invalid existing catalog payload.'}
    return $payload
}
function Save-SignedCatalog([string]$Path,[hashtable]$Payload,[Security.Cryptography.ECDsa]$Key){
    $data=[Text.Encoding]::UTF8.GetBytes(($Payload|ConvertTo-Json -Depth 64 -Compress))
    if($data.Length -gt 20*1024*1024){throw 'Catalog payload exceeds its size limit.'}
    $signature=$Key.SignData($data,[Security.Cryptography.HashAlgorithmName]::SHA256,[Security.Cryptography.DSASignatureFormat]::IeeeP1363FixedFieldConcatenation)
    Write-HostingJson $Path @{payload=[Convert]::ToBase64String($data);signature=[Convert]::ToBase64String($signature)}
}
