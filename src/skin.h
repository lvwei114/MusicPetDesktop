#pragma once
#include <windows.h>
#include <string>
#include <vector>
struct SkinItem{std::wstring path,label,motionPath,presetFile;int motionSpeed{100};bool motionPingPong{};};
// Owning DIB, explicitly released before GDI+ shutdown by the application.
struct PetImage{
    HBITMAP bitmap{},oldBitmap{};HDC dc{};void* bits{};int width{},height{};
    void Reset();
};
std::wstring ExeDir();
std::wstring SkinFolderBesideExe();
std::vector<SkinItem> ScanSkins();
bool CreateDib(int w,int h,PetImage& out);
bool LoadPngImage(const std::wstring& path,int maxW,int maxH,PetImage& out,bool trimTransparent=true);

// Copy a selected image into the portable preset library; never overwrite originals.
bool SaveSkinPreset(const std::wstring& source, std::wstring& savedPath);

std::wstring SkinDisplayName(const std::wstring& path);
bool SetSkinDisplayName(const std::wstring& path,const std::wstring& name);
bool SavePairedPreset(const std::wstring& image,const std::wstring& motion,const std::wstring& name,SkinItem& out,int speed=100,bool pingPong=false);
bool LoadPairedPreset(const std::wstring& file,SkinItem& out);
bool SavePairedMotionOptions(const std::wstring& file,int speed,bool pingPong);
