#pragma once
#include <fstream>
#include <set>
#include <algorithm>
#include "Tools/Colors.hpp"

namespace SpriteViewer {
struct PixelLayer {
    unsigned slot=0,offsetX=0,offsetY=0,width=0,height=0;
    int x=0,y=0;
    std::vector<unsigned char> pixels;
};
struct SpriteFrameAsset {
    unsigned id=0,originX=0,originY=0,width=0,height=0;
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
inline int slot=-1,palette=-1;
inline float zoom=0.7f;
inline bool showOriginal=false,highlight=true,attempted=false;
inline std::string error;
inline std::vector<ImU32> uploaded;
inline std::set<int> uploadedBands;
inline bool uploadedHighlight=false;
inline unsigned uploadedSprite=0;
inline std::vector<SpriteFrameAsset> frames;
inline std::vector<AnimationAsset> animations;
inline int selectedAnimation=-1,frameIndex=0;
inline char animationSearch[128]={};

inline void LostDevice() {if(texture){texture->Release();texture=nullptr;}}
inline void ForgetMatch() {
    LostDevice();original.clear();edited.clear();sizes.clear();slot=palette=-1;
}
inline void Shutdown() {ForgetMatch();device=nullptr;pixels.clear();defaultRows.clear();frames.clear();animations.clear();attempted=false;}

inline bool ReadExactly(std::ifstream& f,char* data,size_t size) {
    f.read(data,(std::streamsize)size);return f.good();
}
inline unsigned BoundDimension(unsigned value) {
    if(value==0 || value>2048)throw std::runtime_error("Unsupported variable sprite dimension.");
    return value;
}
inline void Load() {
    if(attempted)return;attempted=true;
    try {
        std::ifstream f("cerebella-pose.bin",std::ios::binary|std::ios::ate);
        if(!f)throw std::runtime_error("Prepare cerebella-pose.bin from the local installation with scripts/prepare-dll-preview.py.");
        auto length=f.tellg();if(length<24 || length>128*1024*1024)throw std::runtime_error("Unsupported preview asset size.");
        f.seekg(0);char magic[8]={};unsigned paletteBytes=0,fallback=0,frameCount=0,animationCount=0;
        if(!ReadExactly(f,magic,8))throw std::runtime_error("Truncated preview asset.");
        f.read(reinterpret_cast<char*>(&paletteBytes),4);f.read(reinterpret_cast<char*>(&fallback),4);f.read(reinterpret_cast<char*>(&frameCount),4);f.read(reinterpret_cast<char*>(&animationCount),4);
        if(memcmp(magic,"SGPOSE2\0",8)==0) {
            if(paletteBytes<64 || paletteBytes>16384 || paletteBytes%64 || frameCount<1 || frameCount>4096 || animationCount>1000)throw std::runtime_error("Malformed SGPOSE2 preview asset.");
            if(static_cast<unsigned>(length)<24+paletteBytes)throw std::runtime_error("Truncated SGPOSE2 palette.");
            defaultRows.resize(paletteBytes);if(!ReadExactly(f,reinterpret_cast<char*>(defaultRows.data()),paletteBytes))throw std::runtime_error("Truncated SGPOSE2 palette.");
            bool fallbackFound=false;
            for(unsigned r=0;r<frameCount;++r) {
                unsigned id=0,layerCount=0;int originX=0,originY=0;
                f.read(reinterpret_cast<char*>(&id),4);f.read(reinterpret_cast<char*>(&originX),4);f.read(reinterpret_cast<char*>(&originY),4);f.read(reinterpret_cast<char*>(&layerCount),4);
                if(!f || layerCount<1 || layerCount>8)throw std::runtime_error("Malformed SGPOSE2 sprite record.");
                SpriteFrameAsset frame;frame.id=id;frame.originX=(unsigned)std::max(originX,0);frame.originY=(unsigned)std::max(originY,0);
                int minX=0,minY=0,maxX=0,maxY=0;bool haveBounds=false;
                for(unsigned l=0;l<layerCount;++l) {
                    unsigned slot=0,layerWidth=0,layerHeight=0,pixelBytes=0;int x=0,y=0;
                    f.read(reinterpret_cast<char*>(&slot),4);f.read(reinterpret_cast<char*>(&x),4);f.read(reinterpret_cast<char*>(&y),4);f.read(reinterpret_cast<char*>(&layerWidth),4);f.read(reinterpret_cast<char*>(&layerHeight),4);f.read(reinterpret_cast<char*>(&pixelBytes),4);
                    BoundDimension(layerWidth);BoundDimension(layerHeight);
                    if(slot>7 || pixelBytes!=layerWidth*layerHeight*3 || pixelBytes>16*1024*1024)throw std::runtime_error("Malformed or oversized SGPOSE2 sprite layer.");
                    PixelLayer layer;layer.slot=slot;layer.x=x;layer.y=y;layer.width=layerWidth;layer.height=layerHeight;layer.pixels.resize(pixelBytes);
                    if(!ReadExactly(f,reinterpret_cast<char*>(layer.pixels.data()),pixelBytes))throw std::runtime_error("Truncated SGPOSE2 sprite layer.");
                    if(!haveBounds){minX=x;minY=y;maxX=x+(int)layerWidth;maxY=y+(int)layerHeight;haveBounds=true;}else{
                        minX=(std::min)(minX,x);minY=(std::min)(minY,y);maxX=(std::max)(maxX,x+(int)layerWidth);maxY=(std::max)(maxY,y+(int)layerHeight);
                    }
                    frame.layers.push_back(std::move(layer));
                }
                frame.width=BoundDimension((unsigned)(maxX-minX));frame.height=BoundDimension((unsigned)(maxY-minY));
                for(auto& layer:frame.layers){layer.offsetX=(unsigned)(layer.x-minX);layer.offsetY=(unsigned)(layer.y-minY);}
                frames.push_back(std::move(frame));if(id==fallback)fallbackFound=true;
            }
            if(!fallbackFound)throw std::runtime_error("SGPOSE2 is missing the fixed-pose fallback sprite.");
            for(unsigned a=0;a<animationCount;++a) {
                unsigned id=0,nameBytes=0,frameCount=0;f.read(reinterpret_cast<char*>(&id),4);f.read(reinterpret_cast<char*>(&nameBytes),4);
                if(nameBytes<1 || nameBytes>1024)throw std::runtime_error("Malformed SGPOSE2 animation name.");
                std::string name(nameBytes,'\0');if(!ReadExactly(f,name.data(),nameBytes))throw std::runtime_error("Truncated SGPOSE2 animation name.");
                f.read(reinterpret_cast<char*>(&frameCount),4);if(!f || frameCount<1 || frameCount>65535)throw std::runtime_error("Malformed SGPOSE2 timeline.");
                AnimationAsset animation;animation.id=id;animation.name=name;
                for(unsigned t=0;t<frameCount;++t) {
                    unsigned short sprite=0,sprite1=0,sprite2=0,duration=0;
                    f.read(reinterpret_cast<char*>(&sprite),2);f.read(reinterpret_cast<char*>(&sprite1),2);f.read(reinterpret_cast<char*>(&sprite2),2);f.read(reinterpret_cast<char*>(&duration),2);
                    if(!f || duration<1 || duration>65535)throw std::runtime_error("Malformed SGPOSE2 timeline frame.");
                    bool known=false;for(const auto& frame:frames)if(frame.id==sprite){known=true;break;}if(!known)throw std::runtime_error("Animation timeline references an invalid sprite record.");
                    animation.frames.push_back({sprite,duration,sprite1,sprite2});
                }
                animations.push_back(std::move(animation));
            }
            if(f.tellg()!=length)throw std::runtime_error("Trailing or malformed SGPOSE2 preview data.");
        } else if(memcmp(magic,"SGPOSE1\0",8)==0) {
            unsigned count=paletteBytes;
            f.seekg(0);
            char oldMagic[8]={};unsigned w=0,h=0,oldLayers=0,oldCount=0;
            f.read(oldMagic,8);f.read(reinterpret_cast<char*>(&w),4);f.read(reinterpret_cast<char*>(&h),4);f.read(reinterpret_cast<char*>(&oldLayers),4);f.read(reinterpret_cast<char*>(&oldCount),4);
            if(w!=448||h!=384||oldLayers!=2||oldCount<64||oldCount>16384||oldCount%64||static_cast<size_t>(length)!=24+oldCount+size_t(w)*h*3*oldLayers)throw std::runtime_error("Unsupported fixed-pose asset.");
            defaultRows.resize(oldCount);pixels.resize(size_t(w)*h*3*oldLayers);
            f.read(reinterpret_cast<char*>(defaultRows.data()),oldCount);f.read(reinterpret_cast<char*>(pixels.data()),pixels.size());
            width=w;height=h;layers=oldLayers;
            SpriteFrameAsset fixed;fixed.id=2;fixed.width=w;fixed.height=h;
            for(unsigned l=0;l<oldLayers;++l){PixelLayer layer;layer.slot=l;layer.width=w;layer.height=h;layer.pixels.resize(size_t(w)*h*3);std::copy(pixels.begin()+l*w*h*3,pixels.begin()+((l+1)*w*h*3),layer.pixels.begin());fixed.layers.push_back(std::move(layer));}
            frames.push_back(std::move(fixed));
        } else throw std::runtime_error("Unsupported preview asset magic.");
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
inline const SpriteFrameAsset* ActiveFrame() {
    if(selectedAnimation>=0 && selectedAnimation<(int)animations.size()) {
        auto& animation=animations[selectedAnimation];
        if(frameIndex>=0 && frameIndex<(int)animation.frames.size()) {
            unsigned id=animation.frames[frameIndex].sprite;
            for(const auto& frame:frames)if(frame.id==id)return &frame;
        }
    }
    unsigned fallback=2;for(const auto& frame:frames)if(frame.id==fallback)return &frame;
    if(!frames.empty())return &frames.front();
    return nullptr;
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
            float rgb[3]={0.20f,0.20f,0.20f};
            for(const auto& layer:frame->layers) {
                if(x<layer.offsetX || y<layer.offsetY || x>=layer.offsetX+layer.width || y>=layer.offsetY+layer.height)continue;
                unsigned lx=x-layer.offsetX,ly=y-layer.offsetY;
                auto offset=(size_t(ly)*layer.width+lx)*3;
                if(offset+2>=layer.pixels.size())continue;
                int band=layer.pixels[offset+2],shade=layer.pixels[offset+1];
                if(band<0 || band>=256)continue;
                auto color=lookup[band*256+(shade&255)];float a=((color>>24)&255)/255.0f;
                for(int ch=0;ch<3;++ch){float v=((color>>(ch*8))&255)/255.0f;if(highlight&&!selected.empty())v=selected.count(band)?v*0.55f+(ch==2?0.0f:0.45f):v*0.25f;rgb[ch]=rgb[ch]*(1-a)+v*a;}
            }
            row[x]=0xff000000|static_cast<ImU32>(rgb[0]*255)|static_cast<ImU32>(rgb[1]*255)<<8|static_cast<ImU32>(rgb[2]*255)<<16;
        }
    }
    texture->UnlockRect(0);uploaded=colors;uploadedBands=selected;uploadedHighlight=highlight;uploadedSprite=frame->id;
}

inline void Draw(int targetSlot,int targetPalette,const char* character,const std::vector<ImU32>& liveColors,const std::vector<int>& layout,const ColorGroup& group,const std::set<int>& bands) {
    if(!ImGui::CollapsingHeader("Sprite preview",ImGuiTreeNodeFlags_DefaultOpen))return;
    ImGui::TextWrapped("Cerebella local sprite frames | isolated colors only. Overlays, native lighting, rainbow, redirects, transformations and super visuals are not previewed.");
    if(_stricmp(character,"Cerebella")!=0){ImGui::TextDisabled("Select Cerebella to use the local sprite preview.");return;}
    Load();if(!error.empty()){ImGui::TextWrapped("%s",error.c_str());return;}
    if(slot!=targetSlot || palette!=targetPalette || sizes!=layout) {
        ForgetMatch();slot=targetSlot;palette=targetPalette;sizes=layout;original=edited=liveColors;
    }
    ImGui::Separator();ImGui::TextUnformatted("Animation");
    ImGui::InputTextWithHint("##AnimationSearch","Search authored animation name or ID",animationSearch,sizeof(animationSearch));
    ImGui::BeginChild("Animation list",ImVec2(220,140),true);
    std::string query=animationSearch;std::transform(query.begin(),query.end(),query.begin(),::tolower);
    for(int i=0;i<(int)animations.size();++i){
        auto name=animations[i].name;std::string lower=name;std::transform(lower.begin(),lower.end(),lower.begin(),::tolower);
        std::string id=std::to_string(animations[i].id);
        if(!query.empty()&&lower.find(query)==std::string::npos&&id.find(query)==std::string::npos)continue;
        std::string label=name.empty()?("Move "+id):name+" ["+id+"]";
        if(ImGui::Selectable(label.c_str(),selectedAnimation==i)){selectedAnimation=i;frameIndex=0;}
    }
    ImGui::EndChild();ImGui::SameLine();
    ImGui::BeginGroup();
    if(ImGui::Button("Fixed pose fallback")){selectedAnimation=-1;frameIndex=0;}
    if(selectedAnimation>=0 && selectedAnimation<(int)animations.size()) {
        auto& animation=animations[selectedAnimation];
        ImGui::TextDisabled("%zu authored timeline frames",animation.frames.size());
        frameIndex=std::clamp(frameIndex,0,(int)animation.frames.size()-1);
        if(ImGui::Button("Previous"))frameIndex=(std::max)(0,frameIndex-1);ImGui::SameLine();
        if(ImGui::Button("Next"))frameIndex=(std::min)((int)animation.frames.size()-1,frameIndex+1);
        ImGui::SetNextItemWidth(100);ImGui::InputInt("Frame within animation",&frameIndex);
        frameIndex=std::clamp(frameIndex,0,(int)animation.frames.size()-1);
        ImGui::SliderInt("Frame", &frameIndex,0,(int)animation.frames.size()-1);
        ImGui::Text("Frame %d / %d",frameIndex+1,(int)animation.frames.size());
        auto& entry=animation.frames[frameIndex];
        ImGui::Text("Sprite record: %u | duration: %u ticks",entry.sprite,entry.duration);
        ImGui::TextDisabled("Alternate timeline slots: %u, %u",entry.sprite1,entry.sprite2);
    } else ImGui::TextDisabled("Fixed pose fallback: sprite record 2");
    ImGui::EndGroup();
    ImGui::Separator();
    if(ImGui::Button("Snapshot match colors"))original=edited=liveColors;
    ImGui::SameLine();ImGui::Checkbox("Original / Edited",&showOriginal);
    ImGui::TextDisabled(showOriginal?"Original snapshot":"Edited locally (match unchanged)");
    ImGui::SliderFloat("Zoom",&zoom,0.25f,3.0f,"%.2fx");ImGui::Checkbox("Highlight selected group",&highlight);
    ImGui::BeginDisabled(showOriginal);
    for(int i=group.startIndex;i<group.startIndex+group.count && i<(int)edited.size();++i) {
        ImGui::PushID(i);auto color=ColorsTools::ImU32BGRAtoImVec4RGBA(edited[i]);
        if(ImGui::ColorEdit4("##Local swatch",&color.x,ImGuiColorEditFlags_NoInputs))edited[i]=ColorsTools::ImVec4RGBAtoImU32BGRA(color);
        ImGui::PopID();if((i-group.startIndex+1)%6)ImGui::SameLine();
    }
    ImGui::NewLine();ImGui::EndDisabled();
    try {Upload(bands);}catch(const std::exception& e){LostDevice();ImGui::TextWrapped("%s",e.what());}
    if(texture){ImGui::BeginChild("Preview canvas",ImVec2(0,420),true,ImGuiWindowFlags_HorizontalScrollbar);ImGui::Image((ImTextureID)(intptr_t)texture,ImVec2(width*zoom,height*zoom));ImGui::EndChild();}
}
}
