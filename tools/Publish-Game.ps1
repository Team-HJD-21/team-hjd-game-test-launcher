param(
    [Parameter(Mandatory)][string]$BuildDirectory,
    [Parameter(Mandatory)][string]$Id,
    [Parameter(Mandatory)][string]$Name,
    [Parameter(Mandatory)][string]$Version,
    [Parameter(Mandatory)][string]$Executable,
    [string]$Description='',
    [string]$NotesFile,
    [string]$PrivateRoot=(Join-Path (Split-Path $PSScriptRoot) '.local/private-distribution')
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath($PrivateRoot)
$source=[IO.Path]::GetFullPath($BuildDirectory)
if(-not (Test-Path -LiteralPath (Join-Path $root 'publisher.dpapi'))){throw 'Run New-Publisher.ps1 first.'}
if($Id -cnotmatch '^[a-z0-9][a-z0-9-]{0,63}$' -or $Version -cnotmatch '^[0-9][A-Za-z0-9._-]{0,79}$'){throw 'Invalid game id or version.'}
function Test-SafeRelative([string]$Value){
    if(-not $Value -or $Value.Length -gt 220 -or $Value.Contains('\') -or $Value.Contains(':') -or $Value.StartsWith('/')){return $false}
    foreach($segment in $Value.Split('/')){if(-not $segment -or $segment -in '.','..' -or $segment.EndsWith('.') -or $segment.EndsWith(' ') -or $segment -match '^(?i:CON|PRN|AUX|NUL|COM[0-9¹²³]|LPT[0-9¹²³])(?:\.|$)' -or $segment -match '[<>"|?*\x00-\x1f]' -or $segment.StartsWith('._teamhjd',[StringComparison]::OrdinalIgnoreCase)){return $false}}
    return $true
}
if(-not (Test-SafeRelative $Executable) -or -not $Executable.EndsWith('.exe',[StringComparison]::OrdinalIgnoreCase)){throw 'Executable must be a safe relative .exe path.'}
$runtimeFiles=@(Get-ChildItem -LiteralPath $source -File -Recurse -Force)
if($runtimeFiles.Count -eq 0 -or $runtimeFiles.Count -gt 100000){throw 'Empty or oversized build.'}
foreach($directory in @(Get-Item -LiteralPath $source)+@(Get-ChildItem -LiteralPath $source -Directory -Recurse -Force)) {if($directory.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Linked build directories are not allowed.'}}
$seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$artifacts=@();$expanded=0L
foreach($file in $runtimeFiles){
    $relative=[IO.Path]::GetRelativePath($source,$file.FullName).Replace('\','/')
    if(-not(Test-SafeRelative $relative) -or -not $seen.Add($relative) -or ($file.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw "Unsafe build entry: $relative"}
    if($relative -match '(?i)(^|/)(PlaytestLogs|\.git|\.aws|\.codex|\.agents)(/|$)' -or $relative -match '(?i)\.(pem|pfx|key|log|dmp)$' -or $relative -match '(?i)(^|/)(auth\.json|publisher\.dpapi|settings\.json)$'){throw "Private operational file cannot be packaged: $relative"}
    $hash=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    $artifacts+=@{path=$relative;bytes=$file.Length;sha256=$hash};$expanded+=$file.Length
}
if(-not $seen.Contains($Executable)){throw 'Executable is missing.'}
$buildManifest=Join-Path $source 'build-manifest.json'
if(Test-Path -LiteralPath $buildManifest){
    $manifest=Get-Content -LiteralPath $buildManifest -Raw|ConvertFrom-Json
    if($manifest.version -and $manifest.version -ne $Version){throw 'Requested version differs from the game build manifest.'}
    foreach($expected in $manifest.artifacts){$actual=$artifacts|Where-Object path -CEQ $expected.path;if(-not $actual -or $actual.bytes -ne $expected.bytes -or $actual.sha256 -cne $expected.sha256.ToLowerInvariant()){throw "Game build manifest mismatch: $($expected.path)"}}
}
$zipPath=Join-Path $root ('package-'+[Guid]::NewGuid().ToString('N')+'.tmp')
try {
    $stream=[IO.File]::Create($zipPath)
    $zip=[IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Create)
    try{foreach($file in $runtimeFiles){$relative=[IO.Path]::GetRelativePath($source,$file.FullName).Replace('\','/');[IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$file.FullName,$relative,[IO.Compression.CompressionLevel]::Optimal)|Out-Null}}finally{$zip.Dispose();$stream.Dispose()}
    # Detect a build changing while it is being packaged; never publish a torn build.
    foreach($a in $artifacts){$path=Join-Path $source $a.path;if((Get-Item -LiteralPath $path).Length -ne $a.bytes -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $a.sha256){throw 'Build changed during packaging. Stop the build process and retry.'}}
    $sha=(Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
    $packagePath=Join-Path $root "packages/$sha.zip"
    if(-not(Test-Path -LiteralPath $packagePath)){Move-Item -LiteralPath $zipPath -Destination $packagePath}
    $notes=if($NotesFile){[IO.File]::ReadAllText([IO.Path]::GetFullPath($NotesFile))}else{'새 테스트 빌드가 배포되었습니다.'}
    $game=@{id=$Id;name=$Name;version=$Version;description=$Description;executable=$Executable;notes=$notes;url="packages/$sha.zip";sha256=$sha;bytes=(Get-Item -LiteralPath $packagePath).Length;expandedBytes=$expanded;files=$artifacts}
    $publication=& (Join-Path $PSScriptRoot 'Write-SignedCatalog.ps1') -PrivateRoot $root -Game $game
    Write-Output "Published $Id $Version; catalog sequence $($publication.Sequence); files $($artifacts.Count); private package verified."
}finally{if(Test-Path -LiteralPath $zipPath){Remove-Item -LiteralPath $zipPath}}
