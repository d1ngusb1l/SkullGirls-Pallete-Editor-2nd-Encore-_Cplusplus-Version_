#pragma once
#include <array>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <vector>

namespace PaletteData {
using EntryBytes=std::vector<unsigned char>;
struct PaletteEntryPatch {
    uint32_t entry=0,trampoline=0;
    EntryBytes original,redirect,delegate;
};
inline void EntryRelative(EntryBytes& bytes,size_t offset,uint32_t destination,uint32_t next) {
    const uint32_t displacement=destination-next;
    for(int i=0;i<4;++i)bytes[offset+i]=static_cast<unsigned char>(displacement>>(8*i));
}
inline PaletteEntryPatch PlanPaletteEntry(uint32_t entry,uint32_t target,uint32_t trampoline) {
    if(!entry || !target || !trampoline || entry>UINT32_MAX-6 || trampoline>UINT32_MAX-11 ||
       (target>=entry && target<entry+6) || (target>=trampoline && target<trampoline+11) || (trampoline<entry+6 && entry<trampoline+11))throw std::runtime_error("Invalid palette observer addresses.");
    PaletteEntryPatch plan{entry,trampoline,{0x53,0x8b,0xdc,0x83,0xec,0x08},{0xe9,0,0,0,0,0x90},{0x53,0x8b,0xdc,0x83,0xec,0x08,0xe9,0,0,0,0}};
    EntryRelative(plan.redirect,1,target,entry+5);EntryRelative(plan.delegate,7,entry+6,trampoline+11);
    return plan;
}
// The host must serialize these code changes with the palette/render thread.
// Delegate storage is retained; no renderer-visible allocation is freed here.
template<class Reader,class Writer>void InstallPaletteEntry(const PaletteEntryPatch& plan,Reader read,Writer write) {
    if(read(plan.entry,6)!=plan.original)throw std::runtime_error("Unsupported or externally hooked palette updater.");
    write(plan.trampoline,plan.delegate);
    if(read(plan.trampoline,11)!=plan.delegate)throw std::runtime_error("Palette delegate verification failed.");
    try {
        write(plan.entry,plan.redirect);
        if(read(plan.entry,6)!=plan.redirect)throw std::runtime_error("Palette observer verification failed.");
    } catch(...) {
        const auto failure=std::current_exception();
        try{write(plan.entry,plan.original);if(read(plan.entry,6)!=plan.original)throw std::runtime_error("Palette entry restoration verification failed.");}
        catch(...){throw std::runtime_error("Palette observer installation and entry restoration failed.");}
        std::rethrow_exception(failure);
    }
}
template<class Reader,class Writer>void RemovePaletteEntry(const PaletteEntryPatch& plan,Reader read,Writer write) {
    if(read(plan.entry,6)!=plan.redirect)throw std::runtime_error("Palette observer entry changed externally; refusing to overwrite it.");
    try {
        write(plan.entry,plan.original);
        if(read(plan.entry,6)!=plan.original)throw std::runtime_error("Palette observer removal verification failed.");
    } catch(...) {
        const auto failure=std::current_exception();
        try{write(plan.entry,plan.redirect);if(read(plan.entry,6)!=plan.redirect)throw std::runtime_error("Palette observer rollback verification failed.");}
        catch(...){throw std::runtime_error("Palette observer removal and rollback failed.");}
        std::rethrow_exception(failure);
    }
}
}
