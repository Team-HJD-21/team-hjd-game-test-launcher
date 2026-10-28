#requires -Version 7.5
[CmdletBinding()]
param(
    [string]$PrivateRoot=(Join-Path (Split-Path $PSScriptRoot) '.local/production-distribution'),
    [Alias('OutputCsv')][string]$CsvPath,
    [switch]$Force,
    [switch]$AsJson
)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'HostingCommon.ps1')
$root=[IO.Path]::GetFullPath($PrivateRoot);Assert-HostingPath $root
$path=Join-Path $root 'verified-users.json';$users=@()
if([IO.File]::Exists($path)){
    $audit=Read-HostingJson $path (8*1024*1024)
    if($audit.schema -ne 1 -or $audit.users -isnot [array] -or $audit.users.Count -gt 10000){throw 'Invalid verified user audit file.'}
    $users=@(@(foreach($user in $audit.users){
        if($user.email -isnot [string] -or $user.email.Contains("`r") -or $user.email.Contains("`n")){throw 'Invalid verified email record.'}
        [pscustomobject]@{Email=$user.email;FirstVerifiedAt=$user.firstVerifiedAt;LastVerifiedAt=$user.lastVerifiedAt;LastAccessAt=$user.lastAccessAt;LoginCount=$user.loginCount;Source=$user.source}
    })|Sort-Object Email)
}
if($CsvPath){
    $output=[IO.Path]::GetFullPath($CsvPath);Assert-HostingPath $output
    if([IO.Path]::GetExtension($output) -ine '.csv'){throw 'CSV output must have a .csv extension.'}
    if([IO.File]::Exists($output) -and -not $Force){throw 'CSV already exists; pass -Force to overwrite explicitly.'}
    if(-not [IO.Directory]::Exists([IO.Path]::GetDirectoryName($output))){throw 'CSV parent directory must already exist.'}
    $safe=@(foreach($user in $users){$copy=$user.PSObject.Copy();if($copy.Email -match '^[=+\-@]'){$copy.Email="'"+$copy.Email};$copy})
    $temporary=$output+'.'+[Guid]::NewGuid().ToString('N')+'.tmp'
    try{
        if($safe.Count){$safe|Export-Csv -LiteralPath $temporary -NoTypeInformation -Encoding utf8BOM}else{[IO.File]::WriteAllText($temporary,'"Email","FirstVerifiedAt","LastVerifiedAt","LastAccessAt","LoginCount","Source"'+[Environment]::NewLine,[Text.UTF8Encoding]::new($true))}
        [IO.File]::Move($temporary,$output,[bool]$Force)
    }finally{if([IO.File]::Exists($temporary)){[IO.File]::Delete($temporary)}}
}
if($AsJson){ConvertTo-Json -InputObject @($users) -Depth 5}else{$users}
