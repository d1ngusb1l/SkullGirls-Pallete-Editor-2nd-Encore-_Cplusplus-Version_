#pragma once
#include <fstream>
#include <set>
#include "Tools/Colors.hpp"

// Isolated CPU sprite evaluation followed by a D3D9 texture upload. This module
// has no game-memory, palette-file, shader substitution or live-apply API.
namespace SpriteViewer {
inline IDirect3DDevice9* device=nullptr;
inline IDirect3DTexture9* texture=nullptr;
inline std::vector<unsigned char> pixels, defaultRows;
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
inline void LostDevice() {if(texture){texture->Release();texture=nullptr;}}
inline void ForgetMatch() {
    LostDevice();original.clear();edited.clear();sizes.clear();slot=palette=-1;
}
inline void Shutdown() {ForgetMatch();device=nullptr;pixels.clear();defaultRows.clear();attempted=false;}
inline void Load() {
    if(attempted)return;attempted=true;
    try {
        std::ifstream f("cerebella-pose.bin",std::ios::binary|std::ios::ate);
        auto length=f.tellg();if(length<24 || length>16*1024*1024)throw std::runtime_error("Prepare cerebella-pose.bin from the local installation with scripts/prepare-dll-preview.py.");
        f.seekg(0);char magic[8];unsigned count=0;
        f.read(magic,8);f.read(reinterpret_cast<char*>(&width),4);f.read(reinterpret_cast<char*>(&height),4);
        f.read(reinterpret_cast<char*>(&layers),4);f.read(reinterpret_cast<char*>(&count),4);
        if(memcmp(magic,"SGPOSE1\0",8) || width!=448 || height!=384 || layers!=2 || count<64 || count>16384 || count%64 || static_cast<size_t>(length)!=24+count+size_t(width)*height*3*layers)throw std::runtime_error("Unsupported fixed-pose asset.");
        defaultRows.resize(count);pixels.resize(size_t(width)*height*3*layers);
        f.read(reinterpret_cast<char*>(defaultRows.data()),count);f.read(reinterpret_cast<char*>(pixels.data()),pixels.size());
        if(!f)throw std::runtime_error("Truncated fixed-pose asset.");
    }catch(const std::exception& e){error=e.what();pixels.clear();}
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
inline void Upload(const std::set<int>& selected) {
    if(!device || pixels.empty() || original.empty())return;
    const auto& colors=showOriginal?original:edited;
    if(texture && uploaded==colors && uploadedBands==selected && uploadedHighlight==highlight)return;
    if(!texture && FAILED(device->CreateTexture(width,height,1,D3DUSAGE_DYNAMIC,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&texture,nullptr)))throw std::runtime_error("Could not create preview texture.");
    D3DLOCKED_RECT lock{};if(FAILED(texture->LockRect(0,&lock,nullptr,D3DLOCK_DISCARD)))throw std::runtime_error("Could not upload preview texture.");
    std::vector<ImU32> lookup(256*256);
    for(int b=0;b<sizes.size();++b)for(int s=0;s<256;++s)lookup[b*256+s]=Sample(b,s,colors);
    for(unsigned y=0;y<height;++y) {
        auto row=reinterpret_cast<ImU32*>(static_cast<unsigned char*>(lock.pBits)+y*lock.Pitch);
        for(unsigned x=0;x<width;++x) {
            float rgb[3]={0.20f,0.20f,0.20f};
            for(unsigned layer=0;layer<layers;++layer) {
                auto offset=(size_t(layer)*width*height+y*width+x)*3;
                int band=pixels[offset+2],shade=pixels[offset+1];
                auto color=lookup[band*256+shade];float a=((color>>24)&255)/255.0f;
                for(int ch=0;ch<3;++ch) {
                    float v=((color>>(ch*8))&255)/255.0f;
                    if(highlight && !selected.empty())v=selected.count(band)?v*0.55f+(ch==2?0.0f:0.45f):v*0.25f;
                    rgb[ch]=rgb[ch]*(1-a)+v*a;
                }
            }
            row[x]=0xff000000|static_cast<ImU32>(rgb[0]*255)|static_cast<ImU32>(rgb[1]*255)<<8|static_cast<ImU32>(rgb[2]*255)<<16;
        }
    }
    texture->UnlockRect(0);
    uploaded=colors;uploadedBands=selected;uploadedHighlight=highlight;
}
inline void Draw(int targetSlot,int targetPalette,const char* character,const std::vector<ImU32>& liveColors,const std::vector<int>& layout,const ColorGroup& group,const std::set<int>& bands) {
    if(!ImGui::CollapsingHeader("Sprite preview (prototype)",ImGuiTreeNodeFlags_DefaultOpen))return;
    ImGui::TextWrapped("Cerebella fixed pose | local colors only. Overlays, lighting, rainbow and transformations are not previewed.");
    if(_stricmp(character,"Cerebella")!=0){ImGui::TextDisabled("Select Cerebella to use this fixed-pose prototype.");return;}
    Load();if(!error.empty()){ImGui::TextWrapped("%s",error.c_str());return;}
    if(slot!=targetSlot || palette!=targetPalette || sizes!=layout) {
        ForgetMatch();slot=targetSlot;palette=targetPalette;sizes=layout;original=edited=liveColors;
    }
    if(ImGui::Button("Snapshot match colors"))original=edited=liveColors;
    ImGui::SameLine();ImGui::Checkbox("Original / Edited",&showOriginal);
    ImGui::TextDisabled(showOriginal?"Original snapshot":"Edited locally (match unchanged)");
    ImGui::SliderFloat("Zoom",&zoom,0.25f,3.0f,"%.2fx");ImGui::Checkbox("Highlight selected group",&highlight);
    ImGui::BeginDisabled(showOriginal);
    for(int i=group.startIndex;i<group.startIndex+group.count && i<edited.size();++i) {
        ImGui::PushID(i);auto color=ColorsTools::ImU32BGRAtoImVec4RGBA(showOriginal?original[i]:edited[i]);
        if(ImGui::ColorEdit4("##Local swatch",&color.x,ImGuiColorEditFlags_NoInputs))edited[i]=ColorsTools::ImVec4RGBAtoImU32BGRA(color);
        ImGui::PopID();if((i-group.startIndex+1)%6)ImGui::SameLine();
    }
    ImGui::NewLine();ImGui::EndDisabled();
    try {Upload(bands);}catch(const std::exception& e){LostDevice();ImGui::TextWrapped("%s",e.what());}
    if(texture){ImGui::BeginChild("Preview canvas",ImVec2(0,420),true,ImGuiWindowFlags_HorizontalScrollbar);ImGui::Image((ImTextureID)(intptr_t)texture,ImVec2(width*zoom,height*zoom));ImGui::EndChild();}
}
}
