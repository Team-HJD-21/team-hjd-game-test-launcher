param([string]$QtRoot=$env:QT_ROOT_DIR,[switch]$Deploy)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
if(-not $QtRoot){$QtRoot=Join-Path $root '.deps/Qt/6.8.3/msvc2022_64'}
if(-not(Test-Path -LiteralPath (Join-Path $QtRoot 'bin/qmake.exe'))){throw 'Set QT_ROOT_DIR or pass -QtRoot to a Qt 6.8+ MSVC x64 installation.'}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $vs){throw 'Install Visual Studio 2022 C++ desktop tools and a Windows SDK.'}
& (Join-Path $vs 'Common7/Tools/Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
$cmake=Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$ninja=Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
$configuration=@()
$compiler=Split-Path (Get-Command cl.exe).Source
if((Test-Path (Join-Path $compiler '1042/clui.dll')) -and -not(Test-Path (Join-Path $compiler '1033/clui.dll'))){$configuration+='-DHJD_MSVC_INCLUDE_PREFIX:STRING=참고: 포함 파일: '}
& $cmake -S $root -B (Join-Path $root 'build') -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_PREFIX_PATH=$QtRoot" -DCMAKE_BUILD_TYPE=Release @configuration
if($LASTEXITCODE){throw 'CMake configure failed.'}
& $cmake --build (Join-Path $root 'build') --parallel 4
if($LASTEXITCODE){throw 'Build failed.'}
$env:PATH="$QtRoot/bin;$env:PATH"
& (Join-Path (Split-Path $cmake) 'ctest.exe') --test-dir (Join-Path $root 'build') --output-on-failure
if($LASTEXITCODE){throw 'Core checks failed.'}
if($Deploy){& (Join-Path $PSScriptRoot 'package-windows.ps1') -QtRoot $QtRoot}
