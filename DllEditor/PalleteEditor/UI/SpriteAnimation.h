#pragma once
#include "SpritePng.h"
#include <array>
#include <unordered_map>
#include <set>
#include <numeric>
#include <functional>
#include <climits>
#include <cstring>
namespace SpriteAnimation {
using Bytes=std::vector<unsigned char>;
constexpr size_t MaxOutput=128ull*1024*1024;
struct Frame {std::vector<uint32_t> pixels;unsigned ticks=1;};
inline void CheckSize(const Bytes& bytes){if(bytes.size()>MaxOutput)throw std::runtime_error("Animation export exceeds 128 MiB. Choose a shorter animation.");}
inline void BE32(Bytes& out,uint32_t n){for(int s=24;s>=0;s-=8)out.push_back(static_cast<unsigned char>(n>>s));}
inline void BE16(Bytes& out,unsigned n){out.push_back(static_cast<unsigned char>(n>>8));out.push_back(static_cast<unsigned char>(n));}
inline void LE16(Bytes& out,unsigned n){out.push_back(static_cast<unsigned char>(n));out.push_back(static_cast<unsigned char>(n>>8));}
inline uint32_t Read32(const unsigned char* p){return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];}
inline uint32_t CRC(const unsigned char* p,size_t size){uint32_t crc=~0u;while(size--){crc^=*p++;for(int b=0;b<8;++b)crc=(crc>>1)^(0xedb88320u&uint32_t(-int(crc&1)));}return ~crc;}
inline void Chunk(Bytes& out,const char* type,const Bytes& data){
 if(data.size()>MaxOutput)throw std::runtime_error("Animation chunk exceeds export limit.");
 BE32(out,uint32_t(data.size()));const size_t start=out.size();out.insert(out.end(),type,type+4);out.insert(out.end(),data.begin(),data.end());BE32(out,CRC(out.data()+start,4+data.size()));CheckSize(out);
}
inline void Validate(unsigned w,unsigned h,size_t count,unsigned plays){if(!w || !h || w>2048 || h>2048 || !count || count>2000 || plays>1000)throw std::runtime_error("Unsupported animation size, frame count or loop count.");}
using GetFrame=std::function<Frame(size_t)>;
using Cancel=std::function<void()>;
inline Bytes APNG(unsigned width,unsigned height,size_t count,unsigned plays,const GetFrame& get,const Cancel& cancel) {
 Validate(width,height,count,plays);Bytes out={137,80,78,71,13,10,26,10};uint32_t sequence=0;
 for(size_t i=0;i<count;++i) {
  cancel();auto frame=get(i);if(!frame.ticks || frame.ticks>65535)throw std::runtime_error("Invalid authored frame duration.");
  auto png=SpritePng::Encode(width,height,frame.pixels);Bytes ihdr;std::vector<Bytes> imageData;
  for(size_t pos=8;pos+12<=png.size();) {
   const uint32_t length=Read32(png.data()+pos);if(length>png.size()-pos-12)throw std::runtime_error("Invalid encoded PNG chunk.");
   const auto* type=png.data()+pos+4;Bytes data(png.begin()+pos+8,png.begin()+pos+8+length);
   if(memcmp(type,"IHDR",4)==0)ihdr=std::move(data);else if(memcmp(type,"IDAT",4)==0)imageData.push_back(std::move(data));
   pos+=12+length;
  }
  if(ihdr.size()!=13 || ihdr[8]!=8 || ihdr[9]!=6 || ihdr[12]!=0 || imageData.empty())throw std::runtime_error("PNG codec did not produce non-interlaced RGBA frames.");
  if(i==0){Chunk(out,"IHDR",ihdr);Bytes control;BE32(control,uint32_t(count));BE32(control,plays);Chunk(out,"acTL",control);}
  Bytes control;BE32(control,sequence++);BE32(control,width);BE32(control,height);BE32(control,0);BE32(control,0);
  unsigned divisor=std::gcd(frame.ticks,60u);BE16(control,frame.ticks/divisor);BE16(control,60/divisor);
  control.push_back(0);control.push_back(0); // full-canvas SOURCE replacement, no disposal
  Chunk(out,"fcTL",control);
  for(auto& data:imageData)if(i==0)Chunk(out,"IDAT",data);else {Bytes payload;BE32(payload,sequence++);payload.insert(payload.end(),data.begin(),data.end());Chunk(out,"fdAT",payload);}
 }
 cancel();Chunk(out,"IEND",{});return out;
}
struct Palette {
 std::array<uint32_t,256> colors{};unsigned used=1;
 std::unordered_map<uint32_t,unsigned char> cache;
 explicit Palette(const std::vector<uint32_t>& samples) {
  std::set<uint32_t> unique;for(auto color:samples)if((color>>24)>=128)unique.insert(color|0xff000000u);
  if(unique.empty())return;
  if(unique.size()<=255){for(auto c:unique)colors[used++]=c;return;}
  SpritePng::Apartment apartment;Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
  SpritePng::Require(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(factory.GetAddressOf())),"Could not create GIF palette quantizer.");
  const unsigned width=256,height=unsigned((unique.size()+255)/256);std::vector<uint32_t> pixels(unique.begin(),unique.end());pixels.resize(size_t(width)*height,pixels.back());
  Microsoft::WRL::ComPtr<IWICBitmap> bitmap;SpritePng::Require(factory->CreateBitmapFromMemory(width,height,GUID_WICPixelFormat32bppBGRA,width*4,UINT(pixels.size()*4),reinterpret_cast<BYTE*>(pixels.data()),bitmap.GetAddressOf()),"Could not sample GIF colors.");
  Microsoft::WRL::ComPtr<IWICPalette> palette;SpritePng::Require(factory->CreatePalette(palette.GetAddressOf()),"Could not create GIF palette.");
  SpritePng::Require(palette->InitializeFromBitmap(bitmap.Get(),255,FALSE),"Could not quantize GIF colors.");
  std::array<WICColor,256> quantized{};UINT actual=0;SpritePng::Require(palette->GetColors(255,quantized.data(),&actual),"Could not read GIF palette.");
  for(unsigned i=0;i<actual && used<256;++i)colors[used++]=quantized[i]|0xff000000u;
 }
 unsigned char Map(uint32_t color) {
  if((color>>24)<128 || used==1)return 0;
  const uint32_t rgb=color&0xffffff;auto found=cache.find(rgb);if(found!=cache.end())return found->second;
  unsigned best=1;int distance=INT_MAX;
  for(unsigned i=1;i<used;++i){int sum=0;for(int shift=0;shift<24;shift+=8){int d=int((rgb>>shift)&255)-int((colors[i]>>shift)&255);sum+=d*d;}if(sum<distance){distance=sum;best=i;if(!sum)break;}}
  if(cache.size()<131072)cache.emplace(rgb,static_cast<unsigned char>(best));return static_cast<unsigned char>(best);
 }
};
inline Bytes LZW(const std::vector<unsigned char>& indices) {
 Bytes data;uint32_t bits=0;unsigned bitCount=0,width=9,decoderNext=258;bool previous=false;
 auto write=[&](unsigned code){bits|=code<<bitCount;bitCount+=width;while(bitCount>=8){data.push_back(static_cast<unsigned char>(bits));bits>>=8;bitCount-=8;}};
 auto emit=[&](unsigned code){write(code);if(previous){++decoderNext;if(decoderNext==(1u<<width) && width<12)++width;}previous=true;};
 auto clear=[&](){write(256);width=9;decoderNext=258;previous=false;};
 clear();std::unordered_map<uint32_t,unsigned> dictionary;unsigned next=258;
 if(!indices.empty()) {
  unsigned prefix=indices[0];
  for(size_t i=1;i<indices.size();++i){unsigned value=indices[i];uint32_t key=(prefix<<8)|value;auto found=dictionary.find(key);
   if(found!=dictionary.end()){prefix=found->second;continue;}
   emit(prefix);if(next<4096)dictionary.emplace(key,next++);else{clear();dictionary.clear();next=258;}prefix=value;
  }
  emit(prefix);
 }
 write(257);if(bitCount)data.push_back(static_cast<unsigned char>(bits));return data;
}
inline Bytes GIF(unsigned width,unsigned height,size_t count,unsigned plays,const std::vector<uint32_t>& samples,const GetFrame& get,const Cancel& cancel) {
 Validate(width,height,count,plays);cancel();Palette palette(samples);Bytes out={'G','I','F','8','9','a'};
 LE16(out,width);LE16(out,height);out.insert(out.end(),{0xf7,0,0});
 for(auto c:palette.colors){out.push_back(static_cast<unsigned char>(c>>16));out.push_back(static_cast<unsigned char>(c>>8));out.push_back(static_cast<unsigned char>(c));}
 if(plays!=1){out.insert(out.end(),{0x21,0xff,11});const char* app="NETSCAPE2.0";out.insert(out.end(),app,app+11);out.insert(out.end(),{3,1});LE16(out,plays?plays-1:0);out.push_back(0);}
 uint64_t ticks=0,centiseconds=0;
 for(size_t i=0;i<count;++i) {
  cancel();auto frame=get(i);if(frame.pixels.size()!=size_t(width)*height || !frame.ticks || frame.ticks>65535)throw std::runtime_error("Invalid GIF frame.");
  ticks+=frame.ticks;uint64_t target=(ticks*100+30)/60,delay=std::max<uint64_t>(1,target-centiseconds);centiseconds=target;
  std::vector<unsigned char> indices;indices.reserve(frame.pixels.size());for(auto pixel:frame.pixels)indices.push_back(palette.Map(pixel));auto compressed=LZW(indices);
  while(delay) {
   cancel();unsigned hold=unsigned(std::min<uint64_t>(delay,65535));delay-=hold;
   out.insert(out.end(),{0x21,0xf9,4,9});LE16(out,hold);out.insert(out.end(),{0,0,0x2c});LE16(out,0);LE16(out,0);LE16(out,width);LE16(out,height);out.push_back(0);out.push_back(8);
   for(size_t pos=0;pos<compressed.size();pos+=255){size_t size=std::min(size_t(255),compressed.size()-pos);out.push_back(static_cast<unsigned char>(size));out.insert(out.end(),compressed.begin()+pos,compressed.begin()+pos+size);}
   out.push_back(0);CheckSize(out);
  }
 }
 cancel();out.push_back(0x3b);return out;
}
}
