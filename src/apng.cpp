#include "apng.h"
#include <filesystem>
#include <fstream>
#include <cstring>
#include <stdexcept>
#include <algorithm>
namespace {
void Need(bool v){if(!v)throw std::runtime_error("Invalid or oversized APNG");}
unsigned U32(const BYTE* p){return unsigned(p[0])<<24|unsigned(p[1])<<16|unsigned(p[2])<<8|p[3];}
unsigned U16(const BYTE* p){return unsigned(p[0])<<8|p[1];}
void Put(std::vector<BYTE>& b,unsigned n){for(int i=3;i>=0;--i)b.push_back(BYTE(n>>(i*8)));}
unsigned Crc(const BYTE* p,size_t n){unsigned c=~0U;while(n--){c^=*p++;for(int i=0;i<8;++i)c=(c>>1)^((c&1)?0xedb88320U:0);}return ~c;}
void Chunk(std::vector<BYTE>& out,const char* type,const std::vector<BYTE>& data){
    Put(out,(unsigned)data.size());const auto begin=out.size();out.insert(out.end(),type,type+4);out.insert(out.end(),data.begin(),data.end());Put(out,Crc(out.data()+begin,out.size()-begin));
}
}
ApngImage ReadApng(const std::wstring& path){
    std::ifstream f(std::filesystem::path(path),std::ios::binary|std::ios::ate);Need(bool(f));const auto length=f.tellg();
    Need(length>=8&&length<=256*1024*1024);std::vector<BYTE> bytes((size_t)length);f.seekg(0);Need(bool(f.read((char*)bytes.data(),bytes.size())));
    const BYTE signature[]={137,80,78,71,13,10,26,10};Need(!memcmp(bytes.data(),signature,8));
    ApngImage result;std::vector<BYTE> ihdr,shared;std::vector<std::vector<BYTE>> data;
    bool animated=false,sawData=false,ended=false;unsigned expected=0,sequence=0;
    for(size_t offset=8;offset<bytes.size();){
        Need(bytes.size()-offset>=12);const auto n=U32(&bytes[offset]);Need(n<=bytes.size()-offset-12);
        const auto type=&bytes[offset+4],p=&bytes[offset+8];Need(Crc(type,n+4)==U32(p+n));
        const std::string tag((const char*)type,4);std::vector<BYTE> payload(p,p+n);
        if(ihdr.empty())Need(tag=="IHDR");
        if(tag=="IHDR"){
            Need(ihdr.empty()&&n==13);ihdr=payload;result.width=U32(p);result.height=U32(p+4);
            Need(result.width&&result.height&&size_t(result.width)*result.height<=4000000);
        }else if(tag=="acTL"){
            Need(!animated&&!sawData&&n==8);animated=true;expected=U32(p);Need(expected&&expected<=4096);
        }else if(tag=="fcTL"){
            Need(animated&&n==26&&U32(p)==sequence++);if(!data.empty())Need(!data.back().empty());
            ApngFrame frame;frame.width=U32(p+4);frame.height=U32(p+8);frame.x=U32(p+12);frame.y=U32(p+16);
            frame.dispose=p[24];frame.blend=p[25];frame.delay=std::max(20U,unsigned(1000ULL*U16(p+20)/(U16(p+22)?U16(p+22):100)));
            Need(frame.width&&frame.height&&frame.width<=result.width&&frame.height<=result.height&&frame.x<=result.width-frame.width&&frame.y<=result.height-frame.height&&frame.dispose<=2&&frame.blend<=1);
            if(!sawData)Need(frame.width==result.width&&frame.height==result.height&&frame.x==0&&frame.y==0);
            result.frames.push_back(std::move(frame));data.emplace_back();Need(result.frames.size()<=expected);
        }else if(tag=="IDAT"){
            sawData=true;if(!data.empty()){Need(data.size()==1);data.back().insert(data.back().end(),p,p+n);}
        }else if(tag=="fdAT"){
            Need(sawData&&animated&&n>=4&&!data.empty()&&U32(p)==sequence++);data.back().insert(data.back().end(),p+4,p+n);
        }else if(tag=="IEND"){
            Need(n==0&&offset+n+12==bytes.size());ended=true;break;
        }else if(!sawData&&tag!="acTL"){
            // Preserve palette and color metadata required by standalone frame PNGs.
            Chunk(shared,tag.c_str(),payload);
        }else Need((type[0]&32)!=0); // reject unknown critical chunks
        offset+=n+12;
    }
    Need(ended&&animated&&sawData&&result.frames.size()==expected);
    for(size_t i=0;i<result.frames.size();++i){
        Need(!data[i].empty());auto& frame=result.frames[i];auto header=ihdr;
        for(int k=0;k<4;++k){header[k]=BYTE(frame.width>>(24-k*8));header[4+k]=BYTE(frame.height>>(24-k*8));}
        frame.png.assign(signature,signature+8);Chunk(frame.png,"IHDR",header);frame.png.insert(frame.png.end(),shared.begin(),shared.end());
        Chunk(frame.png,"IDAT",data[i]);Chunk(frame.png,"IEND",{});
    }return result;
}
