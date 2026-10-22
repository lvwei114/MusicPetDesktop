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
    s.density = std::clamp((int)GetPrivateProfileIntW(L"pet", L"Density", 1, f.c_str()), 1, 3);
}
void SaveSettings(const Settings& s) {
    const auto f = SettingsFilePath();
    const auto put = [&](const wchar_t* k, int v) { WritePrivateProfileStringW(L"pet", k, std::to_wstring(v).c_str(), f.c_str()); };
    WritePrivateProfileStringW(L"pet", L"Skin", s.skinPath.c_str(), f.c_str());
    put(L"Height", s.petHeight); put(L"X", s.posX); put(L"Y", s.posY);
    put(L"ClickThrough", s.clickThrough); put(L"Locked", s.locked); put(L"GameMode", s.gameMode);
    put(L"Effects", s.effectsOn); put(L"Density", s.density);
}
