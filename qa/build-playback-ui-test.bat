@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "E:\Claude works\MusicPet"
cl /nologo /std:c++20 /EHsc /O2 /MT /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I src /c qa\main-playback-ui.cpp /Foobj\main-playback-ui.obj
if errorlevel 1 exit /b 1
link /nologo /SUBSYSTEM:WINDOWS /MACHINE:X64 /MANIFEST:NO /OUT:qa\MusicPetPlaybackUiTest.exe obj\main-playback-ui.obj obj\gfx.obj obj\skin.obj obj\settings.obj obj\media.obj obj\effects.obj obj\launcher.obj obj\apng.obj obj\motion.obj obj\MusicPet.res windowsapp.lib user32.lib gdi32.lib shell32.lib shlwapi.lib advapi32.lib ole32.lib oleaut32.lib uuid.lib windowscodecs.lib gdiplus.lib comctl32.lib comdlg32.lib mfplat.lib mfreadwrite.lib mfuuid.lib propsys.lib
