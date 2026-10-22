# Personal-use signing. Trust applies only to the current Windows user.
$ErrorActionPreference = 'Stop'
$signingDir = Join-Path $PSScriptRoot 'signing'
New-Item -ItemType Directory -Force -Path $signingDir | Out-Null
$cert = Get-ChildItem Cert:\CurrentUser\My -CodeSigningCert |
    Where-Object { $_.Subject -eq 'CN=LvWei' -and $_.FriendlyName -eq 'MusicPet LvWei personal code signing' -and $_.HasPrivateKey -and $_.NotAfter -gt (Get-Date).AddDays(30) } |
    Sort-Object NotAfter -Descending | Select-Object -First 1
if (-not $cert) {
    $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=LvWei' `
        -FriendlyName 'MusicPet LvWei personal code signing' `
        -CertStoreLocation 'Cert:\CurrentUser\My' -HashAlgorithm SHA256 `
        -KeyAlgorithm RSA -KeyLength 3072 -KeyExportPolicy NonExportable `
        -NotAfter (Get-Date).AddYears(3)
}
# Public certificate only. Never export the signing private key to the workspace.
$publicCert = Join-Path $signingDir 'LvWei.cer'
Export-Certificate -Cert $cert -FilePath $publicCert -Force | Out-Null
foreach ($store in @('Cert:\CurrentUser\Root', 'Cert:\CurrentUser\TrustedPublisher')) {
    if (-not (Test-Path -LiteralPath (Join-Path $store $cert.Thumbprint))) {
        Import-Certificate -FilePath $publicCert -CertStoreLocation $store | Out-Null
    }
}
$exe = Join-Path $PSScriptRoot 'MusicPet.exe'
$signature = Set-AuthenticodeSignature -FilePath $exe -Certificate $cert -HashAlgorithm SHA256
# Verify in a fresh process to avoid cached trust chains after first import.
& (Get-Process -Id $PID).Path -NoProfile -File (Join-Path $PSScriptRoot 'verify-signature.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Fresh-process signature verification failed.' }
