#pragma once
// Diagnostic capture only: never changes shaders, textures, constants or draws.
// Included only by the validation build's D3D9 proxy translation unit.
#include <map>
#include <set>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <cstring>
#include <vector>
#include "../../../PalleteEditor/Data/AtomicFile.h"

namespace RendererTrace {
using Json=nlohmann::json;
inline int frames=0;
inline Json draws,shaders;
inline std::set<std::string> captured;
inline size_t totalShaderBytes=0;
inline uint32_t Word(const std::vector<unsigned char>& b,size_t off) {
    if(off>b.size() || b.size()-off<4)throw std::runtime_error("Truncated shader metadata.");
    uint32_t value;memcpy(&value,b.data()+off,4);return value;
}
inline uint16_t Half(const std::vector<unsigned char>& b,size_t off) {
    if(off>b.size() || b.size()-off<2)throw std::runtime_error("Truncated shader metadata.");
    uint16_t value;memcpy(&value,b.data()+off,2);return value;
}
inline std::string Text(const std::vector<unsigned char>& b,size_t base,uint32_t relative,size_t end) {
    if(relative>end-base)throw std::runtime_error("Invalid shader string offset.");
    size_t start=base+relative,pos=start;
    while(pos<end && pos-start<512 && b[pos])++pos;
    if(pos==end || pos-start==512)throw std::runtime_error("Unterminated shader string.");
    return std::string(b.begin()+start,b.begin()+pos);
}
inline Json Constants(const std::vector<unsigned char>& b) {
    Json result=Json::array();
    for(size_t off=4;off+4<=b.size();) {
        uint32_t token=Word(b,off);if(token==0xffff)break;
        if((token&0xffff)==0xfffe) {
            size_t bytes=((token>>16)&0x7fff)*4;
            if(bytes>b.size()-off-4)throw std::runtime_error("Invalid shader comment.");
            size_t begin=off+4,end=begin+bytes;
            if(bytes>=32 && Word(b,begin)==0x42415443) {
                size_t base=begin+4;uint32_t count=Word(b,base+12),relative=Word(b,base+16);
                if(count>256 || relative>end-base || count>(end-base-relative)/20)throw std::runtime_error("Invalid constant table.");
                for(uint32_t i=0;i<count;i++) {
                    size_t at=base+relative+i*20;
                    result.push_back({{"name",Text(b,base,Word(b,at),end)},{"registerSet",Half(b,at+4)},
                        {"registerIndex",Half(b,at+6)},{"registerCount",Half(b,at+8)}});
                }
                break;
            }
            off=end;
        } else {
            size_t bytes=4+((token>>24)&15)*4;
            if(bytes>b.size()-off)throw std::runtime_error("Invalid shader instruction.");
            off+=bytes;
        }
    }
    return result;
}
inline std::string Identity(const std::vector<unsigned char>& bytes) {
    uint64_t hash=14695981039346656037ull;
    for(auto byte:bytes){hash^=byte;hash*=1099511628211ull;}
    std::ostringstream out;out<<std::hex<<std::setw(16)<<std::setfill('0')<<hash;return out.str();
}
inline std::string Pointer(const void* pointer) {
    std::ostringstream out;out<<std::hex<<(uintptr_t)pointer;return out.str();
}
inline void Draw(IDirect3DDevice9* device,const char* kind,UINT primitives,
    INT baseVertexIndex=0,UINT minVertexIndex=0,UINT numVertices=0,UINT startIndex=0) {
    if(!frames || draws.size()>=512)return;
    try {
        IDirect3DPixelShader9* shader=nullptr;
        if(FAILED(device->GetPixelShader(&shader)) || !shader)return;
        struct ShaderRelease {IDirect3DPixelShader9* p;~ShaderRelease(){p->Release();}} shaderRelease{shader};
        UINT size=0;if(FAILED(shader->GetFunction(nullptr,&size)) || size<8 || size>1024*1024)return;
        std::vector<unsigned char> bytes(size);
        if(FAILED(shader->GetFunction(bytes.data(),&size)) || size!=bytes.size())return;
        auto id=Identity(bytes);
        if(!captured.count(id)) {
            if(captured.size()>=32 || totalShaderBytes+size>8*1024*1024)return;
            auto constants=Constants(bytes);
            PaletteData::AtomicWrite(std::filesystem::path("renderer-trace")/("pixel-"+id+".bin"),bytes);
            shaders[id]={{"byteLength",size},{"versionToken",Word(bytes,0)},{"constants",constants}};
            captured.insert(id);totalShaderBytes+=size;
        }
        Json textures=Json::array();
        for(DWORD sampler=0;sampler<16;sampler++) {
            IDirect3DBaseTexture9* texture=nullptr;
            if(FAILED(device->GetTexture(sampler,&texture)) || !texture)continue;
            Json entry={{"sampler",sampler},{"object",Pointer(texture)},{"type",(int)texture->GetType()}};
            if(texture->GetType()==D3DRTYPE_TEXTURE) {
                D3DSURFACE_DESC desc{};
                if(SUCCEEDED(static_cast<IDirect3DTexture9*>(texture)->GetLevelDesc(0,&desc)))
                    entry.update({{"width",desc.Width},{"height",desc.Height},{"format",(int)desc.Format},{"pool",(int)desc.Pool},{"usage",desc.Usage}});
            }
            texture->Release();textures.push_back(entry);
        }
        Json values=Json::object();
        for(auto& constant:shaders[id]["constants"]) {
            std::string name=constant["name"];UINT index=constant["registerIndex"],count=constant["registerCount"];
            if(constant["registerSet"]==2 && count && count<=32 && index+count<=224) {
                std::vector<float> data(count*4);
                if(SUCCEEDED(device->GetPixelShaderConstantF(index,data.data(),count)))values[name]=data;
            }
        }
        Json geometry=Json::object();
        geometry["baseVertexIndex"]=baseVertexIndex;
        geometry["minVertexIndex"]=minVertexIndex;
        geometry["numVertices"]=numVertices;
        geometry["startIndex"]=startIndex;
        IDirect3DVertexDeclaration9* declaration=nullptr;
        if(SUCCEEDED(device->GetVertexDeclaration(&declaration)) && declaration) {
            UINT count=0;
            if(SUCCEEDED(declaration->GetDeclaration(nullptr,&count)) && count>0 && count<=64) {
                std::vector<D3DVERTEXELEMENT9> elements(count);
                if(SUCCEEDED(declaration->GetDeclaration(elements.data(),&count))) {
                    Json decl=Json::array();
                    for(UINT i=0;i<count;i++)decl.push_back({{"stream",elements[i].Stream},{"offset",elements[i].Offset},
                        {"type",(int)elements[i].Type},{"method",(int)elements[i].Method},{"usage",(int)elements[i].Usage},{"usageIndex",elements[i].UsageIndex}});
                    geometry["vertexDeclaration"]=decl;
                }
            }
            declaration->Release();
        }
        Json streams=Json::array();
        for(UINT stream=0;stream<16;stream++) {
            IDirect3DVertexBuffer9* buffer=nullptr;UINT offset=0,stride=0;
            if(FAILED(device->GetStreamSource(stream,&buffer,&offset,&stride)) || !buffer)continue;
            D3DVERTEXBUFFER_DESC desc{};
            if(SUCCEEDED(buffer->GetDesc(&desc))) {
                Json streamEntry={{"stream",stream},{"object",Pointer(buffer)},{"offset",offset},{"stride",stride},{"length",desc.Size},{"format",(int)desc.Format},{"usage",desc.Usage},{"pool",(int)desc.Pool}};
                if(stride>0 && stride<=256 && desc.Size>=stride) {
                    void* data=nullptr;
                    if(SUCCEEDED(buffer->Lock(0,0,&data,D3DLOCK_READONLY)) && data) {
                        Json vertices=Json::array();
                        const unsigned char* bytes=static_cast<const unsigned char*>(data);
                        for(UINT vertex=0;vertex<8 && (vertex*stride+stride)<=desc.Size;vertex++) {
                            Json values=Json::array();
                            for(UINT at=0;at<stride && at+4<=stride && values.size()<16;at+=4) {
                                float value=0;memcpy(&value,bytes+vertex*stride+at,4);values.push_back(value);
                            }
                            vertices.push_back(values);
                        }
                        buffer->Unlock();streamEntry["vertexSamples"]=vertices;
                    }
                }
                streams.push_back(streamEntry);
            }
            buffer->Release();
        }
        geometry["streams"]=streams;
        IDirect3DIndexBuffer9* indices=nullptr;
        if(SUCCEEDED(device->GetIndices(&indices)) && indices) {
            D3DINDEXBUFFER_DESC desc{};
            if(SUCCEEDED(indices->GetDesc(&desc)))
                geometry["indices"]={{"object",Pointer(indices)},{"length",desc.Size},{"format",(int)desc.Format},{"usage",desc.Usage},{"pool",(int)desc.Pool}};
            indices->Release();
        }
        draws.push_back({{"kind",kind},{"primitives",primitives},{"geometry",geometry},{"shader",id},{"textures",textures},{"constants",values}});
    }catch(const std::exception& e){if(draws.size()<512)draws.push_back({{"error",e.what()}});}
}
inline void CheckOverlayShader(IDirect3DDevice9* device) {
    const auto request=std::filesystem::path("overlay-shader-check.request");
    if(!std::filesystem::exists(request))return;
    std::filesystem::remove(request);
    Json result=Json::object();
    D3DCAPS9 caps{};
    if(SUCCEEDED(device->GetDeviceCaps(&caps))) {
        result["pixelShaderVersion"]=caps.PixelShaderVersion;
        result["maxInstructionSlots"]=caps.MaxPixelShader30InstructionSlots;
    }
    for(const char* variant:{"baseline","prototype","unlit"}) {
        std::ifstream input(std::filesystem::path("overlay-shader")/(std::string(variant)+".bin"),std::ios::binary|std::ios::ate);
        const auto size=input.tellg();
        if(size<8 || size>65536 || static_cast<size_t>(size)%4) {
            result[variant]={{"error","Missing or invalid shader file"}};
            continue;
        }
        std::vector<DWORD> tokens(static_cast<size_t>(size)/4);
        input.seekg(0);
        if(!input.read(reinterpret_cast<char*>(tokens.data()),size) ||
           tokens.front()!=D3DPS_VERSION(3,0) || tokens.back()!=0xffff) {
            result[variant]={{"error","Expected complete ps_3_0 bytecode"}};
            continue;
        }
        IDirect3DPixelShader9* shader=nullptr;
        const auto status=device->CreatePixelShader(tokens.data(),&shader);
        result[variant]={{"accepted",SUCCEEDED(status) && shader!=nullptr},{"hresult",static_cast<uint32_t>(status)}};
        if(shader)shader->Release();
    }
    PaletteData::AtomicWriteText("overlay-shader-check-result.json",result.dump(2));
}
inline void EndFrame(IDirect3DDevice9* device) {
    try {
        CheckOverlayShader(device);
        if(frames && --frames==0) {
            PaletteData::AtomicWriteText("renderer-trace/capture.json",Json({{"shaders",shaders},{"draws",draws},
                {"readOnlyCapture",true},{"drawLimit",512},{"shaderIdentity","FNV-1a diagnostic identifier; not an integrity checksum"}}).dump(2));
        }
        if(!frames && std::filesystem::exists("renderer-trace.request")) {
            std::filesystem::remove("renderer-trace.request");std::filesystem::create_directories("renderer-trace");
            draws=Json::array();shaders=Json::object();captured.clear();totalShaderBytes=0;frames=2;
        }
    }catch(...) {frames=0;}
}
}
