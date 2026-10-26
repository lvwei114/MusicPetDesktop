#include "settings.h"
#include "skin.h"
#include <algorithm>
std::wstring SettingsFilePath() { return ExeDir() + L"\\MusicPet.ini"; }
static std::wstring Read(const wchar_t* key, const wchar_t* fallback, const std::wstring& file) {
    wchar_t text[32768]{};
    GetPrivateProfileStringW(L"pet", key, fallback, text, 32768, file.c_str()); return text;
}
void LoadSettings(Settings& s) {
    const auto f = SettingsFilePath();
    s.motionPath = Read(L"Motion", L"", f);
    s.presetPath = Read(L"Preset", L"", f);
    s.motionSpeed = std::clamp((int)GetPrivateProfileIntW(L"pet",L"MotionSpeed",100,f.c_str()),25,200);
    s.motionPingPong = GetPrivateProfileIntW(L"pet",L"MotionPingPong",0,f.c_str())!=0;
    s.launchPath = Read(L"LaunchPath", L"", f);
    s.skinPath = Read(L"Skin", L"", f);
    s.petHeight = std::clamp((int)GetPrivateProfileIntW(L"pet", L"Height", 300, f.c_str()), 100, 600);
    const auto x = Read(L"X", L"", f), y = Read(L"Y", L"", f);
    s.posX = x.empty() ? INT_MIN : (int)wcstol(x.c_str(), nullptr, 10);
    s.posY = y.empty() ? INT_MIN : (int)wcstol(y.c_str(), nullptr, 10);
    // Never start irretrievably click-through: tray and shortcut remain available.
    s.clickThrough = GetPrivateProfileIntW(L"pet", L"ClickThrough", 0, f.c_str()) != 0;
    s.locked = GetPrivateProfileIntW(L"pet", L"Locked", 0, f.c_str()) != 0;
    s.gameMode = GetPrivateProfileIntW(L"pet", L"GameMode", 0, f.c_str()) != 0;
    s.effectsOn = GetPrivateProfileIntW(L"pet", L"Effects", 1, f.c_str()) != 0;
    s.notePalette = std::clamp((int)GetPrivateProfileIntW(L"pet", L"NotePalette", 0, f.c_str()), 0, 1);
    s.hotkeyPreset = std::clamp((int)GetPrivateProfileIntW(L"pet", L"HotkeyPreset", 0, f.c_str()), 0, 4);
    s.density = std::clamp((int)GetPrivateProfileIntW(L"pet", L"Density", 2, f.c_str()), 1, 3);
}
// Per-user logon startup; no administrator privileges or scheduled task needed.
static constexpr auto RunKey=L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static std::wstring AutoStartCommand(){
    wchar_t path[32768]{};
    const auto n=GetModuleFileNameW(nullptr,path,32768);
    if(!n||n>=32768)return {};
    return L"\""+std::wstring(path)+L"\"";
}
bool IsAutoStartEnabled(){
    wchar_t command[32768]{};DWORD bytes=sizeof(command),type{};
    if(RegGetValueW(HKEY_CURRENT_USER,RunKey,L"MusicPet",RRF_RT_REG_SZ,&type,command,&bytes)!=ERROR_SUCCESS)return false;
    return !AutoStartCommand().empty()&&AutoStartCommand()==command;
}
bool SetAutoStartEnabled(bool enabled){
    HKEY key{};
    if(enabled){
        const auto command=AutoStartCommand();if(command.empty())return false;
        if(RegCreateKeyExW(HKEY_CURRENT_USER,RunKey,0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr)!=ERROR_SUCCESS)return false;
        const auto result=RegSetValueExW(key,L"MusicPet",0,REG_SZ,reinterpret_cast<const BYTE*>(command.c_str()),static_cast<DWORD>((command.size()+1)*sizeof(wchar_t)));
        RegCloseKey(key);return result==ERROR_SUCCESS;
    }
    const auto opened=RegOpenKeyExW(HKEY_CURRENT_USER,RunKey,0,KEY_SET_VALUE,&key);
    if(opened==ERROR_FILE_NOT_FOUND)return true;
    if(opened!=ERROR_SUCCESS)return false;
    const auto result=RegDeleteValueW(key,L"MusicPet");RegCloseKey(key);
    return result==ERROR_SUCCESS||result==ERROR_FILE_NOT_FOUND;
}
void SaveSettings(const Settings& s) {
    const auto f = SettingsFilePath();
    WritePrivateProfileStringW(L"pet", L"Motion", s.motionPath.c_str(), f.c_str());
    WritePrivateProfileStringW(L"pet", L"Preset", s.presetPath.c_str(), f.c_str());
    const auto put = [&](const wchar_t* k, int v) { WritePrivateProfileStringW(L"pet", k, std::to_wstring(v).c_str(), f.c_str()); };
    put(L"MotionSpeed",s.motionSpeed);put(L"MotionPingPong",s.motionPingPong);
    WritePrivateProfileStringW(L"pet", L"LaunchPath", s.launchPath.c_str(), f.c_str());
    WritePrivateProfileStringW(L"pet", L"Skin", s.skinPath.c_str(), f.c_str());
    put(L"Height", s.petHeight); put(L"X", s.posX); put(L"Y", s.posY);
    put(L"ClickThrough", s.clickThrough); put(L"Locked", s.locked); put(L"GameMode", s.gameMode);
    put(L"NotePalette", s.notePalette);
    put(L"HotkeyPreset", s.hotkeyPreset);
    put(L"Effects", s.effectsOn); put(L"Density", s.density);
}
