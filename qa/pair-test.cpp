#include "skin.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>
namespace fs=std::filesystem;
static void Check(bool v,const char* text){if(!v)throw std::runtime_error(text);}
int main(){
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);int result=0;std::ofstream report("pair-report.txt");
    try{
        const auto root=fs::path(ExeDir());const auto image=(root/L"skins"/L"大肥鱼.png").wstring();
        const auto motion=(root/L"fixture.apng").wstring();SkinItem a,b,c,loaded;
        Check(SavePairedPreset(image,motion,L"大肥鱼·跳舞",a),"save animated pair");
        Check(SavePairedPreset(image,L"",L"大肥鱼·待机",b),"save static pair");
        Check(SavePairedPreset(image,motion,L"大肥鱼·另一套",c)&&a.presetFile!=c.presetFile,"multiple pairs with same image");
        Check(a.motionPath==c.motionPath,"identical animations not deduplicated");
        Check(LoadPairedPreset(a.presetFile,loaded)&&loaded.path==image&&!loaded.motionPath.empty(),"load animated pair");
        Check(LoadPairedPreset(b.presetFile,loaded)&&loaded.motionPath.empty(),"static pair inherited animation");
        Check(SavePairedMotionOptions(c.presetFile,50,true),"save speed/bounce");
        Check(LoadPairedPreset(c.presetFile,loaded)&&loaded.motionSpeed==50&&loaded.motionPingPong&&loaded.label==L"大肥鱼·另一套","speed/bounce persistence corrupted pair");
        Check(LoadPairedPreset(b.presetFile,loaded)&&loaded.motionSpeed==100&&!loaded.motionPingPong,"options leaked into another pair");
        Check(SavePairedMotionOptions(c.presetFile,200,false)&&LoadPairedPreset(c.presetFile,loaded)&&loaded.motionSpeed==200&&!loaded.motionPingPong,"speed/bounce reset");
        report<<"PASS: per-pair speed/bounce persistence, isolation, disable and name preservation\n";
        Check(SetSkinDisplayName(a.presetFile,L"改名组合")&&LoadPairedPreset(a.presetFile,loaded)&&loaded.label==L"改名组合","rename");
        wchar_t rel[1024]{};GetPrivateProfileStringW(L"preset",L"Motion",L"",rel,1024,a.presetFile.c_str());
        Check(!fs::path(rel).is_absolute(),"absolute motion path");
        const auto moved=root/L"moved"/L"skins";fs::create_directories(moved);
        // A moved executable uses the same relative entries; verify resolution
        // independently by checking both referenced files after a folder copy.
        fs::copy(root/L"skins",moved,fs::copy_options::recursive|fs::copy_options::overwrite_existing);
        Check(fs::exists(moved/rel),"relative motion lost after copy");
        WritePrivateProfileStringW(L"preset",L"Motion",L"..\\outside.apng",a.presetFile.c_str());
        Check(!LoadPairedPreset(a.presetFile,loaded),"motion traversal accepted");
        WritePrivateProfileStringW(L"preset",L"Motion",rel,a.presetFile.c_str());
        WritePrivateProfileStringW(L"preset",L"Image",L"..\\outside.png",a.presetFile.c_str());
        Check(!LoadPairedPreset(a.presetFile,loaded),"image traversal accepted");
        report<<"PASS: paired persistence, names, independent same-image combinations, dedup, static-only selection, relative relocation and traversal rejection\n";
    }catch(const std::exception& e){report<<"FAIL: "<<e.what()<<"\n";result=1;}
    CoUninitialize();return result;
}
