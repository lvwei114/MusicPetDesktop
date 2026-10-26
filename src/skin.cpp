#include "skin.h"
#include "artwork-layout.h"
#include "artwork-render.h"
#pragma comment(lib,"gdiplus.lib")
#include <wincodec.h>
#include <winrt/base.h>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <cwctype>
#include <filesystem>
#include <vector>
void PetImage::Reset(){
    if(dc&&oldBitmap)SelectObject(dc,oldBitmap);
    if(dc)DeleteDC(dc);if(bitmap)DeleteObject(bitmap);
    dc=nullptr;bitmap=oldBitmap=nullptr;bits=nullptr;width=height=0;
}
bool CreateDib(int w,int h,PetImage& out){
    if(w<=0||h<=0||size_t(w)*h>16000000)return false;
    out.Reset();BITMAPINFO info{};info.bmiHeader={sizeof(BITMAPINFOHEADER),w,-h,1,32,BI_RGB};
    out.bitmap=CreateDIBSection(nullptr,&info,DIB_RGB_COLORS,&out.bits,nullptr,0);out.dc=CreateCompatibleDC(nullptr);
    if(!out.bitmap||!out.dc){out.Reset();return false;}
    out.oldBitmap=(HBITMAP)SelectObject(out.dc,out.bitmap);out.width=w;out.height=h;return true;
}
std::wstring ExeDir(){wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);return std::filesystem::path(path).parent_path().wstring();}
std::wstring SkinFolderBesideExe(){return ExeDir()+L"\\skins";}
std::vector<SkinItem> ScanSkins(){
    std::vector<SkinItem> result;std::error_code error;
    for(const auto& entry:std::filesystem::directory_iterator(SkinFolderBesideExe(),error)){
        if(!entry.is_regular_file(error))continue;
        auto ext=entry.path().extension().wstring();for(auto& c:ext)c=(wchar_t)towlower(c);
        if(ext==L".png"||ext==L".jpg"||ext==L".jpeg"||ext==L".bmp")result.push_back({entry.path().wstring(),SkinDisplayName(entry.path().wstring())});
        if(ext==L".ini"&&entry.path().filename().wstring().ends_with(L".pet.ini")){
            SkinItem item;if(LoadPairedPreset(entry.path().wstring(),item))result.push_back(std::move(item));
        }
    }
    std::sort(result.begin(),result.end(),[](const auto& x,const auto& y){return x.label<y.label;});return result;
}
bool LoadPngImage(const std::wstring& path,int maxW,int maxH,PetImage& out,bool trimTransparent){
    try{
        winrt::com_ptr<IWICImagingFactory> factory;
        winrt::check_hresult(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,__uuidof(IWICImagingFactory),factory.put_void()));
        winrt::com_ptr<IWICBitmapDecoder> decoder;winrt::check_hresult(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,decoder.put()));
        winrt::com_ptr<IWICBitmapFrameDecode> frame;winrt::check_hresult(decoder->GetFrame(0,frame.put()));
        UINT w{},h{};winrt::check_hresult(frame->GetSize(&w,&h));
        if(!w||!h||w>16000||h>16000||size_t(w)*h>32000000)return false;
        winrt::com_ptr<IWICFormatConverter> converter;winrt::check_hresult(factory->CreateFormatConverter(converter.put()));
        winrt::check_hresult(converter->Initialize(frame.get(),GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
        // Trim transparent margins, preserving the original colors and alpha.
        std::vector<BYTE> pixels(size_t(w)*h*4);winrt::check_hresult(converter->CopyPixels(nullptr,w*4,(UINT)pixels.size(),pixels.data()));
        if(!trimTransparent){
            const auto fit=FitArtwork(w,h,maxW,maxH);
            if(!CreateDib(fit.width,fit.height,out))return false;
            RenderArtwork(pixels.data(),w,h,(BYTE*)out.bits,out.width,out.height);return true;
        }
        UINT left=0,top=0,right=w,bottom=h;
        if(trimTransparent){
            left=w;top=h;right=bottom=0;
            for(UINT y=0;y<h;++y)for(UINT x=0;x<w;++x)if(pixels[(size_t(y)*w+x)*4+3]>8){left=std::min(left,x);top=std::min(top,y);right=std::max(right,x+1);bottom=std::max(bottom,y+1);}
            if(right<=left||bottom<=top)return false;
            left=left>3?left-3:0;top=top>3?top-3:0;right=std::min(w,right+3);bottom=std::min(h,bottom+3);
        }
        WICRect crop{(INT)left,(INT)top,(INT)(right-left),(INT)(bottom-top)};
        winrt::com_ptr<IWICBitmapClipper> clipper;winrt::check_hresult(factory->CreateBitmapClipper(clipper.put()));winrt::check_hresult(clipper->Initialize(converter.get(),&crop));
        const auto fit=FitArtwork(crop.Width,crop.Height,maxW,maxH);
        const UINT dw=fit.width,dh=fit.height;
        winrt::com_ptr<IWICBitmapScaler> scaler;winrt::check_hresult(factory->CreateBitmapScaler(scaler.put()));winrt::check_hresult(scaler->Initialize(clipper.get(),dw,dh,WICBitmapInterpolationModeFant));
        if(!CreateDib(dw,dh,out))return false;
        winrt::check_hresult(scaler->CopyPixels(nullptr,dw*4,dw*dh*4,(BYTE*)out.bits));return true;
    }catch(...){out.Reset();return false;}
}

namespace {
// Compare original bytes, so identical files under different names reuse a preset.
bool SameImageFile(const std::wstring& left,const std::wstring& right){
    HANDLE a=CreateFileW(left.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(a==INVALID_HANDLE_VALUE)return false;
    HANDLE b=CreateFileW(right.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(b==INVALID_HANDLE_VALUE){CloseHandle(a);return false;}
    LARGE_INTEGER sa{},sb{};bool same=GetFileSizeEx(a,&sa)&&GetFileSizeEx(b,&sb)&&sa.QuadPart==sb.QuadPart;
    BYTE ba[16384],bb[16384];
    while(same){DWORD na{},nb{};
        if(!ReadFile(a,ba,sizeof(ba),&na,nullptr)||!ReadFile(b,bb,sizeof(bb),&nb,nullptr)||na!=nb){same=false;break;}
        if(!na)break;if(memcmp(ba,bb,na)!=0){same=false;break;}
    }
    CloseHandle(a);CloseHandle(b);return same;
}
}
bool SaveSkinPreset(const std::wstring& source,std::wstring& savedPath){
    namespace fs=std::filesystem;
    savedPath.clear();std::error_code error;
    const fs::path folder(SkinFolderBesideExe()),input(source);
    auto ext=input.extension().wstring();for(auto& c:ext)c=(wchar_t)towlower(c);
    if(ext!=L".png"&&ext!=L".jpg"&&ext!=L".jpeg"&&ext!=L".bmp")return false;
    fs::create_directories(folder,error);if(error)return false;
    // Stage a complete copy before publishing it in the menu. Never overwrite a preset.
    wchar_t temp[32768]{};
    if(!GetTempFileNameW(folder.c_str(),L"pet",0,temp))return false;
    struct TempFile {const wchar_t* path;~TempFile(){DeleteFileW(path);}} cleanup{temp};
    if(!CopyFileW(source.c_str(),temp,FALSE))return false;
    PetImage check;
    const bool valid=LoadPngImage(temp,128,128,check);check.Reset();if(!valid)return false;
    for(const auto& preset:ScanSkins())if(SameImageFile(temp,preset.path)){
        savedPath=preset.path;return true;
    }
    // Limit the filename length, leaving space for duplicate-name suffixes.
    auto stem=input.stem().wstring();if(stem.empty())stem=L"桌宠";if(stem.size()>80)stem.resize(80);
    for(int suffix=0;suffix<10000;++suffix){
        auto name=stem+(suffix?L" ("+std::to_wstring(suffix)+L")":L"")+ext;
        const auto destination=folder/name;
        if(MoveFileExW(temp,destination.c_str(),MOVEFILE_WRITE_THROUGH)){
            savedPath=destination.wstring();return true;
        }
        const auto code=GetLastError();if(code!=ERROR_ALREADY_EXISTS&&code!=ERROR_FILE_EXISTS)return false;
    }
    return false;
}

namespace {
std::wstring SkinNameKey(const std::wstring& path){
    auto name=std::filesystem::path(path).filename().wstring();std::wstring key=L"skin";
    // Encode the filename so INI separators and punctuation cannot corrupt keys.
    for(auto c:name){wchar_t hex[5];swprintf_s(hex,L"%04x",(unsigned)towlower(c));key+=hex;}return key;
}
}
std::wstring SkinDisplayName(const std::wstring& path){
    if(std::filesystem::path(path).filename().wstring().ends_with(L".pet.ini")){
        wchar_t name[256]{};GetPrivateProfileStringW(L"preset",L"Name",L"",name,256,path.c_str());
        if(name[0])return name;
    }
    wchar_t value[256]{};const auto key=SkinNameKey(path);
    GetPrivateProfileStringW(L"names",key.c_str(),L"",value,256,(ExeDir()+L"\\SkinNames.ini").c_str());
    return value[0]?value:std::filesystem::path(path).stem().wstring();
}
bool SetSkinDisplayName(const std::wstring& path,const std::wstring& name){
    if(std::filesystem::path(path).filename().wstring().ends_with(L".pet.ini"))
        return WritePrivateProfileStringW(L"preset",L"Name",name.c_str(),path.c_str())!=FALSE;
    const auto file=ExeDir()+L"\\SkinNames.ini";
    HANDLE handle=CreateFileW(file.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(handle!=INVALID_HANDLE_VALUE){const WORD bom=0xfeff;DWORD written{};const bool ok=WriteFile(handle,&bom,sizeof(bom),&written,nullptr)&&written==sizeof(bom);CloseHandle(handle);if(!ok)return false;}
    else if(GetLastError()!=ERROR_FILE_EXISTS&&GetLastError()!=ERROR_ALREADY_EXISTS)return false;
    const auto key=SkinNameKey(path);
    return WritePrivateProfileStringW(L"names",key.c_str(),name.c_str(),file.c_str())!=FALSE;
}

namespace {
std::wstring ReadPair(const std::wstring& file,const wchar_t* key){
    wchar_t value[32768]{};GetPrivateProfileStringW(L"preset",key,L"",value,32768,file.c_str());return value;
}
std::filesystem::path ResolvePairPath(const std::wstring& value){
    namespace fs=std::filesystem;const fs::path rel(value);
    if(rel.empty()||rel.is_absolute()||rel.has_root_name())return {};
    for(const auto& part:rel)if(part==L"..")return {};
    return fs::path(SkinFolderBesideExe())/rel;
}
}
bool LoadPairedPreset(const std::wstring& file,SkinItem& out){
    try{
        const auto motionValue=ReadPair(file,L"Motion");
        const auto image=ResolvePairPath(ReadPair(file,L"Image")),motion=ResolvePairPath(motionValue);
        if(!motionValue.empty()&&motion.empty())return false;
        if(image.empty()||!std::filesystem::is_regular_file(image)||(!motion.empty()&&!std::filesystem::is_regular_file(motion)))return false;
        out={image.wstring(),SkinDisplayName(file),motion.wstring(),file};
        out.motionSpeed=std::clamp((int)GetPrivateProfileIntW(L"preset",L"Speed",100,file.c_str()),25,200);
        out.motionPingPong=GetPrivateProfileIntW(L"preset",L"PingPong",0,file.c_str())!=0;return true;
    }catch(...){return false;}
}
bool SavePairedPreset(const std::wstring& image,const std::wstring& motion,const std::wstring& name,SkinItem& out,int speed,bool pingPong){
    namespace fs=std::filesystem;
    try{
        std::wstring staticFile;if(!SaveSkinPreset(image,staticFile))return false;
        const fs::path folder(SkinFolderBesideExe());fs::path dynamic;
        if(!motion.empty()){
            const fs::path input(motion);auto ext=input.extension().wstring();for(auto& c:ext)c=(wchar_t)towlower(c);
            if(ext!=L".gif"&&ext!=L".apng"&&ext!=L".png"&&ext!=L".mp4")return false;
            if(!fs::is_regular_file(input)||fs::file_size(input)>256ULL*1024*1024)return false;
            const auto motions=folder/L"motion";fs::create_directories(motions);
            for(const auto& e:fs::directory_iterator(motions))if(e.is_regular_file()&&SameImageFile(input.wstring(),e.path().wstring())){dynamic=e.path();break;}
            if(dynamic.empty()){
                auto stem=input.stem().wstring();if(stem.size()>80)stem.resize(80);
                for(int i=0;i<10000;++i){const auto candidate=motions/(stem+(i?L" ("+std::to_wstring(i)+L")":L"")+ext);
                    if(CopyFileW(input.c_str(),candidate.c_str(),TRUE)){dynamic=candidate;break;}
                    if(GetLastError()!=ERROR_FILE_EXISTS&&GetLastError()!=ERROR_ALREADY_EXISTS)return false;
                }if(dynamic.empty())return false;
            }
        }
        // Each save is a new pair, allowing one static image to have several animations.
        wchar_t temporary[32768]{};if(!GetTempFileNameW(folder.c_str(),L"pair",0,temporary))return false;
        struct Cleanup{const wchar_t* p;~Cleanup(){DeleteFileW(p);}}cleanup{temporary};
        {HANDLE h=CreateFileW(temporary,GENERIC_WRITE,0,nullptr,TRUNCATE_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(h==INVALID_HANDLE_VALUE)return false;WORD bom=0xfeff;DWORD written{};const bool ok=WriteFile(h,&bom,2,&written,nullptr)&&written==2;CloseHandle(h);if(!ok)return false;}
        const auto a=fs::relative(staticFile,folder).wstring(),b=dynamic.empty()?L"":fs::relative(dynamic,folder).wstring();
        if(!WritePrivateProfileStringW(L"preset",L"Image",a.c_str(),temporary)||!WritePrivateProfileStringW(L"preset",L"Motion",b.c_str(),temporary)||!WritePrivateProfileStringW(L"preset",L"Name",name.c_str(),temporary))return false;
        if(!WritePrivateProfileStringW(L"preset",L"Speed",std::to_wstring(std::clamp(speed,25,200)).c_str(),temporary)||!WritePrivateProfileStringW(L"preset",L"PingPong",pingPong?L"1":L"0",temporary))return false;
        WritePrivateProfileStringW(nullptr,nullptr,nullptr,temporary);
        auto stem=fs::path(staticFile).stem().wstring();if(stem.size()>80)stem.resize(80);
        for(int i=0;i<10000;++i){const auto target=folder/(stem+L"-组合"+(i?L" ("+std::to_wstring(i)+L")":L"")+L".pet.ini");
            if(MoveFileExW(temporary,target.c_str(),MOVEFILE_WRITE_THROUGH)){
                if(!SetSkinDisplayName(target.wstring(),name))return false;
                return LoadPairedPreset(target.wstring(),out);
            }if(GetLastError()!=ERROR_FILE_EXISTS&&GetLastError()!=ERROR_ALREADY_EXISTS)return false;
        }
    }catch(...){}return false;
}
bool SavePairedMotionOptions(const std::wstring& file,int speed,bool pingPong){
    // Replace one complete manifest so the pair's name and two options stay intact.
    wchar_t temp[32768]{};const auto folder=std::filesystem::path(file).parent_path().wstring();
    if(!GetTempFileNameW(folder.c_str(),L"opt",0,temp))return false;
    struct Cleanup{const wchar_t* p;~Cleanup(){DeleteFileW(p);}}cleanup{temp};
    if(!CopyFileW(file.c_str(),temp,FALSE))return false;
    if(!WritePrivateProfileStringW(L"preset",L"Speed",std::to_wstring(std::clamp(speed,25,200)).c_str(),temp)||!WritePrivateProfileStringW(L"preset",L"PingPong",pingPong?L"1":L"0",temp))return false;
    WritePrivateProfileStringW(nullptr,nullptr,nullptr,temp);
    if(!MoveFileExW(temp,file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return false;
    WritePrivateProfileStringW(nullptr,nullptr,nullptr,file.c_str());return true;
}
