// 极简 INI 设置：位置、形象、穿透、锁定、特效密度等。
#pragma once

#include <climits>
#include <string>

struct Settings {
    std::wstring launchPath;            // explicitly selected executable or shortcut
    std::wstring skinPath;              // 空 = 内置占位形象
    int petHeight = 200;                // 逻辑像素高度（96 DPI 基准）
    int posX = INT_MIN;                 // INT_MIN = 使用默认居中位置
    int posY = INT_MIN;
    bool clickThrough = false;
    bool locked = false;
    bool gameMode = false;
    int effectsOn = 1;                  // 0/1
    int notePalette = 0;               // 0 default / 1 artwork colors
    int hotkeyPreset = 0;              // 0..3 preferred shortcut; 4 disables it
    int density = 2;                    // 1 稀疏 / 2 正常 / 3 密集
};

std::wstring SettingsFilePath();
void LoadSettings(Settings& s);
void SaveSettings(const Settings& s);
bool IsAutoStartEnabled();
bool SetAutoStartEnabled(bool enabled);
