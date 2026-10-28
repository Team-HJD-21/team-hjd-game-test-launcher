param(
    [Parameter(Mandatory)][string]$LatestPointer,
    [string]$Id='hose-for-hire',
    [string]$Name='Hose for Hire',
    [string]$Executable='HoseForHire.exe',
    [string]$Description='함께 출동하고, 함께 해결하세요. TeamHJD의 협동 플레이테스트.',
    [string]$NotesFile,
    [string]$PrivateRoot=(Join-Path (Split-Path $PSScriptRoot) '.local/private-distribution'),
    [switch]$VerifyOnly
)
$ErrorActionPreference='Stop'
$pointerPath=[IO.Path]::GetFullPath($LatestPointer)
$base=Split-Path $pointerPath
$pointer=Get-Content -LiteralPath $pointerPath -Raw|ConvertFrom-Json
if($pointer.schema -ne 1){throw 'Unsupported latest pointer schema.'}
function Resolve-Artifact([string]$Relative){
    if([IO.Path]::IsPathRooted($Relative) -or $Relative.Contains('\') -or $Relative.Split('/') -contains '..'){throw 'Artifact must be a Builds-relative path.'}
    $result=[IO.Path]::GetFullPath((Join-Path $base $Relative))
    if(-not $result.StartsWith($base+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Artifact escaped the Builds directory.'}
    $current=$result
    while($current.Length -ge $base.Length){if((Get-Item -LiteralPath $current).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Linked artifact path rejected.'};$current=Split-Path $current}
    return $result
}
$archivePath=Resolve-Artifact $pointer.archive
$manifestPath=Resolve-Artifact $pointer.packageManifest
$manifest=Get-Content -LiteralPath $manifestPath -Raw|ConvertFrom-Json
foreach($field in 'version','buildNumber','configuration','wire','buildGuid','bytes','sha256'){if([string]$pointer.$field -ine [string]$manifest.$field){throw "Pointer/package manifest mismatch: $field"}}
if((Get-Item -LiteralPath $archivePath).Length -ne $pointer.bytes -or (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash -ine $pointer.sha256){throw 'Archive size/hash does not match latest pointer.'}
$expected=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
foreach($file in @($manifest.artifacts)+@($manifest.supportFiles)){if($expected.ContainsKey($file.path)){throw 'Duplicate manifest path.'};$expected.Add($file.path,$file)}
$runtimeReference=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
foreach($file in $manifest.artifacts){$runtimeReference.Add($file.path,$file)}
$archive=[IO.Compression.ZipFile]::OpenRead($archivePath)
$artifacts=@();$expanded=0L
try {
    if($archive.Entries.Count -ne $manifest.entries -or $expected.Count+1 -ne $manifest.entries){throw 'Manifest ZIP entry count does not match.'}
    foreach($entry in $archive.Entries){
        $path=$entry.FullName
        if($path -ceq 'build-manifest.json'){
            $stream=$entry.Open();$reader=[IO.StreamReader]::new($stream)
            try{$embedded=$reader.ReadToEnd()|ConvertFrom-Json}finally{$reader.Dispose();$stream.Dispose()}
            foreach($field in 'version','buildNumber','configuration','wire','buildGuid'){if([string]$embedded.$field -ine [string]$manifest.$field){throw "Embedded game manifest mismatch: $field"}}
            if($embedded.artifacts.Count -ne $manifest.artifacts.Count){throw 'Embedded runtime file count mismatch.'}
            foreach($runtime in $embedded.artifacts){$outer=$runtimeReference[$runtime.path];if(-not $outer -or $outer.bytes -ne $runtime.bytes -or $outer.sha256 -ine $runtime.sha256){throw 'Embedded runtime manifest differs from package manifest.'}}
            $stream=$entry.Open();try{$hash=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($stream))}finally{$stream.Dispose()}
            $artifacts+=@{path=$path;bytes=$entry.Length;sha256=$hash.ToLowerInvariant()};$expanded+=$entry.Length;continue
        }
        if(-not $expected.ContainsKey($path)){throw "Unlisted archive path: $path"}
        $f=$expected[$path]
        $stream=$entry.Open();try{$hash=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($stream))}finally{$stream.Dispose()}
        if($entry.Length -ne $f.bytes -or $hash -ine $f.sha256){throw "Archive entry mismatch: $path"}
        $artifacts+=@{path=$path;bytes=$entry.Length;sha256=$hash.ToLowerInvariant()};$expanded+=$entry.Length;$expected.Remove($path)|Out-Null
    }
}finally{$archive.Dispose()}
if($expected.Count){throw 'Archive has missing files.'}
if($VerifyOnly){Write-Output "Verified canonical pointer, archive SHA256 and all $($artifacts.Count) entries. No input files changed.";return}
# Reuse the exact canonical ZIP. Do not extract/repackage, choose by mtime or build Unity.
$root=[IO.Path]::GetFullPath($PrivateRoot)
if(-not(Test-Path -LiteralPath (Join-Path $root 'publisher.dpapi'))){throw 'Initialize private distribution with New-Publisher.ps1 first.'}
if($Id -cnotmatch '^[a-z0-9][a-z0-9-]{0,63}$'){throw 'Invalid game id.'}
$sha=$pointer.sha256.ToLowerInvariant();$target=Join-Path $root "packages/$sha.zip"
if(-not(Test-Path -LiteralPath $target)){Copy-Item -LiteralPath $archivePath -Destination $target}
if((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ine $sha){throw 'Private copy does not match archive.'}
$notes=if($NotesFile){[IO.File]::ReadAllText([IO.Path]::GetFullPath($NotesFile))}else{"$($pointer.version) · BUILD $($pointer.buildNumber) · $($pointer.configuration)`n`n전체 런타임과 동봉 테스트 안내의 파일 검증을 마친 빌드입니다."}
$game=@{id=$Id;name=$Name;version=$pointer.version;description=$Description;executable=$Executable;notes=$notes;url="packages/$sha.zip";sha256=$sha;bytes=[long]$pointer.bytes;expandedBytes=$expanded;files=$artifacts}
& (Join-Path $PSScriptRoot 'Write-SignedCatalog.ps1') -PrivateRoot $root -Game $game
Write-Output "Published canonical $($pointer.packageLabel) privately; original ZIP preserved byte for byte."
