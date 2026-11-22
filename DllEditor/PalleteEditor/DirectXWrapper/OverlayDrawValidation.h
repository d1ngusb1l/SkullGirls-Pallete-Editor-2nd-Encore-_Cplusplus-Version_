#pragma once
#include <wrl/client.h>

namespace RendererTrace {
namespace OverlayDrawValidation {
using Microsoft::WRL::ComPtr;
inline ComPtr<IDirect3DBaseTexture9> first, second, feature;
inline ComPtr<IDirect3DPixelShader9> replacement;
inline ComPtr<IDirect3DTexture9> bandMap;
inline ULONGLONG deadline=0;
inline unsigned drawsReplaced=0;
inline int captureSlot=0;
inline std::string targetFeature;
inline unsigned tint[2]{};

inline void Reset() {
    deadline=0;
    captureSlot=0;
    first.Reset(); second.Reset(); feature.Reset(); replacement.Reset(); bandMap.Reset();
}

inline void Tick(IDirect3DDevice9* device) {
    if(deadline && GetTickCount64()>=deadline) {
        const auto count=drawsReplaced;
        Reset();
        PaletteData::AtomicWriteText("overlay-draw-result.json",Json{{"drawsReplaced",count},{"finished",true}}.dump(2));
    }
    const std::filesystem::path request="overlay-draw.request.json";
    if(!std::filesystem::exists(request))return;
    try {
        if(std::filesystem::file_size(request)>1024)throw std::runtime_error("Request too large.");
        Json data;
        {std::ifstream input(request);input>>data;}
        std::filesystem::remove(request);
        const auto action=data.at("action").get<std::string>();
        if(action=="capture-first" || action=="capture-second") {
            if(deadline)throw std::runtime_error("Draw test already active.");
            targetFeature=data.at("feature").get<std::string>();
            captureSlot=action=="capture-first"?1:2;
            return;
        }
        if(action!="preview" || deadline || !first || !second || !feature || first.Get()==second.Get())
            throw std::runtime_error("Two distinct textures from the same target draw are required.");
        const int band=data.at("secondBand").get<int>();
        if(band<1 || band>255)throw std::runtime_error("Invalid overlay band.");
        std::ifstream shader("overlay-shader/prototype.bin",std::ios::binary|std::ios::ate);
        const auto size=shader.tellg();
        if(size<8 || size>65536 || static_cast<size_t>(size)%4)throw std::runtime_error("Invalid prototype file.");
        std::vector<DWORD> tokens(static_cast<size_t>(size)/4);
        shader.seekg(0);
        if(!shader.read(reinterpret_cast<char*>(tokens.data()),size) || tokens.front()!=D3DPS_VERSION(3,0) ||
           tokens.back()!=0xffff || FAILED(device->CreatePixelShader(tokens.data(),replacement.ReleaseAndGetAddressOf())))
            throw std::runtime_error("Could not create the prototype shader.");
        if(FAILED(device->CreateTexture(1,256,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,bandMap.ReleaseAndGetAddressOf(),nullptr)))
            throw std::runtime_error("Could not create the band selector.");
        D3DLOCKED_RECT lock{};
        if(FAILED(bandMap->LockRect(0,&lock,nullptr,0)))throw std::runtime_error("Could not write the band selector.");
        for(int row=0;row<256;row++) {
            const bool alternate=row==band;
            const DWORD value=0xff000000 | (alternate?0x00ff0000:0) | (tint[alternate?1:0]<<8);
            memcpy(static_cast<unsigned char*>(lock.pBits)+row*lock.Pitch,&value,sizeof(value));
        }
        if(FAILED(bandMap->UnlockRect(0)))throw std::runtime_error("Could not finish the band selector.");
        drawsReplaced=0;
        deadline=GetTickCount64()+15000;
    }catch(const std::exception& e){
        Reset();
        PaletteData::AtomicWriteText("overlay-draw-result.json",Json{{"error",e.what()}}.dump(2));
    }
}

template<class Draw>
HRESULT Run(IDirect3DDevice9* device,Draw draw) {
    if(!captureSlot && !deadline)return draw();
    ComPtr<IDirect3DBaseTexture9> currentFeature, overlay;
    if(FAILED(device->GetTexture(2,currentFeature.GetAddressOf())) || !currentFeature ||
       FAILED(device->GetTexture(3,overlay.GetAddressOf())) || !overlay)return draw();
    if(captureSlot ? Pointer(currentFeature.Get())!=targetFeature : currentFeature.Get()!=feature.Get())return draw();
    ComPtr<IDirect3DPixelShader9> shader;
    UINT size=0;
    if(FAILED(device->GetPixelShader(shader.GetAddressOf())) || !shader ||
       FAILED(shader->GetFunction(nullptr,&size)) || size!=10812)return draw();
    std::vector<unsigned char> bytes(size);
    if(FAILED(shader->GetFunction(bytes.data(),&size)) || Identity(bytes)!="4f2d54bdda78e7fd")return draw();
    if(captureSlot) {
        float paletteIndex[4]{};
        if(FAILED(device->GetPixelShaderConstantF(29,paletteIndex,1)))return draw();
        tint[captureSlot-1]=static_cast<unsigned>(std::clamp(paletteIndex[0],0.0f,1.0f)*255.0f+0.5f);
        if(captureSlot==1) {first=overlay;feature=currentFeature;second.Reset();}
        else if(currentFeature.Get()==feature.Get())second=overlay;
        captureSlot=0;
        return draw();
    }
    if(overlay.Get()!=first.Get() || GetTickCount64()>=deadline)return draw();
    ComPtr<IDirect3DStateBlock9> state;
    if(FAILED(device->CreateStateBlock(D3DSBT_ALL,state.GetAddressOf())) || FAILED(state->Capture()))return draw();
    for(auto setting:{D3DSAMP_ADDRESSU,D3DSAMP_ADDRESSV,D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER,D3DSAMP_MIPFILTER,D3DSAMP_SRGBTEXTURE}) {
        DWORD value=0;
        if(FAILED(device->GetSamplerState(3,setting,&value)) || FAILED(device->SetSamplerState(4,setting,value))) {
            state->Apply();
            return draw();
        }
    }
    const bool bound=SUCCEEDED(device->SetTexture(4,second.Get())) &&
        SUCCEEDED(device->SetTexture(5,bandMap.Get())) &&
        SUCCEEDED(device->SetSamplerState(5,D3DSAMP_MINFILTER,D3DTEXF_POINT)) &&
        SUCCEEDED(device->SetSamplerState(5,D3DSAMP_MAGFILTER,D3DTEXF_POINT)) &&
        SUCCEEDED(device->SetSamplerState(5,D3DSAMP_MIPFILTER,D3DTEXF_NONE)) &&
        SUCCEEDED(device->SetSamplerState(5,D3DSAMP_SRGBTEXTURE,FALSE)) &&
        SUCCEEDED(device->SetPixelShader(replacement.Get()));
    if(!bound){state->Apply();return draw();}
    const auto result=draw();
    state->Apply();
    if(SUCCEEDED(result))++drawsReplaced;
    return result;
}
}
}
