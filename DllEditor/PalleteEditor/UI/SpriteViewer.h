#pragma once
#include <fstream>
#include <set>
#include <algorithm>
#include <limits>
#include "Tools/Colors.hpp"
#include "SpritePreviewGeometry.h"
#include "SpritePreviewPixel.h"

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
inline char animationSearch[128]={};

inline void LostDevice() {if(texture){texture->Release();texture=nullptr;}}
inline void ForgetMatch() {
    centerCanvas=true;LostDevice();original.clear();edited.clear();sizes.clear();slot=palette=-1;snapshotCharacter.clear();
}
inline void ResetAsset() {
    centerCanvas=true;LostDevice();pixels.clear();defaultRows.clear();frames.clear();animations.clear();decodedFrame={};
    decodedSprite=(std::numeric_limits<unsigned>::max)();attempted=false;legacyFixedOnly=false;
    error.clear();loadedCharacter.clear();fallbackSprite=2;selectedAnimation=-1;frameIndex=0;
    canvasLeft=canvasTop=canvasWidth=canvasHeight=0;
}
inline void Shutdown() {
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
                memcmp(magic,"SGPOSE3\0",8)==0 || memcmp(magic,"SGPOSE4\0",8)==0 || memcmp(magic,"SGPOSE5\0",8)==0;
            if(!knownPose)throw std::runtime_error("Unsupported preview asset format.");
        }
        if(sectionLength<24 || sectionLength>128ull*1024*1024)throw std::runtime_error("Unsupported character preview section size.");
        unsigned paletteBytes=0,fallback=0,frameCount=0,animationCount=0;
        f.read(reinterpret_cast<char*>(&paletteBytes),4);f.read(reinterpret_cast<char*>(&fallback),4);f.read(reinterpret_cast<char*>(&frameCount),4);f.read(reinterpret_cast<char*>(&animationCount),4);
        fallbackSprite=fallback;
        if(memcmp(magic,"SGPOSE2\0",8)==0 || memcmp(magic,"SGPOSE3\0",8)==0 || memcmp(magic,"SGPOSE4\0",8)==0)throw std::runtime_error("This preview asset uses an outdated sprite mapping or layer format. Regenerate it with the current scripts/prepare-dll-preview.py.");
        if(memcmp(magic,"SGPOSE5\0",8)==0) {
            if(paletteBytes<64 || paletteBytes>16384 || paletteBytes%64 || frameCount<1 || frameCount>4096 || animationCount>1000)throw std::runtime_error("Malformed SGPOSE5 preview asset.");
            if(sectionLength<24+paletteBytes)throw std::runtime_error("Truncated SGPOSE5 palette.");
            defaultRows.resize(paletteBytes);if(!ReadExactly(f,reinterpret_cast<char*>(defaultRows.data()),paletteBytes))throw std::runtime_error("Truncated SGPOSE5 palette.");
            bool fallbackFound=false;
            for(unsigned r=0;r<frameCount;++r) {
                unsigned id=0,layerCount=0;int originX=0,originY=0;
                f.read(reinterpret_cast<char*>(&id),4);f.read(reinterpret_cast<char*>(&originX),4);f.read(reinterpret_cast<char*>(&originY),4);f.read(reinterpret_cast<char*>(&layerCount),4);
                if(!f || layerCount<1 || layerCount>8)throw std::runtime_error("Malformed SGPOSE5 sprite record.");
                SpriteFrameAsset frame;frame.id=id;
                SpritePreviewGeometry::Bounds bounds;
                for(unsigned l=0;l<layerCount;++l) {
                    unsigned slot=0,used=0;unsigned short columns=0,rows=0,flags=0;int x=0,y=0;unsigned channelBytes[3]={};
                    f.read(reinterpret_cast<char*>(&slot),4);f.read(reinterpret_cast<char*>(&x),4);f.read(reinterpret_cast<char*>(&y),4);f.read(reinterpret_cast<char*>(&columns),2);f.read(reinterpret_cast<char*>(&rows),2);f.read(reinterpret_cast<char*>(&flags),2);f.read(reinterpret_cast<char*>(&used),2);f.read(reinterpret_cast<char*>(&channelBytes[0]),4);f.read(reinterpret_cast<char*>(&channelBytes[1]),4);f.read(reinterpret_cast<char*>(&channelBytes[2]),4);
                    unsigned layerWidth=BoundDimension(columns*16),layerHeight=BoundDimension(rows*16);
                    if(slot>7 || used==0 || used>columns*rows || (flags&~64u) || size_t(layerWidth)*layerHeight*3>16*1024*1024)throw std::runtime_error("Malformed or oversized SGPOSE5 sprite layer.");
                    long long relativeX=static_cast<long long>(x)-originX,relativeY=static_cast<long long>(y)-originY;
                    if(relativeX<-8192 || relativeX>8192 || relativeY<-8192 || relativeY>8192)throw std::runtime_error("Sprite layer origin offset exceeds bounds.");
                    PixelLayer layer;layer.slot=slot;layer.x=static_cast<int>(relativeX);layer.y=static_cast<int>(relativeY);layer.width=layerWidth;layer.height=layerHeight;layer.flags=flags;layer.tiles.resize(used);
                    if(!ReadExactly(f,reinterpret_cast<char*>(layer.tiles.data()),used*sizeof(unsigned short)))throw std::runtime_error("Truncated SGPOSE5 sprite tile list.");
                    std::set<unsigned short> uniqueTiles;
                    for(auto tile:layer.tiles)if(tile>=columns*rows || !uniqueTiles.insert(tile).second)throw std::runtime_error("Invalid SGPOSE5 tile position.");
                    for(unsigned channel=0;channel<3;++channel) {
                        if(channelBytes[channel]==0 || channelBytes[channel]>16*1024*1024 || static_cast<unsigned long long>(f.tellg())>f.limit || channelBytes[channel]>f.limit-static_cast<unsigned long long>(f.tellg()))throw std::runtime_error("Malformed SGPOSE5 channel length.");
                        layer.channels[channel].resize(channelBytes[channel]);
                        if(!ReadExactly(f,reinterpret_cast<char*>(layer.channels[channel].data()),channelBytes[channel]))throw std::runtime_error("Truncated SGPOSE5 sprite channel.");
                    }
                    bounds.Add(layer.x,layer.y,layerWidth,layerHeight);
                    frame.layers.push_back(std::move(layer));
                }
                frame.width=BoundDimension((unsigned)(bounds.right-bounds.left));frame.height=BoundDimension((unsigned)(bounds.bottom-bounds.top));
                frame.originX=-bounds.left;frame.originY=-bounds.top;
                for(auto& layer:frame.layers){layer.offsetX=bounds.OffsetX(layer.x);layer.offsetY=bounds.OffsetY(layer.y);}
                frames.push_back(std::move(frame));if(id==fallback)fallbackFound=true;
            }
            if(!fallbackFound)throw std::runtime_error("SGPOSE5 is missing the fixed-pose fallback sprite.");
            unsigned long long totalTimelineFrames=0;
            for(unsigned a=0;a<animationCount;++a) {
                unsigned id=0,nameBytes=0,frameCount=0;f.read(reinterpret_cast<char*>(&id),4);f.read(reinterpret_cast<char*>(&nameBytes),4);
                if(nameBytes<1 || nameBytes>1024)throw std::runtime_error("Malformed SGPOSE5 animation name.");
                std::string name(nameBytes,'\0');if(!ReadExactly(f,name.data(),nameBytes))throw std::runtime_error("Truncated SGPOSE5 animation name.");
                f.read(reinterpret_cast<char*>(&frameCount),4);if(!f || frameCount<1 || frameCount>65535 || totalTimelineFrames+frameCount>100000)throw std::runtime_error("Malformed or oversized SGPOSE5 timeline.");
                totalTimelineFrames+=frameCount;
                AnimationAsset animation;animation.id=id;animation.name=name;
                for(unsigned t=0;t<frameCount;++t) {
                    unsigned short sprite=0,sprite1=0,sprite2=0,duration=0;
                    f.read(reinterpret_cast<char*>(&sprite),2);f.read(reinterpret_cast<char*>(&sprite1),2);f.read(reinterpret_cast<char*>(&sprite2),2);f.read(reinterpret_cast<char*>(&duration),2);
                    if(!f || duration<1 || duration>65535)throw std::runtime_error("Malformed SGPOSE5 timeline frame.");
                    bool known=false;for(const auto& frame:frames)if(frame.id==sprite){known=true;break;}if(!known)throw std::runtime_error("Animation timeline references an invalid sprite record.");
                    animation.frames.push_back({sprite,duration,sprite1,sprite2});
                }
                animations.push_back(std::move(animation));
            }
            if(static_cast<unsigned long long>(f.tellg())!=static_cast<unsigned long long>(sectionOffset)+sectionLength)throw std::runtime_error("Trailing or malformed SGPOSE5 preview data.");
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

inline ImU32 Sample(int band,int shade,const std::vector<ImU32>& colors) {
    if(band<0 || band>=sizes.size() || !sizes[band])return 0;
    int start=0;for(int b=0;b<band;++b)start+=sizes[b];
    const float index=(sizes[band]-1)*(1.0f-(shade&31)/31.0f);
    int low=static_cast<int>(index),high=(std::min)(low+1,sizes[band]-1);
    if(start+high>=colors.size())return 0;
    float t=index-low,black=(shade>>5)/7.0f;ImU32 result=0;
    for(int ch=0;ch<4;++ch){float a=(colors[start+low]>>(8*ch))&255,b=(colors[start+high]>>(8*ch))&255;float v=a+(b-a)*t;v=ch==3?(std::max)(v,black*255):v*(1-black);result|=static_cast<ImU32>(v)<<(8*ch);}
    return result;
}
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
inline void Upload(const std::set<int>& selected) {
    if(!device || original.empty())return;
    auto frame=ActiveFrame();if(!frame)return;
    const auto& colors=showOriginal?original:edited;
    if(texture && uploaded==colors && uploadedBands==selected && uploadedHighlight==highlight && uploadedSprite==frame->id)return;
    if(texture && (width!=frame->width || height!=frame->height)){texture->Release();texture=nullptr;}
    if(!texture && FAILED(device->CreateTexture(frame->width,frame->height,1,D3DUSAGE_DYNAMIC,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&texture,nullptr)))throw std::runtime_error("Could not create preview texture.");
    D3DLOCKED_RECT lock{};if(FAILED(texture->LockRect(0,&lock,nullptr,D3DLOCK_DISCARD)))throw std::runtime_error("Could not upload preview texture.");
    width=frame->width;height=frame->height;
    std::vector<ImU32> lookup(256*256);
    for(int b=0;b<(int)sizes.size();++b)for(int s=0;s<256;++s)lookup[b*256+s]=Sample(b,s,colors);
    for(unsigned y=0;y<height;++y) {
        auto row=reinterpret_cast<ImU32*>(static_cast<unsigned char*>(lock.pBits)+y*lock.Pitch);
        for(unsigned x=0;x<width;++x) {
            SpritePreviewPixel pixel;
            for(const auto& layer:frame->layers) {
                if(x<layer.offsetX || y<layer.offsetY || x>=layer.offsetX+layer.width || y>=layer.offsetY+layer.height)continue;
                unsigned lx=x-layer.offsetX,ly=y-layer.offsetY;
                auto offset=(size_t(ly)*layer.width+lx)*3;
                if(offset+2>=layer.pixels.size())continue;
                int band=layer.pixels[offset+2],shade=layer.pixels[offset+1];
                if(band<0 || band>=256)continue;
                pixel.Blend(lookup[band*256+(shade&255)],selected.count(band)>0);
            }
            row[x]=pixel.Pack(highlight,!selected.empty());
        }
    }
    texture->UnlockRect(0);uploaded=colors;uploadedBands=selected;uploadedHighlight=highlight;uploadedSprite=frame->id;
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

inline void Draw(int targetSlot,int targetPalette,const char* character,const std::vector<ImU32>& liveColors,const std::vector<int>& layout,const ColorGroup& group,const std::set<int>& bands) {
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
        ForgetMatch();slot=targetSlot;palette=targetPalette;sizes=layout;snapshotCharacter=character;original=edited=liveColors;
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
            if(ImGui::Selectable(label.c_str(),selectedAnimation==animationIndex)){selectedAnimation=animationIndex;frameIndex=0;centerCanvas=true;}
            if(selectedAnimation==animationIndex)ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if(ImGui::Button("Fixed pose fallback")){selectedAnimation=-1;frameIndex=0;centerCanvas=true;}
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
    DrawCanvas();
}
}
