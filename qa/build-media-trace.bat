@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "E:\Claude works\MusicPet"
cl /nologo /std:c++20 /EHsc /O2 /MT /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I src qa\media-switch-trace.cpp qa\media-trace-copy.cpp /Foobj\ /Feqa\MusicPetMediaTrace.exe /link windowsapp.lib user32.lib shell32.lib ole32.lib
