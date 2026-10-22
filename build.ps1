# 构建 MusicPet（原生 C++20 + C++/WinRT，无需 CMake）
# 用法：pwsh -File .\build.ps1
$ErrorActionPreference = 'Stop'

$root = $PSScriptRoot
$vcvars = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path -LiteralPath $vcvars)) {
    $vsWhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vsWhere) {
        $vs = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vs) { $vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat' }
    }
}
if (-not (Test-Path -LiteralPath $vcvars)) { throw "找不到 vcvars64.bat，请确认已安装 MSVC 生成工具" }

$obj = Join-Path $root 'obj'
New-Item -ItemType Directory -Force -Path $obj | Out-Null

$sources = @('gfx.cpp', 'skin.cpp', 'settings.cpp', 'media.cpp', 'effects.cpp', 'launcher.cpp', 'main.cpp') |
    ForEach-Object { Join-Path $root "src\$_" }

$quoted = ($sources | ForEach-Object { "`"$_`"" }) -join ' '
$compile = "cl /nologo /std:c++20 /EHsc /O2 /MT /W3 /utf-8 /DUNICODE /D_UNICODE /DWINVER=0x0A00 " +
           "/D_WIN32_WINNT=0x0A00 /DNOMINMAX /I `"$root\src`" $quoted " +
           "/Fo`"$obj/`" /Fe`"$root\MusicPet.exe`" " +
           "/link /SUBSYSTEM:WINDOWS /MACHINE:X64 windowsapp.lib user32.lib gdi32.lib " +
           "shell32.lib shlwapi.lib advapi32.lib ole32.lib oleaut32.lib uuid.lib windowscodecs.lib " +
           "gdiplus.lib comctl32.lib comdlg32.lib `"$obj\MusicPet.res`" /MANIFEST:NO"

$bat = Join-Path $obj 'build.bat'
Set-Content -LiteralPath $bat -Encoding ascii -Value @(
    '@echo off',
    "call `"$vcvars`" >nul",
    "cd /d `"$root`"",
    "rc /nologo /fo `"$obj\MusicPet.res`" MusicPet.rc",
    'if errorlevel 1 exit /b 1',
    $compile
)

& cmd.exe /c "`"$bat`""
if ($LASTEXITCODE -ne 0) { throw "编译失败，退出码 $LASTEXITCODE" }

Write-Host "构建成功: $root\MusicPet.exe"
