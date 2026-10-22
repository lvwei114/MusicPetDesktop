#include <windows.h>
#include <gdiplus.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <chrono>
#include "skin.h"
#include "effects.h"
#include "gfx.h"
#include "launcher.h"
#include "settings.h"
#pragma comment(lib,"gdiplus.lib")
namespace fs=std::filesystem;
void Check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main(){
    std::ofstream report("qa/features-report.txt");ULONG_PTR token{};Gdiplus::GdiplusStartupInput input;
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);Gdiplus::GdiplusStartup(&token,&input,nullptr);
    int result=0;
    try{
        const fs::path root(ExeDir());fs::create_directories(root/L"inputs");
        const auto source=(root.parent_path().parent_path()/L"skins"/L"Ellie.png").wstring();
        std::wstring first,second,third;
        Check(SaveSkinPreset(source,first),"preset copy failed");
        fs::copy_file(source,root/L"inputs"/L"renamed.png",fs::copy_options::overwrite_existing);
        Check(SaveSkinPreset((root/L"inputs"/L"renamed.png").wstring(),second)&&first==second,"content dedup failed");
        Check(SetSkinDisplayName(first,L"小艾莉 = 晚风"),"custom name write failed");
        Check(SkinDisplayName(first)==L"小艾莉 = 晚风","custom name read failed");
        const auto skins=ScanSkins();bool visible=false;for(const auto& s:skins)if(s.path==first&&s.label==L"小艾莉 = 晚风")visible=true;
        Check(visible,"preset menu name missing");
        fs::copy_file(source,root/L"inputs"/L"Ellie.png",fs::copy_options::overwrite_existing);
        // A valid image with different file bytes but the same name must not overwrite.
        {std::ofstream append(root/L"inputs"/L"Ellie.png",std::ios::app|std::ios::binary);append<<"different";}
        Check(SaveSkinPreset((root/L"inputs"/L"Ellie.png").wstring(),third)&&third!=first,"same-name collision failed");
        Check(SkinDisplayName(first)==L"小艾莉 = 晚风","name changed after adding collision");
        const auto bad=root/L"inputs"/L"bad.png";{std::ofstream file(bad);file<<"not an image";}
        std::wstring ignored;Check(!SaveSkinPreset(bad.wstring(),ignored),"invalid image imported");
        report<<"PASS: presets copy, byte dedup, collision preservation, Unicode names and invalid image rejection\n";
        for(auto dpi:{96U,168U,288U})for(int height:{100,180,300,600})for(float aspect:{.25f,.65f,2.f}){
            const int h=MulDiv(height,dpi,96),w=(int)(h*aspect);auto layout=ComputeNoteLayout(w,h,dpi);
            Check(layout.scale>0&&layout.margin>0&&layout.top>0,"invalid adaptive layout");
            Check(layout.margin>=.0832*w+(11*4.1+6+27)*layout.scale,"horizontal clipping bound");
            Check(layout.top+h*.39>= (18+33*4.1+27)*layout.scale,"vertical clipping bound");
            NoteLayer notes;notes.Configure(3,layout.scale);notes.SetOrigin(layout.margin+w*.5f,layout.top+h*.39f,w*.54f);
            notes.SetPalette(1,nullptr,0,0);notes.SetPlaying(true);for(int i=0;i<160;++i)notes.Tick(1./30);
        }
        report<<"PASS: adaptive layout bounds across size, aspect ratio and DPI\n";
        Settings settings;settings.launchPath=L"C:\\程序\\MusicFree.exe";settings.notePalette=1;settings.hotkeyPreset=3;
        SaveSettings(settings);Settings loaded;LoadSettings(loaded);
        Check(loaded.launchPath==settings.launchPath&&loaded.notePalette==1&&loaded.hotkeyPreset==3,"settings persistence failed");
        report<<"PASS: launch path, palette mode and shortcut preference persistence\n";
        const auto start=GetTickCount64();const auto apps=FindMusicApps();
        for(const auto& app:apps)Check(IsLaunchTarget(app.path),"discovered invalid target");
        Check(!OpenLaunchTarget((root/L"missing.exe").wstring(),nullptr),"missing launcher target accepted");
        report<<"PASS: bounded discovery, valid candidates and missing-target failure ("<<apps.size()<<" candidates, "<<GetTickCount64()-start<<" ms)\n";
        {
            using namespace Gdiplus;Bitmap sheet(800,280,PixelFormat32bppPARGB);
            {Graphics g(&sheet);g.Clear(Color(255,245,247,243));g.SetSmoothingMode(SmoothingModeAntiAlias);
            for(int i=0;i<4;++i)DrawNoteGlyph(g,100.f+i*190,80,80,0,Color(255,80,135,120),i);
            PetImage image;Check(LoadPngImage(source,250,250,image),"palette source decode failed");
            for(int mode=0;mode<2;++mode){NoteLayer notes;notes.SetPalette(mode,image.bits,image.width,image.height);notes.Configure(3,1);notes.SetOrigin(190.f+mode*400,250,90);notes.SetPlaying(true);for(int i=0;i<100;++i)notes.Tick(1./30);notes.Paint(g);}image.Reset();}
            UINT count{},size{};GetImageEncodersSize(&count,&size);std::vector<BYTE> buffer(size);auto enc=(ImageCodecInfo*)buffer.data();GetImageEncoders(count,size,enc);
            for(UINT i=0;i<count;++i)if(wcscmp(enc[i].MimeType,L"image/png")==0)Check(sheet.Save((root.parent_path()/L"note-styles.png").c_str(),&enc[i].Clsid)==Ok,"sheet save failed");
        }
        report<<"PASS: note glyphs and both palette modes rendered\n";
    }catch(const std::exception& e){report<<"FAIL: "<<e.what()<<"\n";result=1;}
    Gdiplus::GdiplusShutdown(token);CoUninitialize();return result;
}
