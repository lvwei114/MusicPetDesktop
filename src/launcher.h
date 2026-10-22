#pragma once
#include <string>
#include <vector>
struct LaunchTarget {std::wstring label,path;};
bool IsLaunchTarget(const std::wstring& path);
std::vector<LaunchTarget> FindMusicApps();
bool OpenLaunchTarget(const std::wstring& path,void* owner);
