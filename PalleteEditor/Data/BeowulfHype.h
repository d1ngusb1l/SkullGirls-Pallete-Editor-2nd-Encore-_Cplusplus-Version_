#pragma once
#include <cmath>
#include <array>
#include <stdexcept>
namespace PaletteData {
// Build 26588330: ordinary kind-1 variable 0x64, float32 entity+0x190.
// Completed amounts only; a persistent mode after spending is not represented.
inline int BeowulfHypeLevel(float amount) {
    if(!std::isfinite(amount) || amount<0 || amount>3)return -1;
    return static_cast<int>(amount);
}
struct HypeComparison {int opcode;float rhs;};
inline std::array<HypeComparison,2> HypeComparisons(int level) {
    if(level<0 || level>3)throw std::runtime_error("Unsupported Beowulf Hype level.");
    // Native 0048ce00: >= is 0x13, < is 0x10, <= is 0x14.
    return {{{0x13,static_cast<float>(level)},{level==3?0x14:0x10,static_cast<float>(level==3?3:level+1)}}};
}
}
