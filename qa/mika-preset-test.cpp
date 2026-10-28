#include "skin.h"
#include "motion.h"
#include "apng.h"
#include <gdiplus.h>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
int main(){
 CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);ULONG_PTR token{};Gdiplus::GdiplusStartupInput in;Gdiplus::GdiplusStartup(&token,&in,nullptr);int code=0;
 std::ofstream report("mika-preset-report.txt");
 try{
  SkinItem p;auto file=(std::filesystem::path(ExeDir())/L"skins/mika.pet.ini").wstring();
  if(!LoadPairedPreset(file,p)||p.label!=L"mika"||p.motionSpeed!=100||p.motionPingPong)throw std::runtime_error("preset load/options");
  bool found=false;for(auto& s:ScanSkins())if(s.label==L"mika"&&!s.motionPath.empty())found=true;if(!found)throw std::runtime_error("preset menu scan");
  PetImage image;if(!LoadPngImage(p.path,600,300,image,false))throw std::runtime_error("static decode");
  auto animation=ReadApng(p.motionPath);if(animation.width!=640||animation.height!=832||animation.frames.size()!=73)throw std::runtime_error("APNG parse");
  MotionPlayer player;player.Configure(p.motionPath,image.width,image.height);player.Play(true);std::vector<BYTE> pixels;std::set<unsigned> hashes;auto start=GetTickCount64();
  while(GetTickCount64()-start<3800){if(player.Frame(pixels)){if(pixels.size()!=size_t(image.width)*image.height*4)throw std::runtime_error("frame size");unsigned h=2166136261;for(auto b:pixels)h=(h^b)*16777619;hashes.insert(h);}Sleep(8);}
  if(player.Failed()||hashes.size()<10)throw std::runtime_error("animation decode");player.Play(false);start=GetTickCount64();while(player.Active()&&GetTickCount64()-start<5000)Sleep(10);
  if(player.Failed()||player.Active())throw std::runtime_error("finish-cycle pause");player.Stop();image.Reset();
  report<<"PASS: mika preset name, relative paths, default options and menu scan\nPASS: static decode and 640x832 / 73-frame APNG parse\nPASS: animation at static viewport, changing frames="<<hashes.size()<<", finish-cycle pause\n";
 }catch(const std::exception& e){report<<"FAIL: "<<e.what()<<"\n";code=1;}
 Gdiplus::GdiplusShutdown(token);CoUninitialize();return code;
}
