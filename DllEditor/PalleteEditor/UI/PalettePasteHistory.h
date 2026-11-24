#pragma once
#include <cstdint>
#include <vector>
#include <algorithm>
namespace PalettePasteHistory {
struct Entry {
    int slot,palette,start;
    std::vector<uint32_t> before,after;
    unsigned long long generation=0;
    bool CanUndo(int currentSlot,int currentPalette,unsigned long long currentGeneration,const std::vector<uint32_t>& colors) const {
        if(slot!=currentSlot || palette!=currentPalette || generation!=currentGeneration || start<0 || before.size()!=after.size() || after.empty())return false;
        const auto offset=size_t(start);
        return offset<=colors.size() && after.size()<=colors.size()-offset && std::equal(after.begin(),after.end(),colors.begin()+offset);
    }
};
}
