#include "motion.h"
#include "apng.h"
#include "artwork-layout.h"
#include "artwork-render.h"
#include <wincodec.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <propvarutil.h>
#include <winrt/base.h>
#include <gdiplus.h>
#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <filesystem>
#include <cstring>
#include <fstream>
#pragma comment(lib,"mfplat.lib")
#pragma comment(lib,"mfreadwrite.lib")
#pragma comment(lib,"mfuuid.lib")
#pragma comment(lib,"propsys.lib")
namespace {
void Check(HRESULT hr){winrt::check_hresult(hr);}
UINT Meta(IWICMetadataQueryReader* r,const wchar_t* key,UINT fallback=0){
    PROPVARIANT v{};UINT n=fallback;
    if(r&&SUCCEEDED(r->GetMetadataByName(key,&v))){
        if(v.vt==VT_UI1)n=v.bVal;else if(v.vt==VT_UI2)n=v.uiVal;else if(v.vt==VT_UI4)n=v.ulVal;else if(v.vt==VT_BOOL)n=v.boolVal!=VARIANT_FALSE;
    }PropVariantClear(&v);return n;
}
}
struct MotionPlayer::Impl {
    std::mutex mutex;std::condition_variable cv;std::thread worker;
    bool quit{},playing{},active{},fresh{},failed{};unsigned generation{};
    std::wstring path;int width{},height{},speedPercent{100};bool pingPong{};std::vector<BYTE> latest;
    // Worker-only options, fixed for the current generation.
    int speed{100};bool bounce{};
    struct CachedFrame{std::vector<BYTE> pixels;int delay;};
    using Cache=std::vector<CachedFrame>;
    int Duration(int ms)const{return std::max(1,int((int64_t(ms)*100+speed/2)/speed));}
    void Emit(const std::vector<BYTE>& frame,unsigned gen){
        std::lock_guard lock(mutex);if(!quit&&active&&gen==generation){latest=frame;fresh=true;}
    }
    void Remember(Cache& cache,int ms,unsigned gen){
        if(!bounce)return;
        std::lock_guard lock(mutex);if(quit||generation!=gen)return;
        if(latest.empty())return;
        if(cache.size()>=4096||latest.size()>128ULL*1024*1024/(cache.size()+1))throw std::runtime_error("Animation cache exceeds 128 MB");
        cache.push_back({latest,Duration(ms)});
    }
    void Bounce(Cache& frames,unsigned gen){
        // Forward frames were already shown during decoding. Omit duplicated
        // endpoints: 0,1,2,3,2,1,0,1,... . A round trip is one pause cycle.
        if(frames.empty())return;
        while(!Interrupted(gen)){
            for(size_t i=frames.size()-1;i>1;){--i;Emit(frames[i].pixels,gen);if(Delay(gen,frames[i].delay))return;}
            if(CycleEnd(gen))return;
            for(const auto& frame:frames){if(Interrupted(gen))return;Emit(frame.pixels,gen);if(Delay(gen,frame.delay))return;}
        }
    }
    bool Interrupted(unsigned gen){std::lock_guard lock(mutex);return quit||generation!=gen;}
    bool CycleEnd(unsigned gen){
        std::lock_guard lock(mutex);
        if(quit||generation!=gen)return true;
        if(!playing){active=false;fresh=false;return true;}return false;
    }
    bool Delay(unsigned gen,int ms){
        std::unique_lock lock(mutex);
        return cv.wait_for(lock,std::chrono::milliseconds(ms),[&]{return quit||generation!=gen;});
    }
    void Publish(const std::vector<BYTE>& raw,UINT w,UINT h,int dw,int dh,unsigned gen){
        std::vector<BYTE> frame(size_t(dw)*dh*4);
        RenderArtwork(raw.data(),w,h,frame.data(),dw,dh);
        std::lock_guard lock(mutex);if(!quit&&active&&gen==generation){latest=std::move(frame);fresh=true;}
    }
    void Gif(const std::wstring& file,int dw,int dh,unsigned gen){
        Cache cache;
        winrt::com_ptr<IWICImagingFactory> factory;Check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,__uuidof(IWICImagingFactory),factory.put_void()));
        winrt::com_ptr<IWICBitmapDecoder> decoder;Check(factory->CreateDecoderFromFilename(file.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnDemand,decoder.put()));
        winrt::com_ptr<IWICMetadataQueryReader> metadata;Check(decoder->GetMetadataQueryReader(metadata.put()));
        UINT w=Meta(metadata.get(),L"/logscrdesc/Width"),h=Meta(metadata.get(),L"/logscrdesc/Height"),count{};
        Check(decoder->GetFrameCount(&count));
        if(!w||!h||size_t(w)*h>4000000||!count||count>4096)throw winrt::hresult_invalid_argument();
        std::vector<BYTE> canvas(size_t(w)*h*4),saved;
        WICColor background{};winrt::com_ptr<IWICPalette> palette;Check(factory->CreatePalette(palette.put()));
        if(SUCCEEDED(decoder->CopyPalette(palette.get()))){WICColor colors[256]{};UINT used{};
            if(SUCCEEDED(palette->GetColors(256,colors,&used))){const auto index=Meta(metadata.get(),L"/logscrdesc/BackgroundColorIndex");if(index<used)background=colors[index]|0xff000000;}}
        auto clear=[&](UINT x,UINT y,UINT fw,UINT fh,WICColor color){
            for(UINT row=y;row<y+fh;++row)for(UINT col=x;col<x+fw;++col)memcpy(&canvas[(size_t(row)*w+col)*4],&color,4);
        };
        while(!Interrupted(gen)){
            std::fill(canvas.begin(),canvas.end(),0);
            for(UINT i=0;i<count&&!Interrupted(gen);++i){
                winrt::com_ptr<IWICBitmapFrameDecode> f;Check(decoder->GetFrame(i,f.put()));
                winrt::com_ptr<IWICMetadataQueryReader> m;Check(f->GetMetadataQueryReader(m.put()));
                const UINT x=Meta(m.get(),L"/imgdesc/Left"),y=Meta(m.get(),L"/imgdesc/Top"),dispose=Meta(m.get(),L"/grctlext/Disposal");
                const WICColor fill=Meta(m.get(),L"/grctlext/TransparencyFlag")?0:background;
                if(i==0)clear(0,0,w,h,fill);
                UINT fw{},fh{};Check(f->GetSize(&fw,&fh));if(!fw||!fh||fw>w||fh>h||x>w-fw||y>h-fh)throw winrt::hresult_invalid_argument();
                if(dispose==3)saved=canvas;
                winrt::com_ptr<IWICFormatConverter> c;Check(factory->CreateFormatConverter(c.put()));
                Check(c->Initialize(f.get(),GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
                std::vector<BYTE> pixels(size_t(fw)*fh*4);Check(c->CopyPixels(nullptr,fw*4,(UINT)pixels.size(),pixels.data()));
                for(UINT row=0;row<fh;++row)for(UINT col=0;col<fw;++col){
                    const auto src=&pixels[(size_t(row)*fw+col)*4];auto dst=&canvas[(size_t(row+y)*w+x+col)*4];
                    for(int k=0;k<4;++k)dst[k]=BYTE(src[k]+(unsigned(dst[k])*(255-src[3])+127)/255);
                }
                Publish(canvas,w,h,dw,dh,gen);
                const int ms=std::max(20U,Meta(m.get(),L"/grctlext/Delay",10)*10);Remember(cache,ms,gen);
                if(Delay(gen,Duration(ms)))return;
                if(dispose==2)clear(x,y,fw,fh,fill);
                if(dispose==3)canvas=saved;
            }
            if(bounce){Bounce(cache,gen);return;}
            if(CycleEnd(gen))return;
        }
    }
    void Video(const std::wstring& file,int dw,int dh,unsigned gen){
        Cache cache;
        winrt::com_ptr<IMFAttributes> attrs;Check(MFCreateAttributes(attrs.put(),2));
        Check(attrs->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING,TRUE));
        winrt::com_ptr<IMFSourceReader> reader;Check(MFCreateSourceReaderFromURL(file.c_str(),attrs.get(),reader.put()));
        Check(reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS,FALSE));Check(reader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM,TRUE));
        winrt::com_ptr<IMFMediaType> request;Check(MFCreateMediaType(request.put()));Check(request->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Video));Check(request->SetGUID(MF_MT_SUBTYPE,MFVideoFormat_RGB32));
        Check(reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,nullptr,request.get()));
        ULONGLONG start=GetTickCount64(),lastEnd{};LONGLONG first=-1;unsigned empty=0;
        while(!Interrupted(gen)){
            DWORD flags{};LONGLONG pts{};winrt::com_ptr<IMFSample> sample;
            Check(reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM,0,nullptr,&flags,&pts,sample.put()));
            if(flags&MF_SOURCE_READERF_ENDOFSTREAM){
                if(first<0)throw winrt::hresult_invalid_argument();
                const auto now=GetTickCount64();if(lastEnd>now&&Delay(gen,(int)std::min<ULONGLONG>(lastEnd-now,60000)))return;
                if(bounce){Bounce(cache,gen);return;}
                if(CycleEnd(gen))return;
                PROPVARIANT pos{};pos.vt=VT_I8;pos.hVal.QuadPart=0;Check(reader->SetCurrentPosition(GUID_NULL,pos));first=-1;start=GetTickCount64();continue;
            }
            if(!sample){if(++empty>100)throw winrt::hresult_invalid_argument();continue;}empty=0;
            if(first<0){first=pts;start=GetTickCount64();}
            const auto due=start+std::max<LONGLONG>(0,(pts-first)/10000)*100/speed;
            LONGLONG duration{};if(FAILED(sample->GetSampleDuration(&duration))||duration<=0)duration=333333;
            lastEnd=due+Duration((int)std::max<LONGLONG>(1,duration/10000));
            const auto now=GetTickCount64();
            if(due>now&&Delay(gen,(int)std::min<ULONGLONG>(due-now,60000)))return;
            winrt::com_ptr<IMFMediaType> type;Check(reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,type.put()));
            UINT32 w{},h{};Check(MFGetAttributeSize(type.get(),MF_MT_FRAME_SIZE,&w,&h));
            if(!w||!h||size_t(w)*h>8300000)throw winrt::hresult_invalid_argument();
            winrt::com_ptr<IMFMediaBuffer> buffer;Check(sample->ConvertToContiguousBuffer(buffer.put()));
            std::vector<BYTE> raw(size_t(w)*h*4);
            if(auto two=buffer.try_as<IMF2DBuffer>()){
                BYTE* scan{};LONG stride{};Check(two->Lock2D(&scan,&stride));
                for(UINT32 row=0;row<h;++row)memcpy(raw.data()+size_t(row)*w*4,scan+ptrdiff_t(row)*stride,w*4);
                two->Unlock2D();
            }else{
                BYTE* data{};DWORD length{};Check(buffer->Lock(&data,nullptr,&length));
                UINT32 encoded{};LONG stride=(LONG)(w*4);if(SUCCEEDED(type->GetUINT32(MF_MT_DEFAULT_STRIDE,&encoded)))stride=(LONG)encoded;
                const size_t pitch=size_t(std::abs(stride));
                if(pitch<w*4||length<pitch*h){buffer->Unlock();throw winrt::hresult_invalid_argument();}
                for(UINT32 row=0;row<h;++row)memcpy(raw.data()+size_t(row)*w*4,data+pitch*(stride<0?h-1-row:row),w*4);
                buffer->Unlock();
            }
            for(size_t p=3;p<raw.size();p+=4)raw[p]=255;
            Publish(raw,w,h,dw,dh,gen);
            Remember(cache,(int)std::max<LONGLONG>(1,duration/10000),gen);
        }
    }
    void Apng(const std::wstring& file,int dw,int dh,unsigned gen){
        Cache cache;
        const auto animation=ReadApng(file);const auto w=animation.width,h=animation.height;
        winrt::com_ptr<IWICImagingFactory> factory;Check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,__uuidof(IWICImagingFactory),factory.put_void()));
        std::vector<BYTE> canvas(size_t(w)*h*4),saved;
        while(!Interrupted(gen)){
            std::fill(canvas.begin(),canvas.end(),0);
            for(const auto& frame:animation.frames){
                if(Interrupted(gen))return;
                winrt::com_ptr<IWICStream> stream;Check(factory->CreateStream(stream.put()));
                Check(stream->InitializeFromMemory(const_cast<BYTE*>(frame.png.data()),(DWORD)frame.png.size()));
                winrt::com_ptr<IWICBitmapDecoder> decoder;Check(factory->CreateDecoderFromStream(stream.get(),nullptr,WICDecodeMetadataCacheOnLoad,decoder.put()));
                winrt::com_ptr<IWICBitmapFrameDecode> f;Check(decoder->GetFrame(0,f.put()));
                winrt::com_ptr<IWICFormatConverter> c;Check(factory->CreateFormatConverter(c.put()));
                Check(c->Initialize(f.get(),GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
                std::vector<BYTE> pixels(size_t(frame.width)*frame.height*4);Check(c->CopyPixels(nullptr,frame.width*4,(UINT)pixels.size(),pixels.data()));
                if(frame.dispose==2)saved=canvas;
                for(unsigned y=0;y<frame.height;++y)for(unsigned x=0;x<frame.width;++x){
                    const auto src=&pixels[(size_t(y)*frame.width+x)*4];auto dst=&canvas[(size_t(y+frame.y)*w+x+frame.x)*4];
                    if(frame.blend==0)memcpy(dst,src,4);
                    else for(int k=0;k<4;++k)dst[k]=BYTE(src[k]+(unsigned(dst[k])*(255-src[3])+127)/255);
                }
                Publish(canvas,w,h,dw,dh,gen);Remember(cache,frame.delay,gen);if(Delay(gen,Duration(frame.delay)))return;
                if(frame.dispose==1)for(unsigned y=0;y<frame.height;++y)memset(&canvas[(size_t(y+frame.y)*w+frame.x)*4],0,frame.width*4);
                else if(frame.dispose==2)canvas=saved;
            }if(bounce){Bounce(cache,gen);return;}if(CycleEnd(gen))return;
        }
    }
    void Run(){
        try{winrt::init_apartment(winrt::apartment_type::multi_threaded);Check(MFStartup(MF_VERSION));
            while(true){
                std::unique_lock lock(mutex);cv.wait(lock,[&]{return quit||(playing&&!path.empty()&&!failed);});if(quit)break;
                const auto file=path;const auto gen=generation;const int w=width,h=height;speed=speedPercent;bounce=pingPong;active=true;lock.unlock();
                try{auto ext=std::filesystem::path(file).extension().wstring();for(auto& c:ext)c=(wchar_t)towlower(c);
                    if(ext==L".gif")Gif(file,w,h,gen);else if(ext==L".apng"||ext==L".png")Apng(file,w,h,gen);else if(ext==L".mp4")Video(file,w,h,gen);else throw winrt::hresult_invalid_argument();
                }catch(...){std::lock_guard guard(mutex);if(gen==generation){failed=true;active=false;fresh=false;latest.clear();}}
            }MFShutdown();winrt::uninit_apartment();
        }catch(...){std::lock_guard lock(mutex);failed=true;active=false;}
    }
};
MotionPlayer::MotionPlayer():impl(std::make_unique<Impl>()){}
MotionPlayer::~MotionPlayer(){Stop();}
void MotionPlayer::Stop(){if(!impl->worker.joinable())return;{std::lock_guard lock(impl->mutex);impl->quit=true;}impl->cv.notify_all();impl->worker.join();}
void MotionPlayer::Configure(const std::wstring& path,int w,int h,int speedPercent,bool pingPong){
    {std::lock_guard lock(impl->mutex);impl->path=path;impl->width=w;impl->height=h;impl->speedPercent=std::clamp(speedPercent,25,200);impl->pingPong=pingPong;++impl->generation;impl->active=false;impl->failed=false;impl->fresh=false;impl->latest.clear();}
    if(!impl->worker.joinable())impl->worker=std::thread([this]{impl->Run();});impl->cv.notify_all();
}
void MotionPlayer::Play(bool value){{std::lock_guard lock(impl->mutex);impl->playing=value;}impl->cv.notify_all();}
bool MotionPlayer::Frame(std::vector<BYTE>& pixels){std::lock_guard lock(impl->mutex);if(!impl->fresh)return false;pixels=impl->latest;impl->fresh=false;return true;}
bool MotionPlayer::Failed(){std::lock_guard lock(impl->mutex);return impl->failed;}
bool MotionPlayer::Active(){std::lock_guard lock(impl->mutex);return impl->active||(!impl->failed&&impl->playing&&!impl->path.empty());}
