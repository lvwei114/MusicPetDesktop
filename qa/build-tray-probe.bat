@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "E:\Claude works\MusicPet"
cl /nologo /std:c++20 /EHsc /MT /DUNICODE /D_UNICODE qa\tray-probe.cpp /Foobj\ /Feqa\TrayProbe.exe /link user32.lib shell32.lib ole32.lib
