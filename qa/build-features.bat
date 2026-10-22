@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "E:\Claude works\MusicPet"
if not exist qa\feature-run mkdir qa\feature-run
cl /nologo /std:c++20 /EHsc /O2 /MT /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I src qa\features.cpp src\skin.cpp src\effects.cpp src\gfx.cpp src\launcher.cpp src\settings.cpp /Foobj\ /Feqa\feature-run\MusicPetFeatureTest.exe /link windowsapp.lib user32.lib shell32.lib advapi32.lib ole32.lib windowscodecs.lib gdi32.lib gdiplus.lib uuid.lib

