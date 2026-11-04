#Requires -Version 7.0
param([Parameter(Mandatory)][string]$BundleRoot,[Parameter(Mandatory)][string]$OutputDirectory,
      [string]$NsisRoot=(Join-Path (Split-Path $PSScriptRoot) '.deps/nsis/nsis-3.13'),[switch]$TestPackage)
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot
$bundle=[IO.Path]::GetFullPath($BundleRoot)
$output=[IO.Path]::GetFullPath($OutputDirectory)
$version=([regex]::Match((Get-Content -LiteralPath (Join-Path $project 'CMakeLists.txt') -Raw),'project\(TeamHJDGameTestLauncher VERSION ([0-9.]+)')).Groups[1].Value
$slot=Join-Path $bundle ('launcher/versions/'+$version)
if(-not(Test-Path -LiteralPath (Join-Path $slot 'bootstrap-manifest.json'))){throw 'Package a fresh complete bundle with its embedded bootstrap first.'}
$compiler=Join-Path ([IO.Path]::GetFullPath($NsisRoot)) 'makensis.exe'
if((Split-Path $NsisRoot -Leaf) -cne 'nsis-3.13'){throw 'Select the exact pinned nsis-3.13 tool directory.'}
& (Join-Path $PSScriptRoot 'Get-Nsis.ps1') -CacheRoot (Split-Path ([IO.Path]::GetFullPath($NsisRoot)))
if(-not(Test-Path -LiteralPath $compiler)){throw 'Pinned NSIS 3.13 is required in the workspace; no global installer is needed.'}
function Invoke-Nsis([string[]]$Arguments){
    $start=[Diagnostics.ProcessStartInfo]::new($compiler)
    $start.UseShellExecute=$false;$start.CreateNoWindow=$true
    $start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
    $start.StandardOutputEncoding=[Text.Encoding]::UTF8;$start.StandardErrorEncoding=[Text.Encoding]::UTF8
    foreach($argument in $Arguments){$start.ArgumentList.Add($argument)}
    $process=[Diagnostics.Process]::Start($start)
    $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    $result=$stdout.GetAwaiter().GetResult()+$stderr.GetAwaiter().GetResult()
    if($process.ExitCode -ne 0){throw ('NSIS compilation failed: '+$result)}
    return $result.Trim()
}
$reported=Invoke-Nsis @('/VERSION')
if($reported -cne 'v3.13'){throw ('Unexpected NSIS compiler version: '+$reported)}
[IO.Directory]::CreateDirectory($output)|Out-Null
$name=if($TestPackage){'TeamHJD-GameTestLauncher-Windows-x64-Setup-test.exe'}else{'TeamHJD-GameTestLauncher-Windows-x64-Setup.exe'}
$destination=Join-Path $output $name
if(Test-Path -LiteralPath $destination){throw 'Select a fresh installer output. Existing artifacts are never overwritten.'}
$terms=Join-Path $output 'installer-terms-utf8.txt'
[IO.File]::WriteAllText($terms,[IO.File]::ReadAllText((Join-Path $project 'legal/TERMS.ko.txt'),[Text.Encoding]::UTF8),[Text.UTF8Encoding]::new($true))
$parameters=@('/V2','/INPUTCHARSET','UTF8',('/DVERSION='+$version),('/DOUTPUT_FILE='+$destination),('/DBUNDLE_ROOT='+$bundle),('/DBOOTSTRAP_ROOT='+(Join-Path $slot 'bootstrap')),('/DAPP_ICON='+(Join-Path $project 'assets/brand/TeamHJD.ico')),('/DTERMS_FILE='+$terms))
if($TestPackage){$parameters+='/DHJD_TEST_PACKAGE'}
Invoke-Nsis ($parameters+@((Join-Path $project 'installer/windows.nsi')))
Get-Item -LiteralPath $destination|Select-Object Name,Length
Get-FileHash -LiteralPath $destination -Algorithm SHA256
