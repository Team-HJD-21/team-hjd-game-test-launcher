#Requires -Version 7.0
param([ValidateSet('Plan','Tags','All')][string]$Scope='Plan')
$ErrorActionPreference='Stop'
$repository='Team-HJD-21/team-hjd-game-test-launcher'
$output=Join-Path (Split-Path $PSScriptRoot) '.local/repository-security'
$tagCondition=@{ref_name=@{include=@('refs/tags/v*');exclude=@()}}
$policies=@(
    @{name='TeamHJD release tag immutability';target='tag';enforcement='active';bypass_actors=@();conditions=$tagCondition;rules=@(@{type='deletion'},@{type='update'})},
    @{name='TeamHJD release tag publishers';target='tag';enforcement='active';bypass_actors=@(@{actor_id=5;actor_type='RepositoryRole';bypass_mode='always'});conditions=$tagCondition;rules=@(@{type='creation'})}
)
# The owner removed the agent-added main ruleset on 2026-11-05. All remains
# compatible with older invocations but manages release-tag rules only.
if($Scope -eq 'Plan'){
    $policies|ConvertTo-Json -Depth 12
    return
}
[IO.Directory]::CreateDirectory($output)|Out-Null
$raw=gh api "repos/$repository/rulesets?includes_parents=true"
if($LASTEXITCODE -ne 0){throw 'Cannot read existing repository rules.'}
$existing=@($raw|ConvertFrom-Json)
$results=@()
foreach($policy in $policies){
    if($Scope -eq 'Tags' -and $policy.target -ne 'tag'){continue}
    $matches=@($existing|Where-Object name -CEQ $policy.name)
    if($matches.Count -gt 1){throw 'Ambiguous managed repository policy; existing rules were preserved.'}
    if($matches.Count -eq 0){
        $inputFile=Join-Path $output ($policy.name.Replace(' ','-')+'.json')
        [IO.File]::WriteAllText($inputFile,($policy|ConvertTo-Json -Depth 12),[Text.UTF8Encoding]::new($false))
        $raw=gh api --method POST "repos/$repository/rulesets" --input $inputFile
        if($LASTEXITCODE -ne 0){throw ('Cannot create repository rule: '+$policy.name)}
        $created=$raw|ConvertFrom-Json
        $id=$created.id
    }else{$id=$matches[0].id}
    $raw=gh api "repos/$repository/rulesets/$id"
    if($LASTEXITCODE -ne 0){throw 'Cannot read back repository rule.'}
    $actual=$raw|ConvertFrom-Json
    if($actual.enforcement -cne 'active' -or $actual.target -cne $policy.target -or
       @($actual.conditions.ref_name.include).Count -ne 1 -or
       $actual.conditions.ref_name.include[0] -cne $policy.conditions.ref_name.include[0] -or
       @($actual.conditions.ref_name.exclude).Count -ne 0){throw 'Existing policy differs; it was preserved for review.'}
    $types=@($actual.rules|ForEach-Object type)
    foreach($rule in $policy.rules){if($rule.type -cnotin $types){throw 'Required repository protection is missing.'}}
    if($policy.name -ceq 'TeamHJD release tag publishers'){
        if(@($actual.bypass_actors).Count -ne 1 -or $actual.bypass_actors[0].actor_id -ne 5 -or
           $actual.bypass_actors[0].actor_type -cne 'RepositoryRole' -or $actual.bypass_actors[0].bypass_mode -cne 'always'){throw 'Unexpected release publisher bypass policy.'}
    }elseif(@($actual.bypass_actors).Count -ne 0){throw 'Unexpected protection bypass actor.'}
    $results+=@{name=$actual.name;id=$id;target=$actual.target;enforcement=$actual.enforcement;verified=$true}
}
$proof=@{result='PASS';repository=$repository;scope=$Scope;verifiedAt=[DateTime]::UtcNow.ToString('o');policies=$results}
[IO.File]::WriteAllText((Join-Path $output ('verified-'+$Scope.ToLowerInvariant()+'.json')),($proof|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
$proof|ConvertTo-Json -Depth 8
