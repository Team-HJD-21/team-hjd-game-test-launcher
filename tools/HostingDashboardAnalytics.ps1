# Pure aggregates over the existing registry. Never read tokens or infer gameplay.
function Get-DashboardAnalytics([object[]]$Users,[DateTimeOffset]$Now=[DateTimeOffset]::UtcNow){
    $zone=[TimeZoneInfo]::FindSystemTimeZoneById('Korea Standard Time')
    $today=[TimeZoneInfo]::ConvertTime($Now,$zone).ToString('yyyy-MM-dd')
    $days=[ordered]@{}
    for($offset=29;$offset -ge 0;$offset--){$days[[TimeZoneInfo]::ConvertTime($Now,$zone).AddDays(-$offset).ToString('yyyy-MM-dd')]=0}
    $result=@{Total=$Users.Count;Sessions=0L;Recent=0;Today=0;Active7=0;Active30=0;NewToday=0;New7=0;Previous7=0;Returning=0;InvalidDates=0;Days=$days;Records=@()}
    foreach($user in $Users){
        $count=0L
        if(-not [long]::TryParse([string]$user.LoginCount,[ref]$count) -or $count -lt 0){throw 'Invalid observed session count'}
        $result.Sessions+=$count
        $record=@{User=$user;First=$null;Last=$null;Today=$false;Recent=$false;Active7=$false;Active30=$false}
        foreach($field in @(@('First','FirstVerifiedAt'),@('Last','LastAccessAt'))){
            $stamp=[DateTimeOffset]::MinValue
            if([DateTimeOffset]::TryParse([string]$user.($field[1]),[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::None,[ref]$stamp) -and $stamp.Offset -eq [TimeSpan]::Zero -and $stamp -le $Now -and $stamp.Year -ge 2020){$record[$field[0]]=$stamp}else{$result.InvalidDates++}
        }
        if($null -ne $record.First){
            $day=[TimeZoneInfo]::ConvertTime($record.First,$zone).ToString('yyyy-MM-dd')
            if($days.Contains($day)){$days[$day]++}
            if($day -eq $today){$result.NewToday++}
            if($record.First -ge $Now.AddDays(-7)){$result.New7++}
            elseif($record.First -ge $Now.AddDays(-14)){$result.Previous7++}
        }
        if($null -ne $record.Last){
            $record.Today=([TimeZoneInfo]::ConvertTime($record.Last,$zone).ToString('yyyy-MM-dd') -eq $today)
            $record.Recent=($record.Last -ge $Now.AddMinutes(-5))
            $record.Active7=($record.Last -ge $Now.AddDays(-7))
            $record.Active30=($record.Last -ge $Now.AddDays(-30))
            if($record.Today){$result.Today++;if($null -ne $record.First -and [TimeZoneInfo]::ConvertTime($record.First,$zone).ToString('yyyy-MM-dd') -lt $today){$result.Returning++}}
            if($record.Recent){$result.Recent++};if($record.Active7){$result.Active7++};if($record.Active30){$result.Active30++}
        }
        $result.Records+=,$record
    }
    return $result
}
