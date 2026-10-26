@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "E:\Claude works\MusicPet"
cl /nologo /std:c++20 /EHsc /O2 /MT /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I src qa\pair-test.cpp src\skin.cpp /Foobj\ /Feqa\run-20261026\MusicPetPairTest.exe /link windowsapp.lib gdi32.lib oleaut32.lib user32.lib ole32.lib windowscodecs.lib
