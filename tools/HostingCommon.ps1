# Shared, local-only process identity and bounded private JSON helpers.
function Assert-HostingPath([string]$Path){
    $current=[IO.Path]::GetFullPath($Path)
    while($current){
        if(Test-Path -LiteralPath $current){if((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Linked hosting paths are not allowed.'}}
        $parent=[IO.Path]::GetDirectoryName($current);if($parent -eq $current){break};$current=$parent
    }
}
function Read-HostingJson([string]$Path,[int]$Maximum){
    Assert-HostingPath $Path
    if((Get-Item -LiteralPath $Path).Length -gt $Maximum){throw 'Private JSON exceeds its size limit.'}
    [IO.File]::ReadAllText($Path,[Text.Encoding]::UTF8)|ConvertFrom-Json -AsHashtable -DateKind String -Depth 64
}
function Write-HostingJson([string]$Path,[object]$Value){
    Assert-HostingPath $Path
    $temporary=$Path+'.'+[Guid]::NewGuid().ToString('N')+'.tmp'
    try{[IO.File]::WriteAllText($temporary,($Value|ConvertTo-Json -Depth 64),[Text.UTF8Encoding]::new($false));[IO.File]::Move($temporary,$Path,$true)}finally{if([IO.File]::Exists($temporary)){[IO.File]::Delete($temporary)}}
}
function Open-HostingLock([string]$Path){
    Assert-HostingPath $Path
    try{[IO.File]::Open($Path,[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)}catch{throw 'Another operator or hosting monitor holds the private lock.'}
}
function Quote-HostingArgument([string]$Value){
    if($Value.Contains('"') -or $Value.Contains("`r") -or $Value.Contains("`n")){throw 'Invalid process argument.'}
    '"'+$Value+'"'
}
function Get-OwnedHostingProcess([hashtable]$State,[string]$Executable,[string[]]$RequiredArguments){
    if(-not $State.pid -or -not $State.startedTicks){return $null}
    $process=Get-Process -Id ([int]$State.pid) -ErrorAction SilentlyContinue
    if(-not $process){return $null}
    try{
        # A protected Windows service may reuse an old PID after reboot. Use
        # CIM creation metadata only to prove that different-image PID stale;
        # an exact creation-time match with a wrong image stays a conflict.
        $expectedName=[IO.Path]::GetFileNameWithoutExtension($Executable)
        if(-not [string]::Equals($process.ProcessName,$expectedName,[StringComparison]::OrdinalIgnoreCase)){
            $created=$null;try{$created=$process.StartTime}catch{}
            if(-not $created){
                $metadata=Get-CimInstance Win32_Process -Filter "ProcessId=$($process.Id)" -ErrorAction Stop
                if(-not $metadata.CreationDate -or $metadata.Name -ine ($process.ProcessName+'.exe')){throw 'Cannot verify reused process metadata.'}
                $created=$metadata.CreationDate
            }
            if($created.ToUniversalTime().Ticks.ToString() -cne [string]$State.startedTicks){return $null}
            throw 'Process identity conflicts with hosting state.'
        }
        if($process.StartTime.ToUniversalTime().Ticks.ToString() -cne [string]$State.startedTicks){return $null}
        if(-not [string]::Equals($process.Path,$Executable,[StringComparison]::OrdinalIgnoreCase)){throw 'Process identity conflicts with hosting state.'}
        $command=(Get-CimInstance Win32_Process -Filter "ProcessId=$($process.Id)" -ErrorAction Stop).CommandLine
        foreach($argument in $RequiredArguments){
            $escaped=[Regex]::Escape($argument)
            if(-not [Regex]::IsMatch($command,'(?<!\S)(?:"'+$escaped+'"|'+$escaped+')(?!\S)',[Text.RegularExpressions.RegexOptions]::IgnoreCase)){throw 'Process arguments conflict with hosting state.'}
        }
        return $process
    }catch{throw 'An alive process cannot be verified as owned; no process was stopped.'}
}
function New-HostingLogPaths([string]$Root,[string]$Kind){
    $directory=Join-Path $Root 'runtime-logs';Assert-HostingPath $directory
    [IO.Directory]::CreateDirectory($directory)|Out-Null
    $stem=$Kind+'-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmss')+'-'+[Guid]::NewGuid().ToString('N')
    @{stdout=(Join-Path $directory ($stem+'.stdout.log'));stderr=(Join-Path $directory ($stem+'.stderr.log'))}
}
