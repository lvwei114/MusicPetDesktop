$ErrorActionPreference = 'Stop'
$verified = Get-AuthenticodeSignature -FilePath (Join-Path $PSScriptRoot 'MusicPet.exe')
if ($verified.Status -ne 'Valid') { throw "Signature verification failed: $($verified.Status), $($verified.StatusMessage)" }
$cert=$verified.SignerCertificate
[ordered]@{
    status = [string]$verified.Status
    signer = $cert.Subject
    thumbprint = $cert.Thumbprint
    expires = $cert.NotAfter.ToString('o')
    algorithm = 'SHA256'
    trustScope = 'CurrentUser Root and TrustedPublisher'
    privateKeyExported = $false
    timestamped = $false
    sha256 = (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'MusicPet.exe') -Algorithm SHA256).Hash
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'signing\signature-report.json') -Encoding utf8
$verified | Select-Object Status,StatusMessage,@{n='Signer';e={$_.SignerCertificate.Subject}} | Format-List
