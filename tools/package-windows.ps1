param([Parameter(Mandatory)][string]$QtRoot)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$version=([regex]::Match((Get-Content -LiteralPath (Join-Path $root 'CMakeLists.txt') -Raw),'project\(TeamHJDGameTestLauncher VERSION ([0-9.]+)')).Groups[1].Value
if(-not $version){throw 'Version not found.'}
$bundle=Join-Path $root 'dist/TeamHJD-GameTestLauncher-Windows-x64'
$payload=Join-Path $bundle "launcher/versions/$version"
if(Test-Path -LiteralPath (Join-Path $payload 'TeamHJDClient.exe')){throw 'This output already exists. Preserve it and select a fresh dist directory before packaging again.'}
New-Item -ItemType Directory -Path $payload -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'build/TeamHJDClient.exe') -Destination $payload
Copy-Item -LiteralPath (Join-Path $root 'build/TeamHJDLauncher.exe') -Destination $bundle
$env:PATH="$QtRoot/bin;$env:PATH"
& (Join-Path $QtRoot 'bin/windeployqt.exe') --release --no-translations --no-compiler-runtime --qmldir (Join-Path $root 'qml') --dir $payload (Join-Path $payload 'TeamHJDClient.exe')
if($LASTEXITCODE){throw 'Client deployment failed.'}
& (Join-Path $QtRoot 'bin/windeployqt.exe') --release --no-translations --no-compiler-runtime --dir $bundle (Join-Path $bundle 'TeamHJDLauncher.exe')
if($LASTEXITCODE){throw 'Bootstrap deployment failed.'}
# App-local Microsoft runtime keeps the portable launcher usable without an installer or elevation.
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$runtime=Get-ChildItem -LiteralPath (Join-Path $vs 'VC/Redist/MSVC') -Directory|Where-Object {$_.Name -match '^\d+\.\d+\.\d+$'}|Sort-Object {[version]$_.Name} -Descending|ForEach-Object {Join-Path $_.FullName 'x64/Microsoft.VC143.CRT'}|Where-Object {Test-Path -LiteralPath $_}|Select-Object -First 1
if(-not $runtime){throw 'Redistributable x64 Visual C++ runtime not found.'}
Copy-Item -Path (Join-Path $runtime '*.dll') -Destination $payload
Copy-Item -Path (Join-Path $runtime '*.dll') -Destination $bundle
"[Paths]`nPrefix=.`nPlugins=.`nQmlImports=qml`n"|Set-Content -LiteralPath (Join-Path $payload 'qt.conf') -Encoding ascii
# Include Qt's exact redistribution licenses and the library source offer.
$licenses=Join-Path $payload 'licenses';New-Item -ItemType Directory -Path $licenses -Force|Out-Null
Copy-Item -LiteralPath (Join-Path $root 'licenses/Qt') -Destination $licenses -Recurse
Copy-Item -LiteralPath (Join-Path $root 'licenses/Qt') -Destination (Join-Path $bundle 'licenses') -Recurse
Copy-Item -LiteralPath (Join-Path $root 'vendor/miniz/LICENSE') -Destination (Join-Path $licenses 'miniz-LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $root 'THIRD_PARTY_NOTICES.md') -Destination $payload
Copy-Item -LiteralPath (Join-Path $root 'THIRD_PARTY_NOTICES.md') -Destination $bundle
[IO.Compression.ZipFile]::CreateFromDirectory($payload,(Join-Path $root 'dist/TeamHJD-LauncherPayload-Windows-x64.zip'),[IO.Compression.CompressionLevel]::Optimal,$false)
$files=@(Get-ChildItem -LiteralPath $payload -Recurse -File|ForEach-Object{@{path=[IO.Path]::GetRelativePath($payload,$_.FullName).Replace('\','/');bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}})
@{version=$version;executable='TeamHJDClient.exe';files=$files}|ConvertTo-Json -Depth 8 -Compress|Set-Content -LiteralPath (Join-Path $payload '._teamhjd-receipt.json') -Encoding utf8NoBOM
@{current=$version;previous=''}|ConvertTo-Json -Compress|Set-Content -LiteralPath (Join-Path $bundle 'launcher/active.json') -Encoding utf8NoBOM
Copy-Item -LiteralPath (Join-Path $root 'docs/PLAYER_GUIDE.ko.md') -Destination (Join-Path $bundle '시작하기.md')
[IO.Compression.ZipFile]::CreateFromDirectory($bundle,(Join-Path $root 'dist/TeamHJD-GameTestLauncher-Windows-x64.zip'),[IO.Compression.CompressionLevel]::Optimal,$false)
Write-Output "Portable bundle and update payload written to dist. No game package or credentials included."
