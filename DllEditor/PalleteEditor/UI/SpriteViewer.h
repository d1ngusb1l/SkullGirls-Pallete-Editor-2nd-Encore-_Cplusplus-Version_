#pragma once
#include <fstream>
#include <set>
#include <algorithm>
#include <limits>
#include "Tools/Colors.hpp"
#include "SpritePreviewGeometry.h"
#include "SpritePreviewPixel.h"
#include "SpritePng.h"
#include "SpriteAnimation.h"
#include "../../../PalleteEditor/Data/PaletteDocument.h"
#include <future>
#include <atomic>
#include <chrono>
#include <map>
#include <memory>

namespace SpriteViewer {
struct PixelLayer {
    unsigned slot=0,offsetX=0,offsetY=0,width=0,height=0;
    int x=0,y=0;
    unsigned flags=0;
    std::vector<unsigned short> tiles;
    std::array<std::vector<unsigned char>,3> channels;
    std::vector<unsigned char> pixels;
};
struct SpriteFrameAsset {
    unsigned id=0,width=0,height=0;
    int originX=0,originY=0;
    std::vector<PixelLayer> layers;
};
struct TimelineEntry {
    unsigned sprite=0,duration=1,sprite1=0,sprite2=0;
};
struct AnimationAsset {
    unsigned id=0;
    std::string name;
    std::vector<TimelineEntry> frames;
};

inline IDirect3DDevice9* device=nullptr;
inline IDirect3DTexture9* texture=nullptr;
inline std::vector<unsigned char> pixels,defaultRows;
inline std::vector<ImU32> original,edited;
inline std::vector<int> sizes;
// Explicit pane-local simulation; never passed to persistence or live writes.
inline std::vector<PaletteData::Transformation> simulatedRules;
inline std::set<std::string> simulatedActive;
inline int simulatedHypeLevel=-1;
inline std::map<int,int> uploadedRedirects;
inline std::map<int,int> SimulatedRedirects() {
    std::map<int,int> result;
    for(const auto& rule:simulatedRules)if(rule.enabled && rule.shadeMask==255 && (rule.hypeLevel>=0?rule.hypeLevel==simulatedHypeLevel:simulatedActive.count(rule.id)>0))result.insert(rule.links.begin(),rule.links.end());
    return result;
}
inline unsigned width=0,height=0,layers=0;
inline unsigned canvasLeft=0,canvasTop=0,canvasWidth=0,canvasHeight=0;
inline int slot=-1,palette=-1;
inline float zoom=0.7f;
inline bool showOriginal=false,highlight=true,attempted=false;
inline bool legacyFixedOnly=false;
inline bool centerCanvas=true,fitCanvas=false;
inline std::string error;
inline std::string loadedCharacter,snapshotCharacter,preparationStatus;
inline std::filesystem::path preparationPython;
inline HANDLE preparationProcess=nullptr,preparationPipe=nullptr;
inline std::vector<ImU32> uploaded;
inline std::set<int> uploadedBands;
inline bool uploadedHighlight=false;
inline unsigned uploadedSprite=0,fallbackSprite=2;
inline std::vector<SpriteFrameAsset> frames;
inline std::vector<AnimationAsset> animations;
inline SpriteFrameAsset decodedFrame;
inline unsigned decodedSprite=(std::numeric_limits<unsigned>::max)();
inline int selectedAnimation=-1,frameIndex=0;
inline int exportStart=1,exportEnd=1;
inline int rangeHandle=0;
inline char animationSearch[128]={};

inline void LostDevice() {if(texture){texture->Release();texture=nullptr;}}
inline void ForgetMatch() {
    centerCanvas=true;LostDevice();simulatedRules.clear();simulatedActive.clear();simulatedHypeLevel=-1;uploadedRedirects.clear();original.clear();edited.clear();sizes.clear();slot=palette=-1;snapshotCharacter.clear();
}
inline void ResetAsset() {
    centerCanvas=true;LostDevice();pixels.clear();defaultRows.clear();frames.clear();animations.clear();decodedFrame={};
    decodedSprite=(std::numeric_limits<unsigned>::max)();attempted=false;legacyFixedOnly=false;
    error.clear();loadedCharacter.clear();fallbackSprite=2;selectedAnimation=-1;frameIndex=0;exportStart=exportEnd=1;
    canvasLeft=canvasTop=canvasWidth=canvasHeight=0;
}
inline void CancelAnimationAndWait();
inline void Shutdown() {
    CancelAnimationAndWait();
    ForgetMatch();device=nullptr;ResetAsset();
    if(preparationPipe){CloseHandle(preparationPipe);preparationPipe=nullptr;}
    if(preparationProcess){CloseHandle(preparationProcess);preparationProcess=nullptr;}
}

class PreviewInput : public std::ifstream {
public:
    using std::ifstream::ifstream;
    unsigned long long limit=4ull*1024*1024*1024;
    PreviewInput& read(char* data,std::streamsize count) {
        const auto position=tellg();
        if(position<0 || count<0 || static_cast<unsigned long long>(position)>limit ||
           static_cast<unsigned long long>(count)>limit-static_cast<unsigned long long>(position))
            throw std::runtime_error("Read exceeds the preview character section.");
        std::ifstream::read(data,count);return *this;
    }
};
inline bool ReadExactly(PreviewInput& f,char* data,size_t size) {
    f.read(data,(std::streamsize)size);return f.good();
}
inline unsigned BoundDimension(unsigned value) {
    if(value==0 || value>2048)throw std::runtime_error("Unsupported variable sprite dimension.");
    return value;
}
inline void RebuildCanvasBounds() {
    unsigned left=0,right=0,top=0,bottom=0;
    for(const auto& frame:frames) {
        left=(std::max)(left,static_cast<unsigned>((std::max)(frame.originX,0)));
        right=(std::max)(right,static_cast<unsigned>((std::max)(static_cast<int>(frame.width)-frame.originX,0)));
        top=(std::max)(top,static_cast<unsigned>((std::max)(frame.originY,0)));
        bottom=(std::max)(bottom,static_cast<unsigned>((std::max)(static_cast<int>(frame.height)-frame.originY,0)));
    }
    if(left+right>8192 || top+bottom>8192)throw std::runtime_error("Sprite origins exceed the preview canvas bound.");
    canvasLeft=left;canvasTop=top;canvasWidth=left+right;canvasHeight=top+bottom;
}
inline void Load(const char* character) {
    if(loadedCharacter!=character){ResetAsset();loadedCharacter=character;}
    if(attempted)return;attempted=true;
    try {
        PreviewInput f("sprite-preview.bin",std::ios::binary|std::ios::ate);
        bool legacyFile=false;
        if(!f && _stricmp(character,"Cerebella")==0) {
            f.clear();f.open("cerebella-pose.bin",std::ios::binary|std::ios::ate);legacyFile=bool(f);
        }
        if(!f)throw std::runtime_error("If you don't already have the required sprite preview files, select your local character-foits.gfs file in the data01 folder in the skug directory -> Use Prepare combined sprite asset to generate sprite-preview.bin from the local game archives.");
        auto fileLength=f.tellg();
        if(fileLength<8 || static_cast<unsigned long long>(fileLength)>4ull*1024*1024*1024)throw std::runtime_error("Unsupported combined preview asset size.");
        f.limit=static_cast<unsigned long long>(fileLength);
        f.seekg(0);char magic[8]={};
        if(!ReadExactly(f,magic,8))throw std::runtime_error("Truncated preview asset.");
        std::streamoff sectionOffset=0;unsigned long long sectionLength=static_cast<unsigned long long>(fileLength);
        if(!legacyFile && memcmp(magic,"SGMULT1\0",8)==0) {
            unsigned version=0,characterCount=0;
            f.read(reinterpret_cast<char*>(&version),4);f.read(reinterpret_cast<char*>(&characterCount),4);
            if(!f || version!=1 || characterCount<1 || characterCount>18)throw std::runtime_error("Malformed combined preview index.");
            struct Section {std::string name;unsigned long long offset,size;};
            std::vector<Section> sections;sections.reserve(characterCount);
            std::set<std::string> unique;
            for(unsigned i=0;i<characterCount;++i) {
                unsigned short nameBytes=0;f.read(reinterpret_cast<char*>(&nameBytes),2);
                if(!f || !nameBytes || nameBytes>64)throw std::runtime_error("Malformed combined preview character name.");
                std::string name(nameBytes,'\0');if(!ReadExactly(f,name.data(),nameBytes))throw std::runtime_error("Truncated combined preview character name.");
                unsigned long long offset=0,size=0;f.read(reinterpret_cast<char*>(&offset),8);f.read(reinterpret_cast<char*>(&size),8);
                if(!f || !unique.insert(name).second || size<24 || size>128ull*1024*1024 ||
                    offset>static_cast<unsigned long long>(fileLength) || size>static_cast<unsigned long long>(fileLength)-offset)
                    throw std::runtime_error("Invalid combined preview section bounds.");
                sections.push_back({name,offset,size});
            }
            const auto indexEnd=static_cast<unsigned long long>(f.tellg());
            std::vector<Section> ordered=sections;
            std::sort(ordered.begin(),ordered.end(),[](const Section& a,const Section& b){return a.offset<b.offset;});
            unsigned long long end=indexEnd;
            for(const auto& section:ordered) {
                if(section.offset!=end)throw std::runtime_error("Invalid combined preview section layout.");
                end=section.offset+section.size;
            }
            if(end!=static_cast<unsigned long long>(fileLength))throw std::runtime_error("Truncated or trailing combined preview data.");
            auto selected=std::find_if(sections.begin(),sections.end(),[character](const Section& section){return _stricmp(section.name.c_str(),character)==0;});
            if(selected==sections.end())throw std::runtime_error("No local preview section exists for this character.");
            sectionOffset=static_cast<std::streamoff>(selected->offset);sectionLength=selected->size;
            f.limit=selected->offset+selected->size;
            f.seekg(sectionOffset);if(!ReadExactly(f,magic,8))throw std::runtime_error("Truncated character preview section.");
        } else if(!legacyFile) {
            if(_stricmp(character,"Cerebella")!=0)throw std::runtime_error("Unsupported combined preview asset format.");
            const bool knownPose=memcmp(magic,"SGPOSE1\0",8)==0 || memcmp(magic,"SGPOSE2\0",8)==0 ||
                memcmp(magic,"SGPOSE3\0",8)==0 || memcmp(magic,"SGPOSE4\0",8)==0 || memcmp(magic,"SGPOSE5\0",8)==0 || memcmp(magic,"SGPOSE6\0",8)==0;
            if(!knownPose)throw std::runtime_error("Unsupported preview asset format.");
        }
        if(sectionLength<24 || sectionLength>128ull*1024*1024)throw std::runtime_error("Unsupported character preview section size.");
        unsigned paletteBytes=0,fallback=0,frameCount=0,animationCount=0;
        f.read(reinterpret_cast<char*>(&paletteBytes),4);f.read(reinterpret_cast<char*>(&fallback),4);f.read(reinterpret_cast<char*>(&frameCount),4);f.read(reinterpret_cast<char*>(&animationCount),4);
        fallbackSprite=fallback;
        if(memcmp(magic,"SGPOSE2\0",8)==0 || memcmp(magic,"SGPOSE3\0",8)==0 || memcmp(magic,"SGPOSE4\0",8)==0 || memcmp(magic,"SGPOSE5\0",8)==0)throw std::runtime_error("This preview asset has outdated timeline indexing/timing. Use Prepare combined sprite asset to regenerate it.");
        if(memcmp(magic,"SGPOSE6\0",8)==0) {
            if(paletteBytes<64 || paletteBytes>16384 || paletteBytes%64 || frameCount<1 || frameCount>4096 || animationCount>1000)throw std::runtime_error("Malformed SGPOSE6 preview asset.");
            if(sectionLength<24+paletteBytes)throw std::runtime_error("Truncated SGPOSE6 palette.");
            defaultRows.resize(paletteBytes);if(!ReadExactly(f,reinterpret_cast<char*>(defaultRows.data()),paletteBytes))throw std::runtime_error("Truncated SGPOSE6 palette.");
            bool fallbackFound=false;
            for(unsigned r=0;r<frameCount;++r) {
                unsigned id=0,layerCount=0;int originX=0,originY=0;
                f.read(reinterpret_cast<char*>(&id),4);f.read(reinterpret_cast<char*>(&originX),4);f.read(reinterpret_cast<char*>(&originY),4);f.read(reinterpret_cast<char*>(&layerCount),4);
                if(!f || layerCount<1 || layerCount>8)throw std::runtime_error("Malformed SGPOSE6 sprite record.");
                SpriteFrameAsset frame;frame.id=id;
                SpritePreviewGeometry::Bounds bounds;
                for(unsigned l=0;l<layerCount;++l) {
                    unsigned slot=0,used=0;unsigned short columns=0,rows=0,flags=0;int x=0,y=0;unsigned channelBytes[3]={};
                    f.read(reinterpret_cast<char*>(&slot),4);f.read(reinterpret_cast<char*>(&x),4);f.read(reinterpret_cast<char*>(&y),4);f.read(reinterpret_cast<char*>(&columns),2);f.read(reinterpret_cast<char*>(&rows),2);f.read(reinterpret_cast<char*>(&flags),2);f.read(reinterpret_cast<char*>(&used),2);f.read(reinterpret_cast<char*>(&channelBytes[0]),4);f.read(reinterpret_cast<char*>(&channelBytes[1]),4);f.read(reinterpret_cast<char*>(&channelBytes[2]),4);
                    unsigned layerWidth=BoundDimension(columns*16),layerHeight=BoundDimension(rows*16);
                    if(slot>7 || used==0 || used>columns*rows || (flags&~64u) || size_t(layerWidth)*layerHeight*3>16*1024*1024)throw std::runtime_error("Malformed or oversized SGPOSE6 sprite layer.");
                    long long relativeX=static_cast<long long>(x)-originX,relativeY=static_cast<long long>(y)-originY;
                    if(relativeX<-8192 || relativeX>8192 || relativeY<-8192 || relativeY>8192)throw std::runtime_error("Sprite layer origin offset exceeds bounds.");
                    PixelLayer layer;layer.slot=slot;layer.x=static_cast<int>(relativeX);layer.y=static_cast<int>(relativeY);layer.width=layerWidth;layer.height=layerHeight;layer.flags=flags;layer.tiles.resize(used);
                    if(!ReadExactly(f,reinterpret_cast<char*>(layer.tiles.data()),used*sizeof(unsigned short)))throw std::runtime_error("Truncated SGPOSE6 sprite tile list.");
                    std::set<unsigned short> uniqueTiles;
                    for(auto tile:layer.tiles)if(tile>=columns*rows || !uniqueTiles.insert(tile).second)throw std::runtime_error("Invalid SGPOSE6 tile position.");
                    for(unsigned channel=0;channel<3;++channel) {
                        if(channelBytes[channel]==0 || channelBytes[channel]>16*1024*1024 || static_cast<unsigned long long>(f.tellg())>f.limit || channelBytes[channel]>f.limit-static_cast<unsigned long long>(f.tellg()))throw std::runtime_error("Malformed SGPOSE6 channel length.");
                        layer.channels[channel].resize(channelBytes[channel]);
                        if(!ReadExactly(f,reinterpret_cast<char*>(layer.channels[channel].data()),channelBytes[channel]))throw std::runtime_error("Truncated SGPOSE6 sprite channel.");
                    }
                    bounds.Add(layer.x,layer.y,layerWidth,layerHeight);
                    frame.layers.push_back(std::move(layer));
                }
                frame.width=BoundDimension((unsigned)(bounds.right-bounds.left));frame.height=BoundDimension((unsigned)(bounds.bottom-bounds.top));
                frame.originX=-bounds.left;frame.originY=-bounds.top;
                for(auto& layer:frame.layers){layer.offsetX=bounds.OffsetX(layer.x);layer.offsetY=bounds.OffsetY(layer.y);}
                frames.push_back(std::move(frame));if(id==fallback)fallbackFound=true;
            }
            if(!fallbackFound)throw std::runtime_error("SGPOSE6 is missing the fixed-pose fallback sprite.");
            unsigned long long totalTimelineFrames=0;
            for(unsigned a=0;a<animationCount;++a) {
                unsigned id=0,nameBytes=0,frameCount=0;f.read(reinterpret_cast<char*>(&id),4);f.read(reinterpret_cast<char*>(&nameBytes),4);
                if(nameBytes<1 || nameBytes>1024)throw std::runtime_error("Malformed SGPOSE6 animation name.");
                std::string name(nameBytes,'\0');if(!ReadExactly(f,name.data(),nameBytes))throw std::runtime_error("Truncated SGPOSE6 animation name.");
                f.read(reinterpret_cast<char*>(&frameCount),4);if(!f || frameCount<1 || frameCount>65535 || totalTimelineFrames+frameCount>100000)throw std::runtime_error("Malformed or oversized SGPOSE6 timeline.");
                totalTimelineFrames+=frameCount;
                AnimationAsset animation;animation.id=id;animation.name=name;
                for(unsigned t=0;t<frameCount;++t) {
                    unsigned short sprite=0,sprite1=0,sprite2=0,duration=0;
                    f.read(reinterpret_cast<char*>(&sprite),2);f.read(reinterpret_cast<char*>(&sprite1),2);f.read(reinterpret_cast<char*>(&sprite2),2);f.read(reinterpret_cast<char*>(&duration),2);
                    if(!f || duration<1 || duration>65535)throw std::runtime_error("Malformed SGPOSE6 timeline frame.");
                    bool known=false;for(const auto& frame:frames)if(frame.id==sprite){known=true;break;}if(!known)throw std::runtime_error("Animation timeline references an invalid sprite record.");
                    animation.frames.push_back({sprite,duration,sprite1,sprite2});
                }
                animations.push_back(std::move(animation));
            }
            if(static_cast<unsigned long long>(f.tellg())!=static_cast<unsigned long long>(sectionOffset)+sectionLength)throw std::runtime_error("Trailing or malformed SGPOSE6 preview data.");
        } else if(memcmp(magic,"SGPOSE1\0",8)==0) {
            legacyFixedOnly=true;
            unsigned count=paletteBytes;
            f.seekg(0);
            char oldMagic[8]={};unsigned w=0,h=0,oldLayers=0,oldCount=0;
            f.read(oldMagic,8);f.read(reinterpret_cast<char*>(&w),4);f.read(reinterpret_cast<char*>(&h),4);f.read(reinterpret_cast<char*>(&oldLayers),4);f.read(reinterpret_cast<char*>(&oldCount),4);
            if(w!=448||h!=384||oldLayers!=2||oldCount<64||oldCount>16384||oldCount%64||static_cast<size_t>(sectionLength)!=24+oldCount+size_t(w)*h*3*oldLayers)throw std::runtime_error("Unsupported fixed-pose asset.");
            defaultRows.resize(oldCount);pixels.resize(size_t(w)*h*3*oldLayers);
            f.read(reinterpret_cast<char*>(defaultRows.data()),oldCount);f.read(reinterpret_cast<char*>(pixels.data()),pixels.size());
            width=w;height=h;layers=oldLayers;fallbackSprite=2;
            SpriteFrameAsset fixed;fixed.id=2;fixed.width=w;fixed.height=h;
            for(unsigned l=0;l<oldLayers;++l){PixelLayer layer;layer.slot=l;layer.width=w;layer.height=h;layer.pixels.resize(size_t(w)*h*3);std::copy(pixels.begin()+l*w*h*3,pixels.begin()+((l+1)*w*h*3),layer.pixels.begin());fixed.layers.push_back(std::move(layer));}
            frames.push_back(std::move(fixed));
        } else throw std::runtime_error("Unsupported preview asset magic.");
        RebuildCanvasBounds();
    }catch(const std::exception& e){error=e.what();pixels.clear();frames.clear();animations.clear();}
}

inline ImU32 Sample(int band,int shade,const std::vector<ImU32>& colors,const std::vector<int>& layout) {
    if(band<0 || band>=layout.size() || !layout[band])return 0;
    int start=0;for(int b=0;b<band;++b)start+=layout[b];
    const float index=(layout[band]-1)*(1.0f-(shade&31)/31.0f);
    int low=static_cast<int>(index),high=(std::min)(low+1,layout[band]-1);
    if(start+high>=colors.size())return 0;
    float t=index-low,black=(shade>>5)/7.0f;ImU32 result=0;
    for(int ch=0;ch<4;++ch){float a=(colors[start+low]>>(8*ch))&255,b=(colors[start+high]>>(8*ch))&255;float v=a+(b-a)*t;v=ch==3?(std::max)(v,black*255):v*(1-black);result|=static_cast<ImU32>(v)<<(8*ch);}
    return result;
}
inline ImU32 Sample(int band,int shade,const std::vector<ImU32>& colors){return Sample(band,shade,colors,sizes);}
inline std::vector<unsigned char> DecodeArtChannel(const std::vector<unsigned char>& input,unsigned expected) {
    if(expected>16*1024*1024 || input.empty())throw std::runtime_error("Sprite channel exceeds decode bounds.");
    std::vector<unsigned char> output;output.reserve(expected);size_t pos=0;
    while(pos<input.size()) {
        unsigned flag=input[pos++],code=flag&127,length=code<125?code+1:0;
        if(code>=125) {
            unsigned bytes=code-124;
            if(bytes>input.size()-pos)throw std::runtime_error("Truncated sprite RLE length.");
            for(unsigned i=0;i<bytes;++i)length=(length<<8)|input[pos++];
            if(!length)length=1;
        }
        if(length>expected-output.size())throw std::runtime_error("Oversized sprite RLE run.");
        if(flag&128) {
            if(pos>=input.size())throw std::runtime_error("Truncated repeated sprite RLE run.");
            output.insert(output.end(),length,input[pos++]);
        } else {
            if(length>input.size()-pos)throw std::runtime_error("Truncated literal sprite RLE run.");
            output.insert(output.end(),input.begin()+pos,input.begin()+pos+length);pos+=length;
        }
    }
    if(output.size()!=expected)throw std::runtime_error("Decoded sprite channel size mismatch.");
    return output;
}
inline void DecodePixelLayer(PixelLayer& layer) {
    const unsigned stride=(unsigned)layer.tiles.size()*16,expected=stride*16;
    if(!stride || expected>16*1024*1024)throw std::runtime_error("Sprite tile data exceeds decode bounds.");
    std::array<std::vector<unsigned char>,3> channels;
    for(unsigned ch=0;ch<3;++ch)channels[ch]=DecodeArtChannel(layer.channels[ch],expected);
    for(unsigned y=2;y<16;y+=2)for(unsigned x=0;x<stride;++x)channels[0][y*stride+x]=(channels[0][y*stride+x]+channels[0][(y-2)*stride+x])&255;
    for(unsigned x=0;x<stride;++x)channels[0][15*stride+x]=(channels[0][15*stride+x]+channels[0][14*stride+x])&255;
    for(unsigned y=1;y<15;y+=2)for(unsigned x=0;x<stride;++x) {
        unsigned a=channels[0][(y-1)*stride+x],b=channels[0][(y+1)*stride+x];
        channels[0][y*stride+x]=(((a>>4)+(b>>4))/2*16+((a&15)+(b&15))/2);
        if(layer.flags&64)channels[1][y*stride+x]=(channels[1][y*stride+x]&224)|(((channels[1][(y-1)*stride+x]&31)+(channels[1][(y+1)*stride+x]&31))/2);
    }
    layer.pixels.assign(size_t(layer.width)*layer.height*3,0);
    for(size_t n=0;n<layer.tiles.size();++n) {
        unsigned tile=layer.tiles[n],left=(tile%(layer.width/16))*16,top=(tile/(layer.width/16))*16;
        for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x) {
            size_t source=y*stride+n*16+x;
            size_t target=((top+y)*layer.width+left+x)*3;
            for(unsigned ch=0;ch<3;++ch)layer.pixels[target+ch]=channels[ch][source];
        }
    }
}
inline const SpriteFrameAsset* FrameMetadata() {
    const SpriteFrameAsset* source=nullptr;
    if(selectedAnimation>=0 && selectedAnimation<(int)animations.size()) {
        auto& animation=animations[selectedAnimation];
        if(frameIndex>=0 && frameIndex<(int)animation.frames.size()) {
            unsigned id=animation.frames[frameIndex].sprite;
            for(const auto& frame:frames)if(frame.id==id){source=&frame;break;}
        }
    }
    if(!source){for(const auto& frame:frames)if(frame.id==fallbackSprite){source=&frame;break;}}
    if(!source && !frames.empty())source=&frames.front();
    return source;
}
inline const SpriteFrameAsset* ActiveFrame() {
    const auto* source=FrameMetadata();
    if(!source)return nullptr;
    if(decodedSprite!=source->id) {
        decodedFrame=*source;
        for(auto& layer:decodedFrame.layers)if(!layer.channels[0].empty()) {
            DecodePixelLayer(layer);
            for(auto& channel:layer.channels){channel.clear();channel.shrink_to_fit();}
            layer.tiles.clear();layer.tiles.shrink_to_fit();
        }
        decodedSprite=source->id;
    }
    return &decodedFrame;
}
inline std::vector<ImU32> RenderFrame(const SpriteFrameAsset& frame,const std::vector<ImU32>& colors,const std::set<int>& selected,bool focus,bool transparent,const std::vector<int>& layout=sizes,const std::map<int,int>& redirects={}) {
    const unsigned outputWidth=BoundDimension(frame.width),outputHeight=BoundDimension(frame.height);
    std::vector<ImU32> lookup(256*256),output(size_t(outputWidth)*outputHeight);
    for(int b=0;b<(int)layout.size();++b)for(int shade=0;shade<256;++shade)lookup[b*256+shade]=Sample(redirects.count(b)?redirects.at(b):b,shade,colors,layout);
    for(unsigned y=0;y<outputHeight;++y)for(unsigned x=0;x<outputWidth;++x) {
        SpritePreviewPixel pixel(transparent);
        for(const auto& layer:frame.layers) {
            if(x<layer.offsetX || y<layer.offsetY || x>=layer.offsetX+layer.width || y>=layer.offsetY+layer.height)continue;
            const unsigned lx=x-layer.offsetX,ly=y-layer.offsetY;
            const auto offset=(size_t(ly)*layer.width+lx)*3;
            if(offset+2>=layer.pixels.size())continue;
            const int band=layer.pixels[offset+2],shade=layer.pixels[offset+1];
            pixel.Blend(lookup[band*256+(shade&255)],selected.count(band)>0);
        }
        output[size_t(y)*outputWidth+x]=transparent?pixel.PackTransparent():pixel.Pack(focus,!selected.empty());
    }
    return output;
}
struct AnimationExport {
    AnimationAsset animation;
    std::map<unsigned,SpriteFrameAsset> sprites;
    std::vector<ImU32> colors;
    std::vector<int> layout;
    SpritePreviewGeometry::Bounds bounds;
    std::map<int,int> redirects;
    bool gif=false;unsigned plays=0;
    std::string name;
};
struct AnimationProgress {std::atomic<size_t> frame{0};size_t total=0;std::atomic<bool> cancel{false};};
inline std::optional<AnimationExport> pendingAnimationExport;
inline std::shared_ptr<AnimationProgress> animationProgress;
inline std::future<std::string> animationWorker;
inline std::string animationStatus;
inline int animationPlays=0;
inline bool AnimationBusy(){return animationWorker.valid();}
inline void DrawExportRange(int count) {
    exportStart=std::clamp(exportStart,1,count);exportEnd=std::clamp(exportEnd,exportStart,count);
    ImGui::TextUnformatted("Export frame range (inclusive)");
    const float width=ImGui::GetContentRegionAvail().x;
    ImGui::SetNextItemWidth((std::max)(60.0f,(width-110)*0.5f));
    if(ImGui::InputInt("Start",&exportStart,0))exportStart=std::clamp(exportStart,1,exportEnd);
    ImGui::SameLine();ImGui::SetNextItemWidth((std::max)(60.0f,(width-110)*0.5f));
    if(ImGui::InputInt("End",&exportEnd,0))exportEnd=std::clamp(exportEnd,exportStart,count);
    const ImVec2 pos=ImGui::GetCursorScreenPos();const float height=30.0f,pad=9.0f;
    ImGui::InvisibleButton("##Export range",ImVec2((std::max)(60.0f,width),height));
    const float left=pos.x+pad,right=pos.x+(std::max)(60.0f,width)-pad;
    auto x=[&](int frame){return left+(right-left)*float(frame-1)/float((std::max)(1,count-1));};
    if(ImGui::IsItemActivated()) {
        const float mouse=ImGui::GetIO().MousePos.x;
        const float a=fabsf(mouse-x(exportStart)),b=fabsf(mouse-x(exportEnd));
        rangeHandle=a==b?(mouse<x(exportStart)?0:1):(a<b?0:1);
    }
    if(ImGui::IsItemActive()) {
        int frame=1+int(std::round(std::clamp((ImGui::GetIO().MousePos.x-left)/(right-left),0.0f,1.0f)*(count-1)));
        if(rangeHandle==0)exportStart=std::clamp(frame,1,exportEnd);else exportEnd=std::clamp(frame,exportStart,count);
    }
    auto* draw=ImGui::GetWindowDrawList();const float y=pos.y+height*0.5f;
    draw->AddLine(ImVec2(left,y),ImVec2(right,y),ImGui::GetColorU32(ImGuiCol_FrameBg),5);
    draw->AddLine(ImVec2(x(exportStart),y),ImVec2(x(exportEnd),y),ImGui::GetColorU32(ImGuiCol_SliderGrabActive),5);
    for(int value:{exportStart,exportEnd})draw->AddCircleFilled(ImVec2(x(value),y),7,ImGui::GetColorU32(ImGuiCol_SliderGrab));
    if(ImGui::IsItemHovered())ImGui::SetTooltip("Drag either endpoint. Start and End are inclusive; original frame durations are preserved.");
    if(ImGui::Button("Whole animation")){exportStart=1;exportEnd=count;}
    unsigned ticks=0;for(int i=exportStart-1;i<exportEnd;++i)ticks+=animations[selectedAnimation].frames[i].duration;
    ImGui::SameLine();ImGui::Text("%d frames, %.2f s",exportEnd-exportStart+1,ticks/60.0f);
}

inline AnimationExport CaptureAnimationExport(bool gif,unsigned plays,int first=1,int last=0) {
    if(selectedAnimation<0 || selectedAnimation>=int(animations.size()) || original.empty())throw std::runtime_error("Select an animation first.");
    AnimationExport capture;capture.animation=animations[selectedAnimation];capture.gif=gif;capture.plays=plays;
    const int count=int(capture.animation.frames.size());
    if(last==0)last=count;
    if(first<1 || last<first || last>count)throw std::runtime_error("Choose a valid inclusive frame range.");
    capture.animation.frames=decltype(capture.animation.frames)(capture.animation.frames.begin()+first-1,capture.animation.frames.begin()+last);
    if(capture.animation.frames.empty() || capture.animation.frames.size()>2000)throw std::runtime_error("Animation export supports 1 to 2000 timeline frames.");
    capture.colors=showOriginal?original:edited;capture.layout=sizes;capture.redirects=showOriginal?std::map<int,int>{}:SimulatedRedirects();size_t bytes=0;
    for(const auto& entry:capture.animation.frames) {
        if(capture.sprites.count(entry.sprite))continue;
        auto found=std::find_if(frames.begin(),frames.end(),[&](const auto& f){return f.id==entry.sprite;});
        if(found==frames.end())throw std::runtime_error("Animation references an unavailable sprite.");
        for(const auto& layer:found->layers){bytes+=layer.pixels.size()+layer.tiles.size()*2;for(const auto& channel:layer.channels)bytes+=channel.size();}
        if(bytes>64ull*1024*1024)throw std::runtime_error("Animation source exceeds the 64 MiB export limit.");
        capture.bounds.Add(-found->originX,-found->originY,found->width,found->height);capture.sprites.emplace(found->id,*found);
    }
    capture.name=snapshotCharacter+"-animation-"+std::to_string(capture.animation.id)+"-frames-"+std::to_string(first)+"-"+std::to_string(last)+(showOriginal?"-original":"-edited")+(gif?".gif":".apng");
    return capture;
}
inline SpriteAnimation::Bytes EncodeAnimation(const AnimationExport& capture,const std::shared_ptr<AnimationProgress>& progress) {
    const unsigned w=unsigned(capture.bounds.right-capture.bounds.left),h=unsigned(capture.bounds.bottom-capture.bounds.top);
    const auto cancel=[&](){if(progress->cancel)throw std::runtime_error("Animation export cancelled.");};
    std::optional<SpriteFrameAsset> decoded;
    auto get=[&](size_t i) {
        cancel();progress->frame=i;const auto& entry=capture.animation.frames.at(i);
        if(!decoded || decoded->id!=entry.sprite) {
            decoded=capture.sprites.at(entry.sprite);size_t decodedBytes=0;
            for(auto& layer:decoded->layers) {
                decodedBytes+=size_t(layer.width)*layer.height*3;
                if(decodedBytes>64ull*1024*1024)throw std::runtime_error("Decoded animation frame exceeds 64 MiB.");
                if(!layer.channels[0].empty()){DecodePixelLayer(layer);for(auto& channel:layer.channels)channel.clear();layer.tiles.clear();}
            }
        }
        const auto pixels=RenderFrame(*decoded,capture.colors,{},false,true,capture.layout,capture.redirects);
        std::vector<uint32_t> canvas(size_t(w)*h,0);
        const unsigned x=capture.bounds.OffsetX(-decoded->originX),y=capture.bounds.OffsetY(-decoded->originY);
        for(unsigned row=0;row<decoded->height;++row)memcpy(canvas.data()+size_t(y+row)*w+x,pixels.data()+size_t(row)*decoded->width,decoded->width*4);
        return SpriteAnimation::Frame{std::move(canvas),entry.duration};
    };
    if(!capture.gif)return SpriteAnimation::APNG(w,h,capture.animation.frames.size(),capture.plays,get,cancel);
    std::vector<uint32_t> samples;for(int band=0;band<int(capture.layout.size());++band)for(int shade=0;shade<256;++shade)samples.push_back(Sample(band,shade,capture.colors,capture.layout));
    return SpriteAnimation::GIF(w,h,capture.animation.frames.size(),capture.plays,samples,get,cancel);
}
inline void StartAnimationExport(const std::filesystem::path& path) {
    if(AnimationBusy() || !pendingAnimationExport)throw std::runtime_error("No animation capture is ready, or an export is already running.");
    auto capture=std::move(*pendingAnimationExport);pendingAnimationExport.reset();
    const auto destination=std::filesystem::absolute(path);
    animationProgress=std::make_shared<AnimationProgress>();animationProgress->total=capture.animation.frames.size();
    animationStatus="Exporting animation in the background.";
    animationWorker=std::async(std::launch::async,[capture=std::move(capture),path=destination,progress=animationProgress]() {
        try {auto bytes=EncodeAnimation(capture,progress);if(progress->cancel)return std::string("Animation export cancelled.");
            PaletteData::AtomicWrite(path,bytes);progress->frame=progress->total;
            return std::string("Saved ")+(capture.gif?"GIF: ":"APNG: ")+PaletteData::PathText(path);
        }catch(const std::exception& e){return std::string("Animation export: ")+e.what();}
    });
}
inline void PollAnimationExport() {
    if(!AnimationBusy() || animationWorker.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
    try {animationStatus=animationWorker.get();}catch(const std::exception& e){animationStatus=std::string("Animation export failed: ")+e.what();}
    animationProgress.reset();
}
inline void CancelAnimationAndWait() {
    if(animationProgress)animationProgress->cancel=true;
    if(animationWorker.valid())try{animationWorker.get();}catch(...){}
    animationProgress.reset();pendingAnimationExport.reset();
}
struct PngExport {
    unsigned width=0,height=0;
    std::vector<ImU32> pixels;
    std::string name;
};
inline std::optional<PngExport> pendingPngExport;
inline std::string exportStatus;
inline void CapturePngExport() {
    const auto* frame=ActiveFrame();if(!frame || original.empty())throw std::runtime_error("Choose a preview frame first.");
    const auto& colors=showOriginal?original:edited;
    std::string name=snapshotCharacter;
    if(selectedAnimation>=0 && selectedAnimation<int(animations.size()))name+="-move-"+std::to_string(animations[selectedAnimation].id)+"-frame-"+std::to_string(frameIndex+1);
    else name+="-sprite-"+std::to_string(frame->id);
    name+=showOriginal?"-original.png":"-edited.png";
    pendingPngExport=PngExport{frame->width,frame->height,RenderFrame(*frame,colors,{},false,true,sizes,showOriginal?std::map<int,int>{}:SimulatedRedirects()),name};
}
inline void SavePngExport(const std::filesystem::path& path) {
    try {
        if(!pendingPngExport)throw std::runtime_error("No captured frame is available. Choose Export PNG again.");
        const auto& image=*pendingPngExport;SpritePng::Save(path,image.width,image.height,image.pixels);
        exportStatus="Saved transparent PNG: "+PaletteData::PathText(path);
    }catch(const std::exception& e){exportStatus=std::string("PNG export failed: ")+e.what();}
    pendingPngExport.reset();
}
inline void Upload(const std::set<int>& selected) {
    if(!device || original.empty())return;
    auto frame=ActiveFrame();if(!frame)return;
    const auto& colors=showOriginal?original:edited;
    const auto redirects=showOriginal?std::map<int,int>{}:SimulatedRedirects();
    if(texture && uploadedRedirects==redirects && uploaded==colors && uploadedBands==selected && uploadedHighlight==highlight && uploadedSprite==frame->id)return;
    // Finish CPU work before locking the device texture, so errors never leave it locked.
    const auto image=RenderFrame(*frame,colors,selected,highlight,false,sizes,redirects);
    if(texture && (width!=frame->width || height!=frame->height)){texture->Release();texture=nullptr;}
    if(!texture && FAILED(device->CreateTexture(frame->width,frame->height,1,D3DUSAGE_DYNAMIC,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&texture,nullptr)))throw std::runtime_error("Could not create preview texture.");
    D3DLOCKED_RECT lock{};if(FAILED(texture->LockRect(0,&lock,nullptr,D3DLOCK_DISCARD)))throw std::runtime_error("Could not upload preview texture.");
    width=frame->width;height=frame->height;
    for(unsigned y=0;y<height;++y)memcpy(static_cast<unsigned char*>(lock.pBits)+y*lock.Pitch,image.data()+size_t(y)*width,width*4);
    texture->UnlockRect(0);uploadedRedirects=redirects;uploaded=colors;uploadedBands=selected;uploadedHighlight=highlight;uploadedSprite=frame->id;
}

inline void DrawCanvas() {
    if(!texture)return;
    const auto* frame=ActiveFrame();if(!frame)return;
    ImGui::BeginChild("Preview canvas",ImVec2(0,(std::max)(180.0f,ImGui::GetContentRegionAvail().y)),true,ImGuiWindowFlags_HorizontalScrollbar);
    if(fitCanvas) {
        const auto available=ImGui::GetContentRegionAvail();
        zoom=std::clamp((std::min)((available.x-16)/frame->width,(available.y-16)/frame->height),0.25f,3.0f);
        fitCanvas=false;centerCanvas=true;
    }
    const ImVec2 canvasSize(canvasWidth*zoom,canvasHeight*zoom);
    const ImVec2 canvasPosition=ImGui::GetCursorScreenPos();
    const float imageX=canvasPosition.x+(static_cast<int>(canvasLeft)-frame->originX)*zoom;
    const float imageY=canvasPosition.y+(static_cast<int>(canvasTop)-frame->originY)*zoom;
    ImGui::GetWindowDrawList()->AddImage((ImTextureID)(intptr_t)texture,
        ImVec2(imageX,imageY),ImVec2(imageX+frame->width*zoom,imageY+frame->height*zoom));
    ImGui::Dummy(canvasSize);
    if(centerCanvas) {
        ImGui::SetScrollX((static_cast<int>(canvasLeft)-frame->originX+frame->width*0.5f)*zoom-ImGui::GetWindowWidth()*0.5f);
        ImGui::SetScrollY((static_cast<int>(canvasTop)-frame->originY+frame->height*0.5f)*zoom-ImGui::GetWindowHeight()*0.5f);
        centerCanvas=false;
    }
    ImGui::EndChild();
}

inline void Draw(int targetSlot,int targetPalette,const char* character,const std::vector<ImU32>& liveColors,const std::vector<int>& layout,const ColorGroup& group,const std::set<int>& bands,const std::vector<PaletteData::Transformation>& rules={}) {
    ImGui::TextWrapped("Isolated preview. Extra artwork layers and shader effects are not previewed.");
    if(!error.empty() || preparationProcess)ImGui::SetNextItemOpen(true);
    if(ImGui::CollapsingHeader("Prepare local sprite assets")) {
        ImGui::BeginDisabled(preparationProcess!=nullptr);
        if(ImGui::Button("Prepare combined sprite asset")) {
            IGFD::FileDialogConfig config;config.path=".";
            ImGuiFileDialog::Instance()->OpenDialog("PrepareSpritePreview","Select local characters-foits.gfs",".gfs",config);
        }
        ImGui::SameLine();
        if(ImGui::Button("Choose Python executable")) {
            IGFD::FileDialogConfig config;config.path=".";
            ImGuiFileDialog::Instance()->OpenDialog("ChoosePreviewPython","Select a working Python 3 executable",".exe",config);
        }
        ImGui::EndDisabled();
        if(!preparationPython.empty())ImGui::TextWrapped("Python: %s",preparationPython.filename().string().c_str());
        if(!preparationStatus.empty())ImGui::TextWrapped("%s",preparationStatus.c_str());
    }
    Load(character);if(!error.empty()){ImGui::TextWrapped("%s",error.c_str());return;}
    if(slot!=targetSlot || palette!=targetPalette || sizes!=layout || snapshotCharacter!=character) {
        ForgetMatch();slot=targetSlot;palette=targetPalette;sizes=layout;snapshotCharacter=character;original=edited=liveColors;simulatedRules=rules;
    }
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.4f,0.85f,1.0f,1.0f),"Animation / frame selection");
    if(legacyFixedOnly)ImGui::TextWrapped("This is a fixed-pose-only asset. Prepare the combined sprite asset from the local game archives to enable animation selection.");
    ImGui::TextDisabled("%zu animations loaded | select an animation to reveal its frame controls",animations.size());
    ImGui::InputTextWithHint("##AnimationSearch","Filter animation name or numeric move ID",animationSearch,sizeof(animationSearch));

    std::string query=animationSearch;std::transform(query.begin(),query.end(),query.begin(),::tolower);
    std::vector<int> filtered;
    for(int i=0;i<(int)animations.size();++i){
        auto name=animations[i].name;std::string lower=name;std::transform(lower.begin(),lower.end(),lower.begin(),::tolower);
        std::string id=std::to_string(animations[i].id);
        if(!query.empty()&&lower.find(query)==std::string::npos&&id.find(query)==std::string::npos)continue;
        filtered.push_back(i);
    }
    int comboIndex=-1;
    for(int i=0;i<(int)filtered.size();++i)if(filtered[i]==selectedAnimation)comboIndex=i;
    std::string selectedLabel="Choose animation";
    if(comboIndex>=0){const auto& a=animations[selectedAnimation];selectedLabel=a.name.empty()?("Move "+std::to_string(a.id)):a.name+" ["+std::to_string(a.id)+"]";}
    ImGui::SetNextItemWidth((std::max)(160.0f,ImGui::GetContentRegionAvail().x-185));
    if(ImGui::BeginCombo("##Animation dropdown",selectedLabel.c_str())){
        for(int i=0;i<(int)filtered.size();++i){
            int animationIndex=filtered[i];const auto& a=animations[animationIndex];
            std::string label=a.name.empty()?("Move "+std::to_string(a.id)):a.name+" ["+std::to_string(a.id)+"]";
            if(ImGui::Selectable(label.c_str(),selectedAnimation==animationIndex)){selectedAnimation=animationIndex;frameIndex=0;exportStart=1;exportEnd=int(animations[animationIndex].frames.size());centerCanvas=true;}
            if(selectedAnimation==animationIndex)ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if(ImGui::Button("Fixed pose fallback")){selectedAnimation=-1;frameIndex=0;exportStart=exportEnd=1;centerCanvas=true;}
    if(selectedAnimation>=0 && selectedAnimation<(int)animations.size()) {
        auto& animation=animations[selectedAnimation];
        frameIndex=std::clamp(frameIndex,0,(int)animation.frames.size()-1);
        if(ImGui::Button("Previous"))frameIndex=(std::max)(0,frameIndex-1);ImGui::SameLine();
        if(ImGui::Button("Next"))frameIndex=(std::min)((int)animation.frames.size()-1,frameIndex+1);
        std::string frameLabel="Frame "+std::to_string(frameIndex+1)+" / "+std::to_string(animation.frames.size());
        ImGui::SetNextItemWidth((std::max)(160.0f,ImGui::GetContentRegionAvail().x-80));
        if(ImGui::BeginCombo("Frame dropdown",frameLabel.c_str())){
            for(int i=0;i<(int)animation.frames.size();++i){
                auto& entry=animation.frames[i];
                std::string label="Frame "+std::to_string(i+1)+" - sprite "+std::to_string(entry.sprite)+" - "+std::to_string(entry.duration)+" ticks";
                if(ImGui::Selectable(label.c_str(),frameIndex==i))frameIndex=i;
                if(frameIndex==i)ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        int frameNumber=frameIndex+1;
        ImGui::SetNextItemWidth(100);if(ImGui::InputInt("Frame number",&frameNumber))frameIndex=std::clamp(frameNumber,1,(int)animation.frames.size())-1;
        frameIndex=std::clamp(frameIndex,0,(int)animation.frames.size()-1);
        auto& entry=animation.frames[frameIndex];
        ImGui::Text("Sprite record: %u | duration: %u ticks",entry.sprite,entry.duration);
        if(ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();ImGui::Text("Other timeline slots: %u, %u",entry.sprite1,entry.sprite2);
            if(const auto* frame=FrameMetadata())ImGui::Text("Origin anchor: %d, %d",frame->originX,frame->originY);
            ImGui::EndTooltip();
        }
    } else ImGui::TextDisabled("Fixed pose fallback: sprite record %u",fallbackSprite);
    ImGui::Separator();
    if(ImGui::Button("Snapshot match colors"))original=edited=liveColors;
    ImGui::SameLine();ImGui::Checkbox("Original / Edited",&showOriginal);
    ImGui::TextDisabled(showOriginal?"Original snapshot":"Edited locally (match unchanged)");
    if(ImGui::CollapsingHeader("Simulate transformation rules locally")) {
        ImGui::TextWrapped("Manual full-band color borrowing only. Check each rule independently. Pose selection does not activate tags. Native conditions, shade masks, static/native redirects and shader effects are not emulated.");
        if(ImGui::Button("Refresh rule snapshot")){simulatedRules=rules;simulatedActive.clear();simulatedHypeLevel=-1;}
        ImGui::BeginDisabled(showOriginal);
        const bool hasHype=std::any_of(simulatedRules.begin(),simulatedRules.end(),[](const auto& rule){return rule.hypeLevel>=0;});
        if(hasHype) {
            const std::string label=simulatedHypeLevel<0?"No Hype simulation":"Hype "+std::to_string(simulatedHypeLevel);
            if(ImGui::BeginCombo("Local Hype level",label.c_str())) {
                if(ImGui::Selectable("No Hype simulation",simulatedHypeLevel<0))simulatedHypeLevel=-1;
                for(int level=0;level<4;++level)if(ImGui::Selectable(("Hype "+std::to_string(level)).c_str(),simulatedHypeLevel==level))simulatedHypeLevel=level;
                ImGui::EndCombo();
            }
            ImGui::TextDisabled("Manual simulation only; no live Hype read or saved override.");
        }
        for(const auto& rule:simulatedRules) {
            const bool supported=rule.shadeMask==255 && rule.enabled;
            bool active=simulatedActive.count(rule.id)>0;
            ImGui::BeginDisabled(!supported);
            if(rule.hypeLevel>=0)ImGui::Text("%s (Hype %d)",rule.name.c_str(),rule.hypeLevel);
            else if(ImGui::Checkbox((rule.name+"##simulate-"+rule.id).c_str(),&active)){if(active)simulatedActive.insert(rule.id);else simulatedActive.erase(rule.id);}
            ImGui::EndDisabled();
            if(!supported)ImGui::TextDisabled(rule.enabled?"Unsupported: partial shader shade mask":"Disabled rule");
        }
        ImGui::EndDisabled();
    }
    ImGui::SliderFloat("Zoom",&zoom,0.25f,3.0f,"%.2fx");ImGui::Checkbox("Highlight selected group",&highlight);
    ImGui::BeginDisabled(showOriginal);
    for(int i=group.startIndex;i<group.startIndex+group.count && i<(int)edited.size();++i) {
        ImGui::PushID(i);auto color=ColorsTools::ImU32BGRAtoImVec4RGBA(showOriginal?original[i]:edited[i]);
        if(ImGui::ColorEdit4("##Local swatch",&color.x,ImGuiColorEditFlags_NoInputs))edited[i]=ColorsTools::ImVec4RGBAtoImU32BGRA(color);
        ImGui::PopID();if((i-group.startIndex+1)%6)ImGui::SameLine();
    }
    ImGui::NewLine();ImGui::EndDisabled();
    try {Upload(bands);}catch(const std::exception& e){LostDevice();ImGui::TextWrapped("%s",e.what());}
    if(ImGui::Button("Center current frame"))centerCanvas=true;
    ImGui::SameLine();if(ImGui::Button("Fit current frame"))fitCanvas=true;
    ImGui::SameLine();
    if(ImGui::Button("Export PNG")) {
        try {
            CapturePngExport();IGFD::FileDialogConfig config;config.path=".";config.fileName=pendingPngExport->name;
            config.flags=ImGuiFileDialogFlags_ConfirmOverwrite;
            ImGuiFileDialog::Instance()->OpenDialog("ExportSpritePNG","Export transparent sprite PNG",".png",config);
        }catch(const std::exception& e){exportStatus=std::string("PNG export failed: ")+e.what();}
    }
    if(ImGui::IsItemHovered())ImGui::SetTooltip("Save this frame at native resolution with Original/Edited colors.\nTransparent background; no zoom, highlighting or unsupported effects.");
    if(!exportStatus.empty())ImGui::TextWrapped("%s",exportStatus.c_str());
    ImGui::BeginDisabled(AnimationBusy() || selectedAnimation<0 || ImGuiFileDialog::Instance()->IsOpened());
    if(ImGui::Button("Export animation"))ImGui::OpenPopup("Animation export options");
    ImGui::EndDisabled();
    if(ImGui::BeginPopup("Animation export options")) {
        if(selectedAnimation<0 || selectedAnimation>=int(animations.size()) || animations[selectedAnimation].frames.empty()){ImGui::CloseCurrentPopup();ImGui::EndPopup();return;}
        DrawExportRange(int(animations[selectedAnimation].frames.size()));
        ImGui::SetNextItemWidth(100);ImGui::InputInt("Plays (0 = forever)",&animationPlays);animationPlays=std::clamp(animationPlays,0,1000);
        auto choose=[&](bool gif) {
            try {
                pendingAnimationExport=CaptureAnimationExport(gif,unsigned(animationPlays),exportStart,exportEnd);
                IGFD::FileDialogConfig config;config.path=".";config.fileName=pendingAnimationExport->name;config.flags=ImGuiFileDialogFlags_ConfirmOverwrite;
                ImGuiFileDialog::Instance()->OpenDialog("ExportSpriteAnimation",gif?"Export transparent GIF":"Export transparent APNG",gif?".gif":".apng",config);
            }catch(const std::exception& e){animationStatus=std::string("Animation export failed: ")+e.what();}
        };
        if(ImGui::Selectable("APNG - full color and partial transparency"))choose(false);
        if(ImGui::Selectable("GIF - 255 colors plus transparency"))choose(true);
        ImGui::TextWrapped("GIF uses a 50%% alpha cutoff and 10 ms timing steps. Some viewers slow very short frames. APNG retains full alpha and authored timing.");
        ImGui::EndPopup();
    }
    if(AnimationBusy() && animationProgress) {
        const auto done=animationProgress->frame.load();
        ImGui::ProgressBar(float(done)/float(animationProgress->total),ImVec2(-120,0));ImGui::SameLine();
        if(ImGui::Button("Cancel export"))animationProgress->cancel=true;
    }
    if(!animationStatus.empty())ImGui::TextWrapped("%s",animationStatus.c_str());
    DrawCanvas();
}
}
