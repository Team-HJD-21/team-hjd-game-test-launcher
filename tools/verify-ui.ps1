param([Parameter(Mandatory)][string]$Executable,[string]$QtRoot,[string]$InstalledProfile)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$output=Join-Path $root ('.local/ui-review-'+[DateTime]::Now.ToString('yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $output -Force|Out-Null
if($QtRoot){$env:PATH="$QtRoot/bin;$env:PATH"}
$cases=@(@{name='home-100';scale='1';surface='';size='1160x790'},@{name='settings-125';scale='1.25';surface='settings';size='1000x700'},@{name='help-150';scale='1.5';surface='help';size='1160x790'},@{name='toast-100';scale='1';surface='toast';size='1160x790'},@{name='notifications-100';scale='1';surface='notifications';size='1160x790'},@{name='versions-100';scale='1';surface='versions';size='1160x790'},@{name='uninstall-100';scale='1';surface='uninstall';size='1160x790'})
foreach($case in $cases){
    $env:QT_SCALE_FACTOR=$case.scale
    $profile=if($InstalledProfile -and $case.surface -in 'versions','uninstall'){$InstalledProfile}else{Join-Path $output $case.name}
    $png=Join-Path $output ($case.name+'.png');$err=Join-Path $output ($case.name+'.txt')
    $arguments=@('--data-root',('"'+$profile+'"'),'--capture',('"'+$png+'"'),'--quit-after-ms','3300','--window-size',$case.size)
    if($case.surface){$arguments+=@('--review-ui',$case.surface)}
    $p=Start-Process -FilePath $Executable -ArgumentList $arguments -PassThru -WindowStyle Hidden -RedirectStandardError $err
    if(-not $p.WaitForExit(15000)){throw "UI review did not exit: $($case.name)"}
    if($p.ExitCode -ne 0 -or -not(Test-Path -LiteralPath $png)){throw "UI review failed: $($case.name)"}
    $errors=[IO.File]::ReadAllText($err)
    if($errors -match 'ReferenceError|TypeError|Unable to assign|Cannot assign|failed to load|is not installed|Binding loop'){throw "QML error in $($case.name): $errors"}
    Write-Output "PASS $($case.name): process exit 0, native app PNG, no QML binding errors"
}
$env:QT_SCALE_FACTOR=$null
Write-Output "UI evidence: $output"
