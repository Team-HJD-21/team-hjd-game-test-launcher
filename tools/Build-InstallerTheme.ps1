#Requires -Version 7.0
param([Parameter(Mandatory)][string]$OutputDirectory,
      [string]$NsisRoot=(Join-Path (Split-Path $PSScriptRoot) '.deps/nsis/nsis-3.13'))
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output){throw 'Select a fresh installer-theme output directory.'}
New-Item -ItemType Directory -Path $output|Out-Null
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $vs){throw 'The official Visual Studio C++ toolchain is required.'}
& (Join-Path $vs 'Common7/Tools/Launch-VsDevShell.ps1') -Arch x86 -HostArch amd64 -SkipAutomaticLocation|Out-Null
$source=Join-Path $project 'installer/TeamHJDTheme.cpp'
$definition=Join-Path $project 'installer/TeamHJDTheme.def'
$include=Join-Path ([IO.Path]::GetFullPath($NsisRoot)) 'Examples/Plugin/nsis'
$library=Join-Path $output 'TeamHJDTheme.dll'
$compile=@('/nologo','/LD','/MT','/O2','/utf-8','/EHsc','/GS','/guard:cf','/DUNICODE','/D_UNICODE',('/I'+$include),('/Fo'+(Join-Path $output 'TeamHJDTheme.obj')),('/Fe'+$library),$source,'/link',('/DEF:'+$definition),'/DYNAMICBASE','/NXCOMPAT','/GUARD:CF','/Brepro','user32.lib','gdi32.lib','comctl32.lib','uxtheme.lib')
& cl.exe @compile
if($LASTEXITCODE){throw 'Native installer theme compilation failed.'}
# Only the original logo is rasterized. Every title, description and version
# stays a native control, so text remains sharp at the user's display scaling.
Add-Type -AssemblyName System.Drawing
$logo=[Drawing.Image]::FromFile((Join-Path $project 'assets/brand/TeamHJD-Simple-Logo.png'))
$bitmap=[Drawing.Bitmap]::new(600,228,[Drawing.Imaging.PixelFormat]::Format24bppRgb)
$graphics=[Drawing.Graphics]::FromImage($bitmap)
try{
    $graphics.Clear([Drawing.ColorTranslator]::FromHtml('#14181b'))
    $graphics.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $width=[single]520;$height=[single]($width*$logo.Height/$logo.Width)
    $graphics.DrawImage($logo,[Drawing.RectangleF]::new(40,((228-$height)/2),$width,$height))
    $bitmap.Save((Join-Path $output 'header.bmp'),[Drawing.Imaging.ImageFormat]::Bmp)
}finally{$graphics.Dispose();$bitmap.Dispose();$logo.Dispose()}
Write-Output 'Built native TeamHJD installer theme and high-density original logo.'
