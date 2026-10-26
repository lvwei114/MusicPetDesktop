#include "motion.h"
#include "apng.h"
#include "artwork-render.h"
#include <gdiplus.h>
#include <fstream>
#include <set>
#include <chrono>
#include <thread>
#include <stdexcept>
#include <filesystem>
using namespace std::chrono_literals;
static void Require(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
static unsigned Hash(const std::vector<BYTE>& p){unsigned v=2166136261;for(auto b:p)v=(v^b)*16777619;return v;}
int main(){
    std::ofstream report("motion-report.txt");Gdiplus::GdiplusStartupInput input;ULONG_PTR token{};Gdiplus::GdiplusStartup(&token,&input,nullptr);int result=0;
    try{
        MotionPlayer motion;std::vector<BYTE> pixels;
        for(int height:{64,131,300,525}){
            std::vector<BYTE> idle(96*64*4);
            for(int y=10;y<=54;++y)for(int x=20;x<=70;++x){auto p=&idle[(y*96+x)*4];p[0]=70;p[1]=150;p[2]=40;p[3]=255;}
            const auto fit=FitArtwork(96,64,height*2,height);std::vector<BYTE> expected(size_t(fit.width)*fit.height*4);
            RenderArtwork(idle.data(),96,64,expected.data(),fit.width,fit.height);
            for(bool bounce:{false,true}){
                motion.Configure(L"layout.apng",fit.width,fit.height,100,bounce);motion.Play(true);const auto until=GetTickCount64()+1000;int checked=0;
                while(GetTickCount64()<until){if(motion.Frame(pixels)){
                    Require(pixels.size()==expected.size(),"paired canvas dimensions differ");
                    for(size_t i=3;i<pixels.size();i+=4)Require(pixels[i]==expected[i],"static/motion alpha geometry differs");++checked;
                }std::this_thread::sleep_for(5ms);}
                Require(checked>=3&&!motion.Failed(),"alignment frames missing");
            }
        }
        report<<"PASS: shared static/animated canvas alpha alignment at 64/131/300/525 px; forward and ping-pong\n";
        const auto wide=FitArtwork(200,100,100,100),tall=FitArtwork(100,200,100,100);
        Require(wide.y==50&&wide.x==0&&tall.y==0&&tall.x==25,"fixed bottom/center anchors differ");
        report<<"PASS: different aspect ratios keep a fixed bottom baseline and horizontal center\n";
        for(auto path:{L"fixture.gif",L"fixture.mp4",L"fixture.apng",L"fixture.png"}){
            motion.Configure(path,160,120);motion.Play(true);std::set<unsigned> frames;bool transparent=false;
            const auto end=GetTickCount64()+2600;
            while(GetTickCount64()<end){if(motion.Frame(pixels)){Require(pixels.size()==160*120*4,"frame dimensions wrong");frames.insert(Hash(pixels));
                for(size_t i=3;i<pixels.size();i+=4)transparent|=pixels[i]==0;}std::this_thread::sleep_for(10ms);}
            Require(!motion.Failed()&&frames.size()>=2,"decode/animation failed");
            if(std::wstring(path)==L"fixture.gif")Require(transparent,"GIF lost transparency");
            motion.Play(false);const auto stopping=GetTickCount64();
            while(motion.Active()&&GetTickCount64()-stopping<3000)std::this_thread::sleep_for(10ms);
            Require(!motion.Active(),"did not finish current cycle on pause");
            Require(!motion.Frame(pixels),"stopped decoder published frame");
            motion.Play(true);bool resumed=false;for(int i=0;i<150&&!resumed;++i){resumed=motion.Frame(pixels);std::this_thread::sleep_for(10ms);}Require(resumed,"resume failed");
            motion.Configure(path,100,180);bool resized=false;for(int i=0;i<150&&!resized;++i){resized=motion.Frame(pixels);std::this_thread::sleep_for(10ms);}Require(resized&&pixels.size()==100*180*4,"resize delivered stale frame");
            report<<"PASS: "<<std::filesystem::path(path).string()<<", loop, finish-cycle pause/resume, resize; unique frames "<<frames.size()<<"\n";
        }
        // Start a known long cycle, request pause after its first frame, then
        // resume before the boundary: no restart and no gap are allowed.
        motion.Configure(L"fixture.apng",96,64);motion.Play(true);
        bool first=false;for(int i=0;i<150&&!first;++i){first=motion.Frame(pixels);std::this_thread::sleep_for(10ms);}Require(first,"no APNG frame");
        const auto firstHash=Hash(pixels);motion.Play(false);std::this_thread::sleep_for(80ms);
        Require(motion.Active(),"pause cut the current cycle short");motion.Play(true);
        std::set<unsigned> observed;
        for(int i=0;i<150;++i){Require(motion.Active(),"resume failed to cancel cycle-stop");if(motion.Frame(pixels))observed.insert(Hash(pixels));std::this_thread::sleep_for(10ms);}
        Require(observed.size()>=3,"resume did not keep animation advancing");
        report<<"PASS: resume during final cycle cancels stop without restarting\n";
        std::vector<std::vector<BYTE>> golden;
        for(int i=0;i<4;++i){std::ifstream f("golden-"+std::to_string(i)+".bgra",std::ios::binary);golden.emplace_back(std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>());Require(golden.back().size()==96*64*4,"missing golden image");}
        motion.Configure(L"fixture.apng",96,64);std::set<int> matched;const auto compareEnd=GetTickCount64()+1700;
        while(GetTickCount64()<compareEnd){if(motion.Frame(pixels)){
            bool found=false;
            for(int frame=0;frame<4;++frame){bool same=true;for(size_t i=0;i<pixels.size();++i)if(std::abs(int(pixels[i])-int(golden[frame][i]))>2){same=false;break;}
                if(same){matched.insert(frame);found=true;break;}}
            if(!found){std::ofstream dump("apng-mismatch.bgra",std::ios::binary);dump.write((const char*)pixels.data(),pixels.size());}
            Require(found,"APNG pixels disagree with reference decoder");
        }std::this_thread::sleep_for(5ms);}
        Require(matched.size()==4,"not all APNG disposal/blend frames matched");
        report<<"PASS: APNG source/over, background/previous disposal and alpha agree with Pillow reference pixels\n";
        motion.Configure(L"fixture.apng",96,64);bool got=false;
        for(int i=0;i<150&&!got;++i){got=motion.Frame(pixels);std::this_thread::sleep_for(10ms);}Require(got,"missing first frame");
        motion.Play(false);std::set<unsigned> tail;const auto tailStart=GetTickCount64();
        while(motion.Active()&&GetTickCount64()-tailStart<2500){if(motion.Frame(pixels))tail.insert(Hash(pixels));std::this_thread::sleep_for(5ms);}
        Require(!motion.Active()&&GetTickCount64()-tailStart>=650&&tail.size()>=2,"pause did not finish remaining APNG frames");
        report<<"PASS: pause renders remaining frames, honors final frame delay, then becomes inactive\n";
        for(auto path:{L"invalid.apng",L"corrupt.apng"}){
            bool rejected=false;try{ReadApng(path);}catch(...){rejected=true;}Require(rejected,"malformed APNG accepted");
        }
        report<<"PASS: APNG chunk bounds and CRC rejection\n";
        for(auto path:{L"fixture.gif",L"fixture.apng",L"fixture.mp4"}){
            ULONGLONG elapsed[2]{};
            for(int mode=0;mode<2;++mode){
                motion.Configure(path,96,64,mode?200:50,false);motion.Play(true);bool gotFirst=false;
                for(int i=0;i<200&&!gotFirst;++i){gotFirst=motion.Frame(pixels);std::this_thread::sleep_for(5ms);}Require(gotFirst,"speed test no frame");
                const auto begin=GetTickCount64();motion.Play(false);
                while(motion.Active()&&GetTickCount64()-begin<4500)std::this_thread::sleep_for(5ms);
                Require(!motion.Active(),"speed test did not stop");elapsed[mode]=GetTickCount64()-begin;
            }
            Require(elapsed[0]>elapsed[1]*2&&elapsed[1]>200,"speed did not change frame timing");
            report<<"PASS: "<<std::filesystem::path(path).string()<<" half/2x speed remaining cycle "<<elapsed[0]<<"/"<<elapsed[1]<<" ms\n";
        }
        for(auto path:{L"fixture.gif",L"fixture.apng",L"fixture.mp4"}){
            const size_t count=std::wstring(path)==L"fixture.mp4"?12:4;
            motion.Configure(path,96,64,100,true);motion.Play(true);std::vector<unsigned> order;const auto until=GetTickCount64()+3500;
            while(GetTickCount64()<until){if(motion.Frame(pixels)){const auto hash=Hash(pixels);if(order.empty()||order.back()!=hash)order.push_back(hash);}if(order.size()>=2*count-1)break;std::this_thread::sleep_for(5ms);}
            Require(order.size()>=2*count-1&&!motion.Failed(),"ping-pong insufficient frames");
            for(size_t i=0;i<count;++i)Require(order[i]==order[2*count-2-i],"ping-pong order/endpoint repetition wrong");
            motion.Play(false);const auto ending=GetTickCount64();
            while(motion.Active()&&GetTickCount64()-ending<3000)std::this_thread::sleep_for(5ms);
            Require(!motion.Active(),"ping-pong did not finish cycle on pause");
            report<<"PASS: "<<std::filesystem::path(path).string()<<" ping-pong forward/reverse and pause after round trip\n";
        }
        motion.Configure(L"cache-limit.gif",512,512,200,true);motion.Play(true);
        const auto limitEnd=GetTickCount64()+6500;
        while(!motion.Failed()&&GetTickCount64()<limitEnd)std::this_thread::sleep_for(10ms);
        Require(motion.Failed()&&!motion.Active(),"oversized bounce cache was not rejected");
        report<<"PASS: 128 MB ping-pong cache cap fails safely\n";
        motion.Play(true);
        motion.Configure(L"missing.mp4",160,120);for(int i=0;i<100&&!motion.Failed();++i)std::this_thread::sleep_for(10ms);
        Require(motion.Failed(),"invalid media did not fail safely");
        const auto end=GetTickCount64();motion.Stop();Require(GetTickCount64()-end<1500,"shutdown slow");
        report<<"PASS: invalid file fallback signal and clean shutdown\n";
    }catch(const std::exception& e){report<<"FAIL: "<<e.what()<<"\n";result=1;}
    Gdiplus::GdiplusShutdown(token);return result;
}
