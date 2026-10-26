@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "E:\Claude works\MusicPet"
cl /nologo /std:c++20 /EHsc /O2 /MT /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I src qa\artwork-layout-test.cpp src\skin.cpp src\motion.cpp src\apng.cpp /Foobj\ /Feqa\MusicPetLayoutTest.exe /link windowsapp.lib user32.lib gdi32.lib ole32.lib oleaut32.lib gdiplus.lib windowscodecs.lib
