@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "E:\Claude works\MusicPet"
rc /nologo /fo "E:\Claude works\MusicPet\obj\MusicPet.res" MusicPet.rc
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /EHsc /O2 /MT /W3 /utf-8 /DUNICODE /D_UNICODE /DWINVER=0x0A00 /D_WIN32_WINNT=0x0A00 /DNOMINMAX /I "E:\Claude works\MusicPet\src" "E:\Claude works\MusicPet\src\gfx.cpp" "E:\Claude works\MusicPet\src\skin.cpp" "E:\Claude works\MusicPet\src\settings.cpp" "E:\Claude works\MusicPet\src\media.cpp" "E:\Claude works\MusicPet\src\effects.cpp" "E:\Claude works\MusicPet\src\launcher.cpp" "E:\Claude works\MusicPet\src\main.cpp" /Fo"E:\Claude works\MusicPet\obj/" /Fe"E:\Claude works\MusicPet\MusicPet.exe" /link /SUBSYSTEM:WINDOWS /MACHINE:X64 windowsapp.lib user32.lib gdi32.lib shell32.lib shlwapi.lib advapi32.lib ole32.lib oleaut32.lib uuid.lib windowscodecs.lib gdiplus.lib comctl32.lib comdlg32.lib "E:\Claude works\MusicPet\obj\MusicPet.res" /MANIFEST:NO
