#include "pch.h"
#include "NativePaletteObserver.h"
#include "../../PalleteEditor/Data/PaletteEntryPatch.h"
#include <optional>
#include <mutex>

namespace NativePaletteObserver {
namespace {
using NativeFn=void(__thiscall*)(void*,int,void*,void*);
NativeFn delegate=nullptr;
Callback callback=nullptr;
std::optional<PaletteData::PaletteEntryPatch> installed;
void* retainedCode=nullptr;
uintptr_t installedBase=0;
std::recursive_mutex evaluatorMutex;
PaletteData::EntryBytes Read(uint32_t address,size_t count) {
    PaletteData::EntryBytes result(count);SIZE_T actual=0;
    if(!ReadProcessMemory(GetCurrentProcess(),(void*)(uintptr_t)address,result.data(),count,&actual) || actual!=count)throw std::runtime_error("Could not read palette updater entry.");
    return result;
}
uint32_t Word(uintptr_t address) {
    uint32_t value=0;SIZE_T count=0;
    if(!ReadProcessMemory(GetCurrentProcess(),(void*)address,&value,4,&count) || count!=4)throw std::runtime_error("Could not read palette updater build guard.");
    return value;
}
void ProtectedWrite(uint32_t address,const PaletteData::EntryBytes& bytes) {
    DWORD old=0,unused=0;
    if(!VirtualProtect((void*)(uintptr_t)address,bytes.size(),PAGE_EXECUTE_READWRITE,&old))throw std::runtime_error("Could not protect palette updater entry.");
    SIZE_T count=0;const bool written=WriteProcessMemory(GetCurrentProcess(),(void*)(uintptr_t)address,bytes.data(),bytes.size(),&count) && count==bytes.size();
    const bool flushed=FlushInstructionCache(GetCurrentProcess(),(void*)(uintptr_t)address,bytes.size())!=0;
    const bool restored=VirtualProtect((void*)(uintptr_t)address,bytes.size(),old,&unused)!=0;
    if(!written || !flushed || !restored)throw std::runtime_error("Palette observer code write failed.");
}
void __fastcall Observe(void* palette,void*,int index,void* cache,void* entity) {
    // Keep the observation and the corresponding native gate reads together.
    std::lock_guard lock(evaluatorMutex);
    // Preserve every native call, including initialization/null-entity calls.
    // The coordinator must fail its own gates closed on invalid snapshots.
    if(auto observer=callback)try{observer((uintptr_t)palette,index,(uintptr_t)cache,(uintptr_t)entity);}catch(...){}
    delegate(palette,index,cache,entity);
}
}
bool Installed(){std::lock_guard lock(evaluatorMutex);return installed.has_value();}
bool Supported(uintptr_t moduleBase) {
    std::lock_guard lock(evaluatorMutex);
    try {
        if(moduleBase!=(uintptr_t)GetModuleHandleW(nullptr))return false;
        const auto pe=Word(moduleBase+0x3c);
        if(pe>0x10000 || Word(moduleBase+pe+8)!=0x6af3723f || Word(moduleBase+pe+24+56)!=0x8bd000)return false;
        const auto current=Read((uint32_t)(moduleBase+0x87a60),6);
        return installed?(moduleBase==installedBase && current==installed->redirect):current==PaletteData::EntryBytes{0x53,0x8b,0xdc,0x83,0xec,0x08};
    }catch(...){return false;}
}
void Install(uintptr_t moduleBase,Callback observer) {
    std::lock_guard lock(evaluatorMutex);
    static_assert(sizeof(void*)==4,"Palette observer requires the researched Win32 ABI.");
    if(!observer || !Supported(moduleBase))throw std::runtime_error("Unsupported palette updater observer signature/build.");
    if(installed){callback=observer;return;}
    // Allocate once. Never release code a renderer/native call may still reference.
    if(!retainedCode)retainedCode=VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READ);
    if(!retainedCode)throw std::runtime_error("Could not allocate palette delegate.");
    auto plan=PaletteData::PlanPaletteEntry((uint32_t)(moduleBase+0x87a60),(uint32_t)(uintptr_t)&Observe,(uint32_t)(uintptr_t)retainedCode);
    delegate=(NativeFn)retainedCode;callback=observer;
    try{PaletteData::InstallPaletteEntry(plan,Read,ProtectedWrite);}
    catch(...){callback=nullptr;throw;}
    installedBase=moduleBase;installed=std::move(plan);
}
void Remove() {
    std::lock_guard lock(evaluatorMutex);
    if(!installed)return;
    PaletteData::RemovePaletteEntry(*installed,Read,ProtectedWrite);
    callback=nullptr;installed.reset();installedBase=0;
    // delegate and retainedCode deliberately remain valid until process exit.
}
}
