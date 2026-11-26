#pragma once
#include "MoveRouteTracker.h"
#include <array>

namespace PaletteData {
struct MoveContextGate {
    uintptr_t address=0;
    int route=-1;
    bool value=false,known=false;
};
struct MoveContextBinding {
    uintptr_t actor=0,palette=0,headerAddress=0;
    int slot=-1,paletteIndex=-1;
    std::string character;
    std::array<unsigned char,12> header{};
    const MoveRouteTable* table=nullptr;
    std::vector<MoveContextGate> gates;
};
struct MoveContextFrame {
    MoveRouteIdentity identity;
    const char* character=nullptr;
    int paletteIndex=-1,animation=-1,previous=-1;
};
// Configuration allocates on Replace. Observe/Invalidate only update existing
// route state and caller-owned float gates; they never touch gameplay memory.
class MoveContextService {
    struct Slot {MoveRouteTracker tracker;MoveRouteIdentity identity;const MoveRouteTable* table=nullptr;MoveRouteResult result;};
    std::array<Slot,6> slots;
    std::vector<MoveContextBinding> bindings;
    template<class Writer>bool Gate(MoveContextGate& gate,bool value,Writer& write) {
        if(gate.known && gate.value==value)return true;
        if(!write(gate.address,value?1.f:0.f)){gate.known=false;return false;}
        gate.value=value;gate.known=true;return true;
    }
public:
    void Replace(std::vector<MoveContextBinding> next) {
        for(auto& binding:next) {
            if(!binding.actor || !binding.palette || !binding.headerAddress || binding.slot<0 || binding.slot>=6 || binding.paletteIndex<0 || !binding.table || binding.character.empty())throw std::runtime_error("Invalid move context binding.");
            for(auto& gate:binding.gates){if(!gate.address || gate.route<0 || gate.route>=binding.table->Routes())throw std::runtime_error("Invalid move context gate.");gate.known=false;}
        }
        for(int slot=0;slot<6;++slot) {
            const auto match=std::find_if(next.begin(),next.end(),[&](const auto& binding){return binding.slot==slot;});
            if(match==next.end() || slots[slot].identity.actor!=match->actor || slots[slot].identity.palette!=match->palette || slots[slot].table!=match->table)slots[slot]=Slot{};
        }
        bindings=std::move(next);
    }
    bool HasActor(uintptr_t actor,uintptr_t palette) const {
        return std::any_of(bindings.begin(),bindings.end(),[&](const auto& binding){return binding.actor==actor && binding.palette==palette;});
    }
    MoveRouteResult Status(int slot) const {return slot>=0 && slot<6?slots[slot].result:MoveRouteResult{};}
    template<class Writer>void Invalidate(int slot,Writer write) {
        if(slot>=0 && slot<6)slots[slot]=Slot{};else for(auto& state:slots)state=Slot{};
        for(auto& binding:bindings)if(slot<0 || binding.slot==slot)for(auto& gate:binding.gates)Gate(gate,false,write);
    }
    template<class HeaderLive,class Writer>void Observe(const MoveContextFrame& frame,HeaderLive live,Writer write) {
        const auto& key=frame.identity;
        if(!key.Valid() || !frame.character || frame.paletteIndex<0){Invalidate(-1,write);return;}
        const MoveRouteTable* table=nullptr;
        bool ownerLive=false;
        for(auto& binding:bindings)if(binding.slot==key.slot && binding.actor==key.actor && binding.palette==key.palette && binding.character==frame.character) {
            if(live(binding.headerAddress,binding.header)){table=binding.table;ownerLive=true;break;}
        }
        if(!ownerLive){Invalidate(key.slot,write);return;}
        auto& state=slots[key.slot];state.identity=key;state.table=table;
        state.result=state.tracker.Observe(*table,key,frame.animation,frame.previous);
        bool ok=true;
        for(auto& binding:bindings)if(binding.slot==key.slot && binding.actor==key.actor && binding.palette==key.palette) {
            const bool valid=binding.character==frame.character && binding.table==table && live(binding.headerAddress,binding.header);
            for(auto& gate:binding.gates)ok=Gate(gate,valid && state.result.status==MoveRouteStatus::Active && gate.route==state.result.route,write) && ok;
        }
        if(!ok)Invalidate(key.slot,write);
    }
};
}
