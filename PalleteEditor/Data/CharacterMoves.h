#pragma once
#include <string>
#include <utility>
#include <vector>

namespace PaletteData {
// Explicit mappings from data01/move-families.json, researched build 26588330.
// These initial mappings await user-controlled gameplay validation.
struct CharacterMove {
    std::string id,character,label,foitSha256;
    std::vector<std::pair<int,int>> ranges;
};
inline const std::vector<CharacterMove>& CharacterMoves() {
    static const std::vector<CharacterMove> moves={
        {"beowulf/chair-toss","Beowulf","Chair Toss (all strengths)","5302729ad0f055f1c89008c189f69ce2ce167f508ba77e619a889bbe59540411",{{191,198}}},
        {"beowulf/organic-arm","Beowulf","Gigantic Arm","5302729ad0f055f1c89008c189f69ce2ce167f508ba77e619a889bbe59540411",{{234,236}}}
    };
    return moves;
}
inline const CharacterMove* FindCharacterMove(const std::string& id) {
    for(const auto& move:CharacterMoves())if(move.id==id)return &move;
    return nullptr;
}
inline bool ExclusiveCharacterMoves(const std::string& left,const std::string& right) {
    const auto a=FindCharacterMove(left),b=FindCharacterMove(right);
    if(!a || !b || a->character!=b->character)return false;
    for(auto x:a->ranges)for(auto y:b->ranges)if(x.first<=y.second && y.first<=x.second)return false;
    return true;
}
}
