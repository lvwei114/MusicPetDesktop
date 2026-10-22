$ErrorActionPreference='Stop'
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class CopyDataProbe {
 [StructLayout(LayoutKind.Sequential)] public struct Data {public UIntPtr id;public int size;public IntPtr data;}
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls,string name);
 [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] public static extern IntPtr CreateWindowEx(uint ex,string cls,string title,uint style,int x,int y,int w,int h,IntPtr parent,IntPtr menu,IntPtr instance,IntPtr param);
 [DllImport("user32.dll")] public static extern bool DestroyWindow(IntPtr h);
 [DllImport("user32.dll",SetLastError=true)] public static extern IntPtr SendMessageTimeout(IntPtr h,uint m,IntPtr w,ref Data l,uint flags,uint timeout,out UIntPtr result);
}
"@
$tray=[CopyDataProbe]::FindWindow('Shell_TrayWnd',$null)
$data=New-Object CopyDataProbe+Data
$data.id=[UIntPtr]::new([UInt64]2147483646)
$result=[UIntPtr]::Zero
$ok=[CopyDataProbe]::SendMessageTimeout($tray,0x4a,[IntPtr]::Zero,[ref]$data,3,1500,[ref]$result)
$apiError=[Runtime.InteropServices.Marshal]::GetLastWin32Error()
[ordered]@{host=(Get-Process -Id $PID).Path;messageSuccess=$ok.ToInt64();error=$apiError;reply=$result.ToUInt64()} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'trusted-host-report.json') -Encoding utf8



$own=[CopyDataProbe]::CreateWindowEx(0,'STATIC','Trusted host probe',0,0,0,100,100,[IntPtr]::Zero,[IntPtr]::Zero,[IntPtr]::Zero,[IntPtr]::Zero)
$ok=[CopyDataProbe]::SendMessageTimeout($tray,0x4a,$own,[ref]$data,3,1500,[ref]$result)
$apiError=[Runtime.InteropServices.Marshal]::GetLastWin32Error()
[ordered]@{ownerMessageSuccess=$ok.ToInt64();error=$apiError;reply=$result.ToUInt64();ownerCreated=($own -ne [IntPtr]::Zero)} | ConvertTo-Json | Add-Content -LiteralPath (Join-Path $PSScriptRoot 'trusted-host-report.json') -Encoding utf8
[CopyDataProbe]::DestroyWindow($own) | Out-Null

# SIG # Begin signature block
# MIIG9AYJKoZIhvcNAQcCoIIG5TCCBuECAQExDzANBglghkgBZQMEAgEFADB5Bgor
# BgEEAYI3AgEEoGswaTA0BgorBgEEAYI3AgEeMCYCAwEAAAQQH8w7YFlLCE63JNLG
# KX7zUQIBAAIBAAIBAAIBAAIBADAxMA0GCWCGSAFlAwQCAQUABCCt4HzemCYGoyJI
# Hl5MU7HFt7mx9tSuetQYkY3x+2SJpaCCA/QwggPwMIICWKADAgECAhAYXfXHK7VY
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
# BDEiBCDiiPstMKbvnw0eAVwAFpkGwX98cqlgPymdjp2qTMsSjzANBgkqhkiG9w0B
# AQEFAASCAYBN+w1IcIP5PZVQLIXFE6kIO3xU4paCtKKVg904DOXy9VkOYHZDdcM5
# yjwCklAXw678aUcRrMQiYoL9gqoOcMYtxL6UA1W6TPKf5sy1tykxQIGOt5UVn9my
# BE1H4rZQXW7Ls80KNctpwY98834QVqNFynYQ1KkZIvIGBb7nwPUBiHc+ya/pTsBY
# UFLfwyZyfsyWJEDLcc+ezL95SpF5jyHkRxejSa3mIwU3ii9neq++fPjPGU0uPakq
# A/m8B80QrAKnyDvKkfxwn4h7Ey8OWCL0VuuAMEkZlxp2xtNdr8j/BnE4RpnSQOMT
# ac79iMZRpOxrg33GZ9v9g5AkGEJU4PqnZJTls1gq+yOyNeCmFcBihxbQIJBRWXJX
# qQvTDRmOsdC3EOTl5rfzRYaQuesGj5KEd/ZSYKh7hEl5eu2hR63fQN8E60/tGajc
# tRGYkJUzYIwoG7UQSIFNWcm3neRgNWYCUhkTKT1mHPBVuJ5DvynMpRARTOmnfp55
# TJfaPhyyDiw=
# SIG # End signature block
