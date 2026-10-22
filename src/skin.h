#pragma once
#include <windows.h>
#include <string>
#include <vector>
struct SkinItem{std::wstring path,label;};
// Owning DIB, explicitly released before GDI+ shutdown by the application.
struct PetImage{
    HBITMAP bitmap{},oldBitmap{};HDC dc{};void* bits{};int width{},height{};
    void Reset();
};
std::wstring ExeDir();
std::wstring SkinFolderBesideExe();
std::vector<SkinItem> ScanSkins();
bool CreateDib(int w,int h,PetImage& out);
bool LoadPngImage(const std::wstring& path,int maxW,int maxH,PetImage& out);

// Copy a selected image into the portable preset library; never overwrite originals.
bool SaveSkinPreset(const std::wstring& source, std::wstring& savedPath);

std::wstring SkinDisplayName(const std::wstring& path);
bool SetSkinDisplayName(const std::wstring& path,const std::wstring& name);
