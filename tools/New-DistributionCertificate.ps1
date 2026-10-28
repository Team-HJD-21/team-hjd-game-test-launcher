param([Parameter(Mandatory)][string]$DnsName,[Parameter(Mandatory)][string]$OutputDirectory,[string]$LocalAddress)
$ErrorActionPreference='Stop'
if($DnsName -notmatch '^[a-zA-Z0-9.-]+$'){throw 'Invalid DNS name.'}
$destination=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $destination -Force|Out-Null
if(Test-Path -LiteralPath (Join-Path $destination 'server-key.pem')){throw 'An existing TLS key must not be overwritten.'}
$caKey=[Security.Cryptography.RSA]::Create(3072)
$serverKey=[Security.Cryptography.RSA]::Create(2048)
try {
    $caRequest=[Security.Cryptography.X509Certificates.CertificateRequest]::new('CN=TeamHJD Private Distribution CA',$caKey,[Security.Cryptography.HashAlgorithmName]::SHA256,[Security.Cryptography.RSASignaturePadding]::Pkcs1)
    $caRequest.CertificateExtensions.Add([Security.Cryptography.X509Certificates.X509BasicConstraintsExtension]::new($true,$true,0,$true))
    $caRequest.CertificateExtensions.Add([Security.Cryptography.X509Certificates.X509KeyUsageExtension]::new([Security.Cryptography.X509Certificates.X509KeyUsageFlags]::KeyCertSign -bor [Security.Cryptography.X509Certificates.X509KeyUsageFlags]::CrlSign,$true))
    $caRequest.CertificateExtensions.Add([Security.Cryptography.X509Certificates.X509SubjectKeyIdentifierExtension]::new($caRequest.PublicKey,$false))
    $now=[DateTimeOffset]::UtcNow
    $ca=$caRequest.CreateSelfSigned($now.AddMinutes(-5),$now.AddYears(2))
    $serverRequest=[Security.Cryptography.X509Certificates.CertificateRequest]::new("CN=$DnsName",$serverKey,[Security.Cryptography.HashAlgorithmName]::SHA256,[Security.Cryptography.RSASignaturePadding]::Pkcs1)
    $san=[Security.Cryptography.X509Certificates.SubjectAlternativeNameBuilder]::new();$san.AddDnsName($DnsName);$san.AddDnsName('localhost');$san.AddIpAddress([Net.IPAddress]::Loopback)
    if($LocalAddress){$san.AddIpAddress([Net.IPAddress]::Parse($LocalAddress))}
    $serverRequest.CertificateExtensions.Add($san.Build())
    $serverRequest.CertificateExtensions.Add([Security.Cryptography.X509Certificates.X509BasicConstraintsExtension]::new($false,$false,0,$true))
    $serverRequest.CertificateExtensions.Add([Security.Cryptography.X509Certificates.X509KeyUsageExtension]::new([Security.Cryptography.X509Certificates.X509KeyUsageFlags]::DigitalSignature -bor [Security.Cryptography.X509Certificates.X509KeyUsageFlags]::KeyEncipherment,$true))
    $usage=[Security.Cryptography.OidCollection]::new();$usage.Add([Security.Cryptography.Oid]::new('1.3.6.1.5.5.7.3.1'))|Out-Null
    $serverRequest.CertificateExtensions.Add([Security.Cryptography.X509Certificates.X509EnhancedKeyUsageExtension]::new($usage,$true))
    $serial=[Security.Cryptography.RandomNumberGenerator]::GetBytes(16);$serial[0]=$serial[0] -band 127
    $certificate=$serverRequest.Create($ca,$now.AddMinutes(-5),$now.AddDays(90),$serial)
    [IO.File]::WriteAllText((Join-Path $destination 'ca-cert.pem'),$ca.ExportCertificatePem())
    [IO.File]::WriteAllText((Join-Path $destination 'server-chain.pem'),$certificate.ExportCertificatePem()+"`n"+$ca.ExportCertificatePem())
    [IO.File]::WriteAllText((Join-Path $destination 'server-key.pem'),$serverKey.ExportPkcs8PrivateKeyPem())
    $protectedCa=[Security.Cryptography.ProtectedData]::Protect($caKey.ExportPkcs8PrivateKey(),$null,[Security.Cryptography.DataProtectionScope]::CurrentUser)
    [IO.File]::WriteAllBytes((Join-Path $destination 'ca-key.dpapi'),$protectedCa)
    $certificate.Dispose();$ca.Dispose()
}finally{$caKey.Dispose();$serverKey.Dispose()}
Write-Output 'Server certificate and invite CA generated. No global certificate trust changed.'
