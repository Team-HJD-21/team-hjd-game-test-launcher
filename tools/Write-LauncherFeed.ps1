#Requires -Version 7.0
param([Parameter(Mandatory)][string]$ReleaseDirectory,[Parameter(Mandatory)][string]$OutputFile,
      [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{40}$')][string]$SourceCommit,
      [string]$PrivateRoot=(Join-Path (Split-Path $PSScriptRoot) '.local/launcher-publisher'),
      [Parameter(Mandatory)][ValidatePattern('^[0-9]+\.[0-9]+\.[0-9]+$')][string]$Version,
      [Parameter(Mandatory)][ValidatePattern('^[0-9]+\.[0-9]+\.[0-9]+$')][string]$MinimumVersion)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'CatalogCommon.ps1')
$release=[IO.Path]::GetFullPath($ReleaseDirectory)
$private=[IO.Path]::GetFullPath($PrivateRoot)
$output=[IO.Path]::GetFullPath($OutputFile)
if([version]$MinimumVersion -gt [version]$Version){throw 'Minimum version cannot exceed this release.'}
if(Test-Path -LiteralPath $output){throw 'Select a new output file. Review existing signed feeds instead of replacing them during preparation.'}
$public=Read-HostingJson (Join-Path $private 'public.json') 16384
$key=Open-CatalogKey $private
$lock=Open-HostingLock (Join-Path $private 'feed-publisher.lock')
try{
    $parameters=$key.ExportParameters($false)
    if($public.keyId -cne 'launcher-p256-2026-11' -or $public.publicKey -cne ('p256:'+[Convert]::ToBase64String([byte[]]($parameters.Q.X+$parameters.Q.Y)))){throw 'Launcher signing key does not match its public record.'}
    $sequence=1L;$state=Join-Path $private 'feed-sequence.json'
    if(Test-Path -LiteralPath $state){$prior=Read-HostingJson $state 16384;if($prior.sequence -isnot [long] -and $prior.sequence -isnot [int] -or $prior.sequence -lt 1){throw 'Publisher sequence state is invalid.'};$sequence=[long]$prior.sequence+1}
    $platforms=@{}
    foreach($platform in @('windows-x64','macos-arm64','macos-x64')){
        $suffix=if($platform -ceq 'windows-x64'){'Windows-x64'}else{'macOS-'+$platform.Substring(6)}
        $names=@{payload=('TeamHJD-LauncherPayload-'+$suffix+'.zip');installer=('TeamHJD-GameTestLauncher-'+$suffix+$(if($platform -ceq 'windows-x64'){'-Setup.exe'}else{'.dmg'}))}
        $files=@{}
        foreach($kind in @('payload','installer')){
            $name=$names[$kind];$path=Join-Path $release $name;Assert-HostingPath $path
            $item=Get-Item -LiteralPath $path
            if(-not($item -is [IO.FileInfo]) -or $item.Length -le 0 -or $item.Length -ge 2GB){throw 'A required final artifact is missing or exceeds the supported size.'}
            $files[$kind]=@{name=$name;url=('https://github.com/Team-HJD-21/team-hjd-game-test-launcher/releases/download/v'+$Version+'/'+$name);bytes=$item.Length;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
        }
        $platforms[$platform]=$files
    }
    $now=[DateTime]::UtcNow
    $payload=@{schema=1;sequence=$sequence;version=$Version;minimumVersion=$MinimumVersion;sourceCommit=$SourceCommit;publishedAt=$now.ToString('yyyy-MM-ddTHH:mm:ssZ');expiresAt=$now.AddDays(365).ToString('yyyy-MM-ddTHH:mm:ssZ');platforms=$platforms}
    $data=[Text.Encoding]::UTF8.GetBytes(($payload|ConvertTo-Json -Depth 12 -Compress))
    $signature=$key.SignData($data,[Security.Cryptography.HashAlgorithmName]::SHA256,[Security.Cryptography.DSASignatureFormat]::IeeeP1363FixedFieldConcatenation)
    if(-not $key.VerifyData($data,$signature,[Security.Cryptography.HashAlgorithmName]::SHA256,[Security.Cryptography.DSASignatureFormat]::IeeeP1363FixedFieldConcatenation)){throw 'Feed signature self-verification failed.'}
    [IO.Directory]::CreateDirectory((Split-Path $output))|Out-Null
    Write-HostingJson $output @{keyId=$public.keyId;payload=[Convert]::ToBase64String($data);signature=[Convert]::ToBase64String($signature)}
    Write-HostingJson $state @{sequence=$sequence;version=$Version}
    Write-Output ('Signed launcher feed prepared, sequence '+$sequence+'. Publish only after all final release files have been checked and made public.')
}finally{$lock.Dispose();$key.Dispose()}
