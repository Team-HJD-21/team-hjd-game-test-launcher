param([Parameter(Mandatory)][string]$PrivateRoot,[Parameter(Mandatory)][hashtable]$Game)
$ErrorActionPreference='Stop'
$catalogPath=Join-Path $PrivateRoot 'catalog.json';$games=@();$sequence=0L
if(Test-Path -LiteralPath $catalogPath){$old=Get-Content -LiteralPath $catalogPath -Raw|ConvertFrom-Json;$payload=[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($old.payload))|ConvertFrom-Json;$sequence=[long]$payload.sequence;$games=@($payload.games|Where-Object id -CNE $Game.id)}
$games+=,$Game
$now=[DateTimeOffset]::UtcNow
$catalog=@{schema=1;sequence=$sequence+1;issuedAt=$now.ToString('o');expiresAt=$now.AddDays(7).ToString('o');games=$games}
$data=[Text.Encoding]::UTF8.GetBytes(($catalog|ConvertTo-Json -Depth 30 -Compress))
$private=[Security.Cryptography.ProtectedData]::Unprotect([IO.File]::ReadAllBytes((Join-Path $PrivateRoot 'publisher.dpapi')),$null,[Security.Cryptography.DataProtectionScope]::CurrentUser)
$key=[Security.Cryptography.ECDsa]::Create()
try{$consumed=0;$key.ImportPkcs8PrivateKey($private,[ref]$consumed);$signature=$key.SignData($data,[Security.Cryptography.HashAlgorithmName]::SHA256,[Security.Cryptography.DSASignatureFormat]::IeeeP1363FixedFieldConcatenation)}finally{$key.Dispose();[Array]::Clear($private,0,$private.Length)}
$signed=@{payload=[Convert]::ToBase64String($data);signature=[Convert]::ToBase64String($signature)}|ConvertTo-Json -Compress
$temporary=Join-Path $PrivateRoot ('catalog-'+[Guid]::NewGuid().ToString('N')+'.tmp')
[IO.File]::WriteAllText($temporary,$signed,[Text.UTF8Encoding]::new($false));[IO.File]::Move($temporary,$catalogPath,$true)
