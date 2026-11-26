// Runs at the appearance evaluator entry, before its native predicates read gates.
#include "NativePaletteObserver.h"
#include "../../PalleteEditor/Data/CharacterMoveRoutes.h"
#include <mutex>
#include <cmath>
namespace MoveContextHost {
inline PaletteData::MoveContextService service;
inline std::recursive_mutex mutex;
inline std::map<std::string,std::unique_ptr<PaletteData::MoveRouteTable>> tables;
inline bool Read(uintptr_t address,void* out,size_t count) {
    SIZE_T done=0;return address && ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),out,count,&done) && done==count;
}
inline bool Word(uintptr_t address,DWORD& value){return Read(address,&value,4);}
inline bool Write(uintptr_t address,float value) {
    SIZE_T done=0;float check=-1;
    return WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),&value,4,&done) && done==4 && Read(address,&check,4) && check==value;
}
inline bool Header(uintptr_t address,const std::array<unsigned char,12>& expected) {
    std::array<unsigned char,12> bytes;return Read(address,bytes.data(),bytes.size()) && bytes==expected;
}
inline void Observe(uintptr_t palette,int index,uintptr_t,uintptr_t entity) {
    std::lock_guard lock(mutex);
    try {
        DWORD root=0,mode=0,actor=0,actualPalette=0,actualIndex=0;
        if(!Word(OverlayEditor::base+0x85ea4c,root) || !root || !Word(root+0x168,mode) || mode!=4){service.Suspend();return;}
        if(!entity)return; // Initialization calls have no fighter context.
        if(!Word(entity+0x330,actor) || !actor || !Word(actor+0x328,actualPalette) || !Word(actor+0x330,actualIndex)){service.Suspend();return;}
        if(actualPalette!=palette || actualIndex!=static_cast<DWORD>(index))return;
        int slot=-1,matches=0,entities=0;DWORD candidate=0,owner=0;
        for(int n=0;n<6;++n) {
            if(!Word(root+0x5e8+4*n,candidate)){service.Suspend();return;}
            if(candidate==actor){slot=n;++matches;}
            if(!Word(root+0x1d4+4*n,candidate)){service.Suspend();return;}
            if(candidate) {
                if(!Word(candidate+0x330,owner)){service.Suspend();return;}
                if(owner==actor){++entities;if(candidate!=entity)return;}
            }
        }
        if(matches!=1 || entities!=1){service.Suspend();return;}
        if(!service.HasActor(actor,palette))return;
        char name[32]{};float motion[2]{};
        if(!Read(actor+0x310,name,sizeof(name)) || !memchr(name,0,sizeof(name)) || !Read(entity+0x9c,motion,sizeof(motion)) ||
           !std::isfinite(motion[0]) || !std::isfinite(motion[1]) || motion[0]<0 || motion[0]>65534 || motion[1]<-1 || motion[1]>65534 ||
           std::floor(motion[0])!=motion[0] || std::floor(motion[1])!=motion[1]){service.Suspend(slot);return;}
        DWORD verifyRoot=0,verifyActor=0,verifyOwner=0,verifyPalette=0,verifyIndex=0;
        if(!Word(OverlayEditor::base+0x85ea4c,verifyRoot) || verifyRoot!=root || !Word(root+0x168,mode) || mode!=4 ||
           !Word(root+0x5e8+4*slot,verifyActor) || verifyActor!=actor || !Word(entity+0x330,verifyOwner) || verifyOwner!=actor ||
           !Word(actor+0x328,verifyPalette) || verifyPalette!=palette || !Word(actor+0x330,verifyIndex) || verifyIndex!=actualIndex){service.Suspend(slot);return;}
        PaletteData::MoveContextFrame frame;
        frame.identity={root,actor,entity,palette,OverlayEditor::ruleGeneration,slot};
        frame.character=name;frame.paletteIndex=index;frame.animation=static_cast<int>(motion[0]);frame.previous=static_cast<int>(motion[1]);
        service.Observe(frame,Header,Write);
    } catch(...) {service.Suspend();}
}
inline void Register() {
    OverlayEditor::moveContextHooks={
        [](){return NativePaletteObserver::Supported(OverlayEditor::base);},
        [](){return NativePaletteObserver::Installed();},
        [](){NativePaletteObserver::Install(OverlayEditor::base,Observe);},
        [](){NativePaletteObserver::Remove();},
        [](){std::lock_guard lock(mutex);service.Replace(OverlayEditor::ContextBindings());},
        [](){std::lock_guard lock(mutex);service.Abandon();},
        [](const std::string& character)->const PaletteData::MoveRouteTable* {
            std::lock_guard lock(mutex);auto& table=tables[character];
            if(!table)table=std::make_unique<PaletteData::MoveRouteTable>(PaletteData::BuildCharacterMoveRoutes(character));return table.get();
        },
        [](int slot)->const char* {std::lock_guard lock(mutex);switch(service.Status(slot).status){
            case PaletteData::MoveRouteStatus::Active:return "Tracking move";
            case PaletteData::MoveRouteStatus::Inactive:return "Idle";
            default:return "Move origin unavailable";
        }}
    };
}
}
