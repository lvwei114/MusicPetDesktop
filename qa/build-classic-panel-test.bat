@echo off
chcp 65001 >nul
cd /d "%~dp0.."
if not exist obj mkdir obj
if not exist qa\run-classic-panel\skins\mika mkdir qa\run-classic-panel\skins\mika
copy /y "skins\大肥鱼.png" "qa\run-classic-panel\skins\大肥鱼.png" >nul
if errorlevel 1 exit /b 1
copy /y skins\mika.pet.ini qa\run-classic-panel\skins\mika.pet.ini >nul
if errorlevel 1 exit /b 1
copy /y skins\mika\frame_0001.png qa\run-classic-panel\skins\mika\frame_0001.png >nul
if errorlevel 1 exit /b 1
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++20 /EHsc /O2 /MT /W3 /utf-8 /DUNICODE /D_UNICODE /DWINVER=0x0A00 /D_WIN32_WINNT=0x0A00 /DNOMINMAX /I src /c qa\classic-panel-test.cpp /Foobj\classic-panel-test.obj
if errorlevel 1 exit /b 1
link /nologo /SUBSYSTEM:CONSOLE /MACHINE:X64 /MANIFEST:NO /OUT:qa\run-classic-panel\MusicPetClassicPanelTest.exe obj\classic-panel-test.obj obj\gfx.obj obj\skin.obj obj\settings.obj obj\media.obj obj\effects.obj obj\launcher.obj obj\apng.obj obj\motion.obj obj\MusicPet.res windowsapp.lib user32.lib gdi32.lib shell32.lib shlwapi.lib advapi32.lib ole32.lib oleaut32.lib uuid.lib windowscodecs.lib gdiplus.lib comctl32.lib comdlg32.lib
