param([string]$OutputDirectory=(Join-Path (Split-Path $PSScriptRoot) '.deps/cloudflared/2026.10.5'))
$ErrorActionPreference='Stop'
$version='2026.10.5'
$expected='db4e4c413612a0916f3e1679d3caaf376f94911952d163028fb726fb2e0516a9'
$assetName='cloudflared-windows-amd64.exe'
$destination=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $destination -Force|Out-Null
$executable=Join-Path $destination 'cloudflared.exe'
if(Test-Path -LiteralPath $executable){
    if((Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected){throw 'Existing helper differs from the pinned official release; it was preserved.'}
}else{
    $release=Invoke-RestMethod -Uri "https://api.github.com/repos/cloudflare/cloudflared/releases/tags/$version"
    $asset=@($release.assets|Where-Object name -CEQ $assetName)
    $url="https://github.com/cloudflare/cloudflared/releases/download/$version/$assetName"
    if($asset.Count -ne 1 -or $asset[0].browser_download_url -cne $url -or $asset[0].digest -cne "sha256:$expected"){throw 'Official release metadata differs from the reviewed helper pin.'}
    $temporary=Join-Path $destination ('download-'+[Guid]::NewGuid().ToString('N')+'.part')
    try{
        Invoke-WebRequest -Uri $url -OutFile $temporary
        if((Get-Item -LiteralPath $temporary).Length -ne $asset[0].size -or (Get-FileHash -LiteralPath $temporary -Algorithm SHA256).Hash.ToLowerInvariant() -cne $expected){throw 'Official helper download verification failed.'}
        Move-Item -LiteralPath $temporary -Destination $executable
    }finally{if(Test-Path -LiteralPath $temporary){Remove-Item -LiteralPath $temporary}}
}
$license=Invoke-WebRequest -Uri "https://raw.githubusercontent.com/cloudflare/cloudflared/$version/LICENSE"
[IO.File]::WriteAllText((Join-Path $destination 'LICENSE.txt'),$license.Content,[Text.UTF8Encoding]::new($false))
@{version=$version;sha256=$expected;source="https://github.com/cloudflare/cloudflared/releases/tag/$version";asset=$assetName}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $destination 'provenance.json') -Encoding utf8NoBOM
Write-Output "Verified official cloudflared $version at $executable"
