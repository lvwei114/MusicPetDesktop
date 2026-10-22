# Publish a personal-use build outside the low-integrity development workspace.
param([string]$Destination = (Join-Path $env:LOCALAPPDATA 'Programs\MusicPet'))
$ErrorActionPreference = 'Stop'
$source = $PSScriptRoot
$destinationPath = [IO.Path]::GetFullPath($Destination)
$sourcePath = [IO.Path]::GetFullPath($source).TrimEnd('\')
if ($destinationPath.TrimEnd('\') -eq $sourcePath -or $destinationPath.StartsWith($sourcePath+'\',[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Choose a normal user installation directory outside this development workspace.'
}
$targetExe = Join-Path $destinationPath 'MusicPet.exe'
$running = Get-Process -Name MusicPet -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $targetExe }
if ($running) { throw 'Exit the installed MusicPet before publishing an update.' }
$signature = Get-AuthenticodeSignature -LiteralPath (Join-Path $source 'MusicPet.exe')
if ($signature.Status -ne 'Valid') { throw 'Sign and verify MusicPet.exe before publishing.' }
New-Item -ItemType Directory -Force -Path $destinationPath | Out-Null
foreach ($file in @('MusicPet.exe','README.md','启动桌宠.cmd','恢复桌宠.cmd','预览效果.cmd')) {
    Copy-Item -LiteralPath (Join-Path $source $file) -Destination (Join-Path $destinationPath $file) -Force
}
$targetSkins = Join-Path $destinationPath 'skins'
New-Item -ItemType Directory -Force -Path $targetSkins | Out-Null
foreach ($skin in (Get-ChildItem -LiteralPath (Join-Path $source 'skins') -File)) {
    $target = Join-Path $targetSkins $skin.Name
    if (-not (Test-Path -LiteralPath $target)) { Copy-Item -LiteralPath $skin.FullName -Destination $target }
}
foreach ($file in @('MusicPet.ini','SkinNames.ini')) {
    $target = Join-Path $destinationPath $file
    $inputFile = Join-Path $source $file
    if ((Test-Path -LiteralPath $inputFile) -and -not (Test-Path -LiteralPath $target)) {
        Copy-Item -LiteralPath $inputFile -Destination $target
        if ($file -eq 'MusicPet.ini') {
            $config = Get-Content -LiteralPath $target -Raw
            $config = $config.Replace((Join-Path $source 'skins'),$targetSkins)
            Set-Content -LiteralPath $target -Value $config -Encoding Unicode
        }
    }
}
if ((Get-FileHash -LiteralPath $targetExe -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath (Join-Path $source 'MusicPet.exe') -Algorithm SHA256).Hash) {
    throw 'Published EXE differs from the verified build.'
}
Write-Output "Published to $destinationPath"
Write-Output 'Run the published EXE normally. Do not change workspace sandbox permissions.'

# SIG # Begin signature block
# MIIG9AYJKoZIhvcNAQcCoIIG5TCCBuECAQExDzANBglghkgBZQMEAgEFADB5Bgor
# BgEEAYI3AgEEoGswaTA0BgorBgEEAYI3AgEeMCYCAwEAAAQQH8w7YFlLCE63JNLG
# KX7zUQIBAAIBAAIBAAIBAAIBADAxMA0GCWCGSAFlAwQCAQUABCDtCiVVPqynDcig
# FiFx7NzGm9KhNqOZer/Zi2V4Ow39d6CCA/QwggPwMIICWKADAgECAhAYXfXHK7VY
# jEww5PZvQhvUMA0GCSqGSIb3DQEBCwUAMBAxDjAMBgNVBAMMBUx2V2VpMB4XDTI2
# MTAxOTE1NTc0OFoXDTI5MTAxOTE2MDc0OFowEDEOMAwGA1UEAwwFTHZXZWkwggGi
# MA0GCSqGSIb3DQEBAQUAA4IBjwAwggGKAoIBgQDWj9G8aUER9M3FjVT4JfBwME6v
# m+LlVpyzisx0TFaPBfI4+qv54qW6uMtWFU41BU58XFijowfL7hqgj+p2m+3LzVlM
# MuGhSrvimTxGuL/58urphkK8f7BcMIUzwQcXbmZsEMe8ECGSdn/RcLsYfCl35vSt
# k5PNyEoSptFIfbt1Pk7IXWTigLm+UEQLneODXLSPicn9keLcgEn0dmTfOrVHkyZX
# gABgazd283YFcXiMcQTmLz7ovr9338KBtRbC0pjJ1wyGNvfvANEdYdYEOyA3k7b8
# 1dtkWD8tGxKWr5zcyWTjbsysx2q9M+3phTD84khxv8QLfpeejE2cB/mT4t+MXc/c
# lxLT2Xh+ufMYpYps9C+qfSYlMrFudjZhY3USYW7OBOj53L4cLhPkrIU/X5eQ6PLL
# ecugvFptGeBMKmpkTGBSkJeWUCwQP4LoO37NC3gvt/meEaz27Av4/61HWc4RR0dc
# 5RjfrWaDW9iXCUbMl/efW6i2u93VseTAywVY8D0CAwEAAaNGMEQwDgYDVR0PAQH/
# BAQDAgeAMBMGA1UdJQQMMAoGCCsGAQUFBwMDMB0GA1UdDgQWBBQx+YFK10o6GA4H
# E3qtROgB6iYH6DANBgkqhkiG9w0BAQsFAAOCAYEANVcuRh2dTE6HK7IItIit98KB
# 2HGK5UG0DvxPZQU3ZrqOgo7cDpKExuoGZ3LEtTOvrwzu1FETb0usQ//PK18TwM17
# A3fs4LAcwJPzhge1DgdzqRnamDYscuo4PxKciWMi/eX2Y8fv/em85wyuab7Cx26y
# vrNb5t/tbLcsq+DuaHOFrWurYBlzB8gWUKex9/DCSgHB/Snj+t7MQ+Lsw2LiPKV9
# +inaiG+sThr6F2BGXz0lPqEpkA8M90RTVVm7IBNmcRFBIKjO7jXqbEhhagwIxL7l
# 9BXUaWzSKxvUhbW6fhezzRNgDvkUu8rlw0bzrUwsrLHpfI8/ErqSv+YLBKTaIS1Q
# 3UyrCSHgnr/j3R/SeQJzFnVXxL+oDibTO3PqccMwyJ64sMpwZRjctI8CmsRRRN0Y
# UnLo9b3KzJXxWQMrAYipsqz0Im0VducQBdENLSSMnLEXvyDLbpQ2I0j54bkrm8Du
# ZcWvo46W9nA5bqHIIZ/Ifvv8w/H2UNHzk1maYn6sMYICVjCCAlICAQEwJDAQMQ4w
# DAYDVQQDDAVMdldlaQIQGF31xyu1WIxMMOT2b0Ib1DANBglghkgBZQMEAgEFAKCB
# hDAYBgorBgEEAYI3AgEMMQowCKACgAChAoAAMBkGCSqGSIb3DQEJAzEMBgorBgEE
# AYI3AgEEMBwGCisGAQQBgjcCAQsxDjAMBgorBgEEAYI3AgEVMC8GCSqGSIb3DQEJ
# BDEiBCAFCMQeyspjA+baO0VWiBlD+7bjpQAtnECib2d0Ong6UDANBgkqhkiG9w0B
# AQEFAASCAYA+KrhJtF4Hu2Y8gN7B+WBxFgGdsVhNwSnVuxAaSS8orHoMvsOFRmHT
# f4XFWVKR13jaXeGsE2r5WeIUi2TEP9PmK5mzvAWShkhQeDghFhz6tXJjpJ5GEsdk
# 5YyHq9qh0l84nAg+ag/MraFA2C4SaIPLnzCeDeyLAhmnMeylIWQePV1KnFB6330u
# LXXEzYGRYLriuk7Fvl7SXpUMMYPOZhHVwdvb6db4YkzjCDLavSlY1GrVZ6xdPHTh
# gkffvQXAjzjeHWALNnBhY54QuGI7Ej6HDI2IWJla863jKojgjxQv5AKvU2DUsN5D
# o01cCLEWouHTweXRGyDoWGRCVBdUW9SP1wJiXZ9ddRokQe8hQHiwqNj0SLPQVqFj
# xiDfwK3C2VQktmmyhg3t22jk5gTgrFh6wzDNbgoRb6SWtaV2X37p2uZFou8L215o
# vnUKTeKhIIL7YKi6ZKX0Vq4CsPZbZeLEjkNkUJRxpYfEiHkMiTnNU0K0RhMSJoeV
# MC7qvqx7SCg=
# SIG # End signature block
