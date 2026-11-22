#pragma once
#ifndef SGPE_DLL_HOST
#include "pch.h"
#include "Character.h"
#include "Data/GroupJSONFiles.h"
#endif
#include <array>
#include <map>
#include <set>
#include <vector>
#include <stdexcept>
#include <cstring>
#include <functional>
#include "Data/PaletteDocument.h"

// Phase 1 overlay prototype. Native layouts are guarded to the researched build.
namespace OverlayEditor {
// DLL callers run at the render boundary on the game's own thread. The
// external editor must suspend the target; suspending ourselves would deadlock.
class EffectUpdateGuard {
#ifndef SGPE_DLL_HOST
    HANDLE target;
    LONG (NTAPI* resume)(HANDLE);
#endif
public:
    explicit EffectUpdateGuard(HANDLE process) {
#ifndef SGPE_DLL_HOST
        target=process;
        auto suspend=(LONG (NTAPI*)(HANDLE))GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtSuspendProcess");
        resume=(LONG (NTAPI*)(HANDLE))GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtResumeProcess");
        if(GetProcessId(process)==GetCurrentProcessId() || !suspend || !resume || suspend(process)!=0)
            throw std::runtime_error("Could not pause game for effect update.");
#endif
    }
    ~EffectUpdateGuard() {
#ifndef SGPE_DLL_HOST
        resume(target);
#endif
    }
    EffectUpdateGuard(const EffectUpdateGuard&)=delete;
    EffectUpdateGuard& operator=(const EffectUpdateGuard&)=delete;
};
using Bytes = std::vector<unsigned char>;
inline HANDLE process = nullptr;
inline DWORD base = 0;
inline ULONGLONG refreshUntil = 0;
inline std::string status;
struct Original { DWORD actor, palette, address; int slot; Bytes header; };
inline std::map<DWORD, Original> originals;
inline std::map<DWORD,nlohmann::json> overlayAssignments,rainbowAssignments;
inline std::map<std::string, Bytes> templates;
inline std::set<std::string> resolvedOverlayTextures;
inline std::set<int> selected;
inline std::string texture;
inline int lighting = 0, tint = 0;
inline std::string selectionKey;
inline void PausedWrite(DWORD address, const Bytes& bytes);
inline void ClearColorAssignments();
inline Bytes Read(DWORD address, size_t count) {
    Bytes b(count); SIZE_T n = 0;
    if (!ReadProcessMemory(process, (void*)address, b.data(), count, &n) || n != count)
        throw std::runtime_error("Could not read game data.");
    return b;
}
inline DWORD Word(DWORD a) { auto b = Read(a, 4); DWORD v; memcpy(&v,b.data(),4); return v; }
inline void Put(Bytes& b, size_t off, DWORD v) { memcpy(b.data()+off,&v,4); }
inline void Write(DWORD a, const Bytes& b) {
    SIZE_T n=0;
    if (!WriteProcessMemory(process,(void*)a,b.data(),b.size(),&n) || n!=b.size())
        throw std::runtime_error("Could not update game data.");
}
inline void Branch(unsigned char opcode) {
    DWORD a=base+0x87dc4, old=0, ignored=0;
    if (!VirtualProtectEx(process,(void*)a,1,PAGE_EXECUTE_READWRITE,&old)) throw std::runtime_error("Could not refresh shader cache.");
    bool ok=WriteProcessMemory(process,(void*)a,&opcode,1,nullptr)!=0;
    FlushInstructionCache(process,(void*)a,1);
    VirtualProtectEx(process,(void*)a,1,old,&ignored);
    if (!ok) throw std::runtime_error("Shader refresh failed.");
}
inline void Refresh() {
    auto b=Read(base+0x87dc3,6);
    if (b!=Bytes{0x0f,0x84,0xbd,6,0,0} && b!=Bytes{0x0f,0x88,0xbd,6,0,0})
        throw std::runtime_error("Unsupported shader cache signature.");
    // CMP of the cached boolean against zero cannot set SF for either 0 or 1.
    // JZ -> JS therefore rebuilds for both clean and dirty states. JZ -> JNZ
    // (the earlier prototype) incorrectly skipped genuine condition changes.
    Branch(0x88); refreshUntil=GetTickCount64()+300;
}
inline void Tick() {
    if (refreshUntil && GetTickCount64()>=refreshUntil) { Branch(0x84); refreshUntil=0; }
}
inline bool Valid(const Original& o) {
    DWORD root=Word(base+0x85ea4c);
    return Word(root+0x168)==4 && Word(root+0x5e8+o.slot*4)==o.actor && Word(o.actor+0x328)==o.palette;
}
inline void Remove(DWORD address) {
    auto i=originals.find(address);
    if(i==originals.end()) return;
    if(Valid(i->second)) { PausedWrite(address,i->second.header); Refresh(); }
    originals.erase(i);
    overlayAssignments.erase(address); rainbowAssignments.erase(address);
}
inline void Shutdown() {
    if(!process) return;
    try {
        for(auto& item:originals) if(Valid(item.second)) PausedWrite(item.first,item.second.header);
        if(!originals.empty()) { Refresh(); Sleep(350); }
        if(refreshUntil) { Branch(0x84); refreshUntil=0; }
    } catch (...) { try { if(refreshUntil) Branch(0x84); } catch (...) {} }
    refreshUntil=0; resolvedOverlayTextures.clear();
    ClearColorAssignments(); originals.clear(); overlayAssignments.clear(); rainbowAssignments.clear(); templates.clear(); process=nullptr;
}
inline void ForgetMatch() { ClearColorAssignments(); originals.clear(); overlayAssignments.clear(); rainbowAssignments.clear(); templates.clear(); selectionKey.clear(); }
inline void PausedWrite(DWORD address, const Bytes& bytes) {
    EffectUpdateGuard updateGuard(process);
    try {
        Write(address,bytes);
        if(Read(address,bytes.size())!=bytes) throw std::runtime_error("Effect write verification failed.");
    } catch(...) { throw; }

}
inline void Bind(HANDLE h, DWORD b) {
    if(process==h && base==b) return;
    Shutdown(); process=h; base=b;
    DWORD pe=Word(base+0x3c);
    if(Word(base+pe+8)!=0x6af3723f || Word(base+pe+24+56)!=0x8bd000)
        throw std::runtime_error("Overlay support requires Skullgirls build 26588330.");
}
// Allocate native container data with the game's operator new. Buffers detached
// on Reset are retained until process exit, avoiding reads after free by drawing.
inline DWORD Allocate(size_t count) {
#ifdef SGPE_DLL_HOST
    auto allocate=(void* (__cdecl*)(size_t))(base+0x2cd15c);
    auto result=allocate(count);
    if(!result) throw std::runtime_error("Native allocation failed.");
    return (DWORD)result;
#else
    Bytes code{0x68,0,0,0,0,0xb8,0,0,0,0,0xff,0xd0,0x83,0xc4,4,0xc2,4,0};
    Put(code,1,(DWORD)count); Put(code,6,base+0x2cd15c);
    void* remote=VirtualAllocEx(process,nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    if(!remote) throw std::runtime_error("Could not allocate effect data.");
    Write((DWORD)remote,code); FlushInstructionCache(process,remote,code.size());
    HANDLE thread=CreateRemoteThread(process,nullptr,0,(LPTHREAD_START_ROUTINE)remote,nullptr,0,nullptr);
    if(!thread) { VirtualFreeEx(process,remote,0,MEM_RELEASE); throw std::runtime_error("Native allocation failed."); }
    DWORD wait=WaitForSingleObject(thread,5000), result=0;
    if(wait==WAIT_OBJECT_0) GetExitCodeThread(thread,&result);
    CloseHandle(thread);
    if(wait==WAIT_OBJECT_0) VirtualFreeEx(process,remote,0,MEM_RELEASE);
    if(wait!=WAIT_OBJECT_0 || !result) throw std::runtime_error("Native allocation did not complete.");
    return result;
#endif
}
#include "OverlayTextureResources.inl"
#include "BundledOverlayTemplates.inl"
inline void Catalog() {
    templates.clear(); DWORD root=Word(base+0x85ea4c);
    BundledOverlayTemplates();
    // Fixed-build preset metadata contains inline names and stable resource keys;
    // all container pointers are replaced before attaching a new record.
    try {
        nlohmann::json catalog; std::ifstream file("overlay-textures.json"); file>>catalog;
        for(auto& item:catalog.items()) {
            auto record=item.value().get<Bytes>();
            if(record.size()==120) templates.emplace(item.key(),record);
        }
    } catch(...) {}
    for(int slot=0;slot<6;slot++) {
        DWORD actor=Word(root+0x5e8+4*slot); if(!actor) continue;
        DWORD pal=Word(actor+0x328), count=Word(pal), outer=Word(pal+0x14);
        if(count>256) continue;
        for(DWORD index=0;index<count;index++) {
            DWORD begin=Word(outer+12*index), end=Word(outer+12*index+4);
            if(end<begin || end-begin>120*64 || (end-begin)%120) continue;
            for(DWORD a=begin;a<end;a+=120) {
                auto record=Read(a,120); DWORD len=Word(a+20), cap=Word(a+24);
                if(!len || len>15 || cap!=15 || Word(a+76)>15) continue;
                std::string name((char*)record.data()+4,len);
                templates.emplace(name,record);
            }
        }
    }
    if(texture.empty() && !templates.empty()) texture=templates.begin()->first;
}
inline Bytes BuildOverlayRecord(const PaletteData::Overlay& assignment) {
    if(assignment.bands.empty())throw std::runtime_error("Select one or more groups.");
    auto preset=templates.find(assignment.texture);
    if(preset==templates.end()) throw std::runtime_error("Could not load overlay texture '"+assignment.texture+"': no supported preset metadata is available.");
    ResolveOverlayTexture(assignment.texture);
    Bytes bands;
    for(int band:assignment.bands) {
        if(band<1 || band>255)throw std::runtime_error("Invalid overlay band.");
        bands.push_back(static_cast<unsigned char>(band));
    }
    const DWORD bandMemory=Allocate(bands.size());
    Write(bandMemory,bands);
    Bytes record=preset->second;
    record[0]=record[1]=0;
    const DWORD bandEnd=bandMemory+static_cast<DWORD>(bands.size());
    Put(record,32,bandMemory); Put(record,36,bandEnd); Put(record,40,bandEnd);
    record[44]=static_cast<unsigned char>(assignment.lighting);
    record[46]=255;
    Put(record,48,assignment.tintBand); Put(record,88,0);
    record[92]=0;
    std::fill(record.begin()+96,record.end(),0); Put(record,116,15);
    return record;
}
inline Bytes OverlayHeader(const PaletteData::Overlay& primary,const PaletteData::Overlay* secondary=nullptr) {
    auto records=BuildOverlayRecord(primary);
    if(secondary) {
        auto record=BuildOverlayRecord(*secondary);
        records.insert(records.end(),record.begin(),record.end());
    }
    DWORD memory=Allocate(records.size()); Write(memory,records);
    DWORD end=memory+static_cast<DWORD>(records.size());
    Bytes header(12); Put(header,0,memory); Put(header,4,end); Put(header,8,end);
    return header;
}
inline void Apply(const Character& c,const PaletteData::Overlay* secondary=nullptr) {
    Catalog();
#ifdef SGPE_DLL_HOST
    const auto header=OverlayHeader({texture,selected,lighting,tint},secondary);
#else
    // The standalone editor retains the second assignment in the .pal metadata.
    const auto header=OverlayHeader({texture,selected,lighting,tint});
#endif
    DWORD root=Word(base+0x85ea4c), actor=Word(root+0x5e8+4*c.ID), pal=Word(actor+0x328);
    DWORD address=Word(pal+0x14)+12*c.Current_Pallete_Num;
    Original original{actor,pal,address,c.ID,Read(address,12)};
    auto old=originals.find(address); if(old!=originals.end()) original=old->second;
    EffectUpdateGuard updateGuard(process);
    try { if(!Valid(original)) throw std::runtime_error("Match changed; retry in the new match."); originals[address]=original; Write(address,header); }
    catch(...) { throw; }

    overlayAssignments[address]={{"texture",texture},{"bands",selected},{"lighting",lighting},{"tintBand",tint}};
    if(secondary)overlayAssignments[address]["secondOverlay"]={
        {"texture",secondary->texture},{"bands",secondary->bands},{"lighting",secondary->lighting},{"tintBand",secondary->tintBand}};
    Refresh(); status="Overlay applied. Reset restores the original overlay.";
}
#include "RainbowControls.inl"
#include "AppearanceControls.inl"
#include "TransformationControls.inl"
#include "PalettePersistence.inl"
inline void Draw(HANDLE h,DWORD b,Character& c) {
    try {
        Bind(h,b); Tick();
        std::string key=std::to_string(c.ID)+":"+std::to_string(c.Current_Pallete_Num);
        if(selectionKey!=key) {
            selected.clear(); rainbowBands.clear(); opacityBands.clear(); colorLinks.clear(); SelectTransformationPalette(c);
            auto data=AppearanceData(c);
            auto overlay=overlayAssignments.find(Word(data.palette+0x14)+12*c.Current_Pallete_Num);
            if(overlay!=overlayAssignments.end()) { selected=overlay->second.at("bands").get<std::set<int>>(); texture=overlay->second.at("texture"); lighting=overlay->second.at("lighting"); tint=overlay->second.at("tintBand"); }
            auto rainbow=rainbowAssignments.find(RainbowAddress(c));
            if(rainbow!=rainbowAssignments.end()) { rainbowBands=rainbow->second.at("bands").get<std::set<int>>(); rainbowSpeed=rainbow->second.at("speed"); rainbowInterval=rainbow->second.at("interval"); }
            selectionKey=key;
        }
        DrawTransformation(c);
        DrawAppearance(c);
        DrawRainbow(c);
        if(!status.empty())ImGui::TextWrapped("%s",status.c_str());
    } catch(const std::exception& e) { status=e.what(); ImGui::TextWrapped("Overlay: %s",status.c_str()); }
}
}
