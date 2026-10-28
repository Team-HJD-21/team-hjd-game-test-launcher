param([Parameter(Mandatory)][string]$PythonPath,[int]$Port=8443,[Parameter(Mandatory)][string]$ResultPath)
$ErrorActionPreference='Stop'
try{
    if($Port -ne 8443){throw 'This operator helper is limited to the reviewed TCP8443 distribution port.'}
    $program=[IO.Path]::GetFullPath($PythonPath)
    if(-not(Test-Path -LiteralPath $program) -or [IO.Path]::GetFileName($program) -ne 'python.exe'){throw 'Expected the exact distribution Python executable.'}
    $name='TeamHJD-PrivateDistribution-TCP8443'
    $existing=Get-NetFirewallRule -Name $name -ErrorAction SilentlyContinue
    if($existing){$existing|Remove-NetFirewallRule}
    New-NetFirewallRule -Name $name -DisplayName 'TeamHJD Private Game Distribution (HTTPS TCP8443)' -Direction Inbound -Action Allow -Protocol TCP -LocalPort $Port -Program $program -Profile Any -EdgeTraversalPolicy Block|Out-Null
    @{result='PASS';rule=$name;port=$Port;program=$program}|ConvertTo-Json|Set-Content -LiteralPath $ResultPath -Encoding utf8NoBOM
}catch{@{result='FAIL';error=$_.Exception.Message}|ConvertTo-Json|Set-Content -LiteralPath $ResultPath -Encoding utf8NoBOM;throw}
