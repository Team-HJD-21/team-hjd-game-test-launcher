param([Parameter(Mandatory)][string]$QtRoot,[string]$OutputRoot)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$sdkVersion=(& (Join-Path $QtRoot 'bin/qmake.exe') -query QT_VERSION).Trim()
if($LASTEXITCODE -ne 0 -or $sdkVersion -cne '6.11.3'){throw 'The reviewed security release requires the exact Qt 6.11.3 SDK.'}
if(-not $OutputRoot){$OutputRoot=Join-Path $root 'dist'}
$output=[IO.Path]::GetFullPath($OutputRoot)
$version=([regex]::Match((Get-Content -LiteralPath (Join-Path $root 'CMakeLists.txt') -Raw),'project\(TeamHJDGameTestLauncher VERSION ([0-9.]+)')).Groups[1].Value
if(-not $version){throw 'Version not found.'}
$helperDirectory=Join-Path $root '.deps/cloudflared/2026.10.5'
$helper=Join-Path $helperDirectory 'cloudflared.exe'
$helperHash='db4e4c413612a0916f3e1679d3caaf376f94911952d163028fb726fb2e0516a9'
if(@('cloudflared.exe','LICENSE.txt','provenance.json'|Where-Object {-not(Test-Path -LiteralPath (Join-Path $helperDirectory $_) -PathType Leaf)}).Count){
    & (Join-Path $PSScriptRoot 'Get-Cloudflared.ps1') -OutputDirectory $helperDirectory
}
foreach($name in 'cloudflared.exe','LICENSE.txt','provenance.json'){
    if(-not(Test-Path -LiteralPath (Join-Path $helperDirectory $name) -PathType Leaf)){throw 'Pinned cloudflared dependency is missing. Run tools/Get-Cloudflared.ps1 before packaging.'}
}
if((Get-FileHash -LiteralPath $helper -Algorithm SHA256).Hash.ToLowerInvariant() -cne $helperHash){throw 'The cloudflared helper differs from the reviewed official release.'}
$helperProvenance=Get-Content -LiteralPath (Join-Path $helperDirectory 'provenance.json') -Raw|ConvertFrom-Json
if($helperProvenance.version -cne '2026.10.5' -or $helperProvenance.sha256 -cne $helperHash -or $helperProvenance.source -cne 'https://github.com/cloudflare/cloudflared/releases/tag/2026.10.5' -or $helperProvenance.asset -cne 'cloudflared-windows-amd64.exe'){throw 'cloudflared provenance does not match the pinned official dependency.'}
$bundle=Join-Path $output 'TeamHJD-GameTestLauncher-Windows-x64'
$payload=Join-Path $bundle "launcher/versions/$version"
if(Test-Path -LiteralPath $bundle){throw 'This bundle already exists and may be a user installation. Preserve it and select a fresh OutputRoot.'}
New-Item -ItemType Directory -Path $payload -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'build/TeamHJDClient.exe') -Destination $payload
Copy-Item -LiteralPath $helper -Destination (Join-Path $payload 'cloudflared.exe')
Copy-Item -LiteralPath (Join-Path $root 'build/TeamHJDLauncher.exe') -Destination $bundle
$env:PATH="$QtRoot/bin;$env:PATH"
& (Join-Path $QtRoot 'bin/windeployqt.exe') --release --no-translations --no-compiler-runtime --qmldir (Join-Path $root 'qml') --dir $payload (Join-Path $payload 'TeamHJDClient.exe')
if($LASTEXITCODE){throw 'Client deployment failed.'}
& (Join-Path $QtRoot 'bin/windeployqt.exe') --release --no-translations --no-compiler-runtime --dir $bundle (Join-Path $bundle 'TeamHJDLauncher.exe')
foreach($directory in @($payload,$bundle)){
    foreach($library in @('Qt6Core.dll','Qt6Network.dll')){
        $deployed=Join-Path $directory $library
        if(-not(Test-Path -LiteralPath $deployed)){continue}
        if((Get-Item -LiteralPath $deployed).VersionInfo.FileVersion -cne '6.11.3.0' -or
           (Get-FileHash -LiteralPath $deployed -Algorithm SHA256).Hash -cne (Get-FileHash -LiteralPath (Join-Path $QtRoot ('bin/'+$library)) -Algorithm SHA256).Hash){throw 'A deployed Qt library does not match the reviewed security SDK.'}
    }
}
if($LASTEXITCODE){throw 'Bootstrap deployment failed.'}
$sdkRoot=Join-Path ${env:ProgramFiles(x86)} 'Windows Kits/10'
foreach($name in @('d3dcompiler_47.dll','dxcompiler.dll','dxil.dll')){
    $source=Join-Path $sdkRoot ('Redist/D3D/x64/'+$name);$target=Join-Path $payload $name
    if(-not(Test-Path -LiteralPath $source -PathType Leaf)){throw 'Install the Windows SDK x64 D3D redistributable components before packaging.'}
    # windeployqt can select the host OS compiler or the SDK according to its
    # shell environment. Use the licensed SDK redistributable consistently in
    # this newly created staging folder, then verify the exact copied bytes.
    Copy-Item -LiteralPath $source -Destination $target -Force
    if((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -cne (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash){throw 'The staged shader DLL differs from the selected official SDK redistributable.'}
}
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
Copy-Item -LiteralPath (Join-Path $root 'vendor/miniz/LICENSE') -Destination (Join-Path $bundle 'licenses/miniz-LICENSE.txt')
& python (Join-Path $PSScriptRoot 'Add-WindowsD3DNotices.py') --output $licenses --sdk $sdkRoot
if($LASTEXITCODE){throw 'Windows shader redistribution notices could not be verified.'}
Copy-Item -LiteralPath (Join-Path $helperDirectory 'LICENSE.txt') -Destination (Join-Path $licenses 'cloudflared-LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $helperDirectory 'provenance.json') -Destination (Join-Path $licenses 'cloudflared-provenance.json')
Copy-Item -LiteralPath (Join-Path $root 'licenses/NSIS') -Destination $licenses -Recurse
Copy-Item -LiteralPath (Join-Path $root 'licenses/NSIS') -Destination (Join-Path $bundle 'licenses') -Recurse
Copy-Item -LiteralPath (Join-Path $root 'legal') -Destination $payload -Recurse
Copy-Item -LiteralPath (Join-Path $root 'legal') -Destination $bundle -Recurse
Copy-Item -LiteralPath (Join-Path $root 'THIRD_PARTY_NOTICES.md') -Destination $payload
Copy-Item -LiteralPath (Join-Path $root 'THIRD_PARTY_NOTICES.md') -Destination $bundle
# The payload also carries a complete, independently runnable bootstrap. It
# replaces the stable entry point only after the old client has exited.
$bootstrap=Join-Path $payload 'bootstrap'
New-Item -ItemType Directory -Path $bootstrap|Out-Null
Get-ChildItem -LiteralPath $bundle -File|ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination $bootstrap}
Copy-Item -LiteralPath (Join-Path $bundle 'licenses') -Destination $bootstrap -Recurse
Copy-Item -LiteralPath (Join-Path $bundle 'legal') -Destination $bootstrap -Recurse
$bootstrapFiles=@(Get-ChildItem -LiteralPath $bootstrap -Recurse -File|ForEach-Object {@{path=[IO.Path]::GetRelativePath($bootstrap,$_.FullName).Replace('\','/');bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}})
@{version=$version;files=$bootstrapFiles}|ConvertTo-Json -Depth 8 -Compress|Set-Content -LiteralPath (Join-Path $payload 'bootstrap-manifest.json') -Encoding utf8NoBOM
[IO.Compression.ZipFile]::CreateFromDirectory($payload,(Join-Path $output 'TeamHJD-LauncherPayload-Windows-x64.zip'),[IO.Compression.CompressionLevel]::Optimal,$false)
$files=@(Get-ChildItem -LiteralPath $payload -Recurse -File|ForEach-Object{@{path=[IO.Path]::GetRelativePath($payload,$_.FullName).Replace('\','/');bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}})
@{version=$version;executable='TeamHJDClient.exe';files=$files}|ConvertTo-Json -Depth 8 -Compress|Set-Content -LiteralPath (Join-Path $payload '._teamhjd-receipt.json') -Encoding utf8NoBOM
@{current=$version;previous=''}|ConvertTo-Json -Compress|Set-Content -LiteralPath (Join-Path $bundle 'launcher/active.json') -Encoding utf8NoBOM
Copy-Item -LiteralPath (Join-Path $root 'docs/PLAYER_GUIDE.ko.md') -Destination (Join-Path $bundle '시작하기.md')
[IO.Compression.ZipFile]::CreateFromDirectory($bundle,(Join-Path $output 'TeamHJD-GameTestLauncher-Windows-x64.zip'),[IO.Compression.CompressionLevel]::Optimal,$false)
Write-Output "Portable bundle and update payload written to $output. No game package or credentials included."
