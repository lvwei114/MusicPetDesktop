#include "skin.h"
#include "motion.h"
#include "artwork-layout.h"
#include <gdiplus.h>
#include <fstream>
#include <thread>
#include <chrono>
#include <stdexcept>
using namespace std::chrono_literals;
static void Need(bool v,const char* s){if(!v)throw std::runtime_error(s);}
static RECT Bounds(const BYTE* pixels,int w,int h){
    RECT box{w,h,0,0};for(int y=0;y<h;++y)for(int x=0;x<w;++x)if(pixels[(size_t(y)*w+x)*4+3]>8){
        box.left=std::min<LONG>(box.left,x);box.top=std::min<LONG>(box.top,y);box.right=std::max<LONG>(box.right,x+1);box.bottom=std::max<LONG>(box.bottom,y+1);}
    return box;
}
int main(){
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);Gdiplus::GdiplusStartupInput input;ULONG_PTR token{};Gdiplus::GdiplusStartup(&token,&input,nullptr);
    std::ofstream report("layout-report.txt");int result=0;
    try{
        for(int height:{64,131,300,525}){
            PetImage idle;Need(LoadPngImage(L"layout-idle.png",height*2,height,idle,false),"load full canvas");
            const auto fit=FitArtwork(96,64,height*2,height);Need(idle.width==fit.width&&idle.height==fit.height,"idle canvas dimensions");
            const auto expected=Bounds((BYTE*)idle.bits,idle.width,idle.height);
            for(bool bounce:{false,true}){
                MotionPlayer motion;motion.Configure(L"layout.apng",idle.width,idle.height,100,bounce);motion.Play(true);
                const auto end=GetTickCount64()+1050;int seen=0;std::vector<BYTE> pixels;
                while(GetTickCount64()<end){if(motion.Frame(pixels)){
                    const auto actual=Bounds(pixels.data(),idle.width,idle.height);
                    Need(std::abs(actual.left-expected.left)<=1&&std::abs(actual.top-expected.top)<=1&&std::abs(actual.right-expected.right)<=1&&std::abs(actual.bottom-expected.bottom)<=1,"idle and motion size/position disagree");++seen;
                }std::this_thread::sleep_for(5ms);}
                Need(seen>=3&&!motion.Failed(),"missing alignment frames");
            }idle.Reset();
        }
        const auto wide=FitArtwork(200,100,100,100),tall=FitArtwork(100,200,100,100);
        Need(wide.x==0&&wide.y==50&&wide.width==100&&wide.height==50,"wide frame baseline");
        Need(tall.x==25&&tall.y==0&&tall.width==50&&tall.height==100,"tall frame horizontal center");
        PetImage trimmed,full;Need(LoadPngImage(L"layout-idle.png",192,64,trimmed)&&LoadPngImage(L"layout-idle.png",192,64,full,false),"legacy load");
        Need(trimmed.width!=full.width,"legacy static crop unexpectedly changed");trimmed.Reset();full.Reset();
        report<<"PASS: static/APNG same canvas size and position at 64/131/300/525 px, forward and ping-pong, transparent margins\nPASS: fixed center/bottom anchors for different aspect ratios; standalone static crop retained\n";
    }catch(const std::exception& e){report<<"FAIL: "<<e.what()<<"\n";result=1;}
    Gdiplus::GdiplusShutdown(token);CoUninitialize();return result;
}
