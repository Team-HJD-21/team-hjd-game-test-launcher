param([string]$OutputDirectory=(Join-Path (Split-Path $PSScriptRoot) '.local/tls-fixture'))
$ErrorActionPreference='Stop'
$destination=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $destination -Force|Out-Null
if(Test-Path -LiteralPath (Join-Path $destination 'server-key.pem')){throw 'Fixture already exists; preserve its certificate and key.'}
$rsa=[Security.Cryptography.RSA]::Create(2048)
try {
    $request=[Security.Cryptography.X509Certificates.CertificateRequest]::new('CN=localhost',$rsa,[Security.Cryptography.HashAlgorithmName]::SHA256,[Security.Cryptography.RSASignaturePadding]::Pkcs1)
    $san=[Security.Cryptography.X509Certificates.SubjectAlternativeNameBuilder]::new();$san.AddDnsName('localhost');$san.AddIpAddress([Net.IPAddress]::Parse('127.0.0.1'))
    $request.CertificateExtensions.Add($san.Build())
    $request.CertificateExtensions.Add([Security.Cryptography.X509Certificates.X509BasicConstraintsExtension]::new($true,$false,0,$true))
    $request.CertificateExtensions.Add([Security.Cryptography.X509Certificates.X509KeyUsageExtension]::new([Security.Cryptography.X509Certificates.X509KeyUsageFlags]::DigitalSignature -bor [Security.Cryptography.X509Certificates.X509KeyUsageFlags]::KeyEncipherment -bor [Security.Cryptography.X509Certificates.X509KeyUsageFlags]::KeyCertSign,$true))
    $certificate=$request.CreateSelfSigned([DateTimeOffset]::UtcNow.AddMinutes(-5),[DateTimeOffset]::UtcNow.AddDays(2))
    [IO.File]::WriteAllText((Join-Path $destination 'server-cert.pem'),$certificate.ExportCertificatePem())
    [IO.File]::WriteAllText((Join-Path $destination 'server-key.pem'),$rsa.ExportPkcs8PrivateKeyPem())
    $certificate.Dispose()
} finally { $rsa.Dispose() }
Write-Output 'Loopback-only certificate created. No Windows trust store or global TLS setting changed.'
