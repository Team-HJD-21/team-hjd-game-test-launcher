#Requires -Version 7.0
param([ValidateSet('Before','After')][string]$Stage='After',
      [Parameter(Mandatory)][string]$EvidenceFile,
      [string]$DataRoot=(Join-Path $env:LOCALAPPDATA 'TeamHJD/Launcher'))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'HostingCommon.ps1')
$evidence=[IO.Path]::GetFullPath($EvidenceFile);Assert-HostingPath $evidence
if($Stage -eq 'Before' -and (Test-Path -LiteralPath $evidence)){throw 'Preserve the prior baseline and select a new evidence file.'}
$prior=if($Stage -eq 'After'){Read-HostingJson $evidence 1048576}else{$null}
if($prior){$DataRoot=$prior.dataRoot}
$data=[IO.Path]::GetFullPath($DataRoot);Assert-HostingPath $data
$settings=Read-HostingJson (Join-Path $data 'settings.json') 1048576
$games=[IO.Path]::GetFullPath([string]$settings.gamesRoot);Assert-HostingPath $games
$invariants=[ordered]@{server=$settings.server;publisherKey=$settings.publisherKey;gamesRoot=$settings.gamesRoot;privacyVersion=$settings.privacyVersion;privacyAcceptedAt=$settings.privacyAcceptedAt}
$fingerprint=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes(($invariants|ConvertTo-Json -Compress)))).ToLowerInvariant()
$snapshots=@()
foreach($directory in Get-ChildItem -LiteralPath $games -Directory){
    $activePath=Join-Path $directory.FullName 'active.json'
    if(-not(Test-Path -LiteralPath $activePath)){continue}
    $active=Read-HostingJson $activePath 16384
    $slot=[IO.Path]::GetFullPath((Join-Path $directory.FullName ('versions/'+$active.current)))
    if(-not $slot.StartsWith($directory.FullName+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Game slot escapes its root.'}
    Assert-HostingPath $slot
    $receiptPath=Join-Path $slot '._teamhjd-receipt.json';$receipt=Read-HostingJson $receiptPath 4194304
    $count=0
    foreach($entry in $receipt.files){
        $file=[IO.Path]::GetFullPath((Join-Path $slot $entry.path))
        if(-not $file.StartsWith($slot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Game receipt escapes its slot.'}
        Assert-HostingPath $file
        if((Get-Item -LiteralPath $file).Length -ne $entry.bytes -or (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() -cne $entry.sha256){throw 'An actual game file differs from its receipt.'}
        $count++
    }
    $snapshots+=@{id=$directory.Name;slot=$slot;active=$active.current;receiptSha256=(Get-FileHash -LiteralPath $receiptPath -Algorithm SHA256).Hash.ToLowerInvariant();files=$count}
}
$credentialsPresent=-not [string]::IsNullOrEmpty([string]$settings.cloudflareBlob)
if($Stage -eq 'Before'){
    [IO.Directory]::CreateDirectory((Split-Path $evidence))|Out-Null
    Write-HostingJson $evidence @{dataRoot=$data;gamesRoot=$games;settingsInvariantSha256=$fingerprint;protectedCredentialsPresent=$credentialsPresent;games=$snapshots;stage='Before';result='PASS'}
}else{
    if($prior.settingsInvariantSha256 -cne $fingerprint -or $prior.gamesRoot -ine $games -or ($prior.protectedCredentialsPresent -and -not $credentialsPresent) -or @($prior.games).Count -ne $snapshots.Count){throw 'User settings, credential presence or game inventory changed.'}
    foreach($game in $prior.games){
        $match=@($snapshots|Where-Object id -CEQ $game.id)
        if($match.Count -ne 1 -or $match[0].slot -ine $game.slot -or $match[0].active -cne $game.active -or $match[0].receiptSha256 -cne $game.receiptSha256 -or $match[0].files -ne $game.files){throw 'An installed game or receipt changed.'}
    }
    $result=[IO.Path]::Combine((Split-Path $evidence),('after-'+[DateTime]::Now.ToString('yyyyMMdd-HHmmss-fff')+'.json'))
    Write-HostingJson $result @{result='PASS';stage='After';settingsInvariantsPreserved=$true;protectedCredentialPresencePreserved=$true;gameReceiptsAndAllFilesVerified=$true;games=$snapshots.Count;files=($snapshots|Measure-Object -Property files -Sum).Sum;humanEmailAuthentication=$false}
}
Write-Output ('PASS user data '+$Stage+'; games='+$snapshots.Count+' files='+($snapshots|Measure-Object -Property files -Sum).Sum+'; no credentials or file contents printed')
