#pragma once
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>
#include <stdexcept>

namespace PaletteData {
// Explicit mappings from data01/move-families.json, researched build 26588330.
// The original two Beowulf mappings passed user gameplay checks. Roster additions
// are authored-data mappings; shared timed phases require synchronized context.
struct CharacterMove {
    std::string id,character,label,foitSha256;
    std::vector<std::pair<int,int>> ranges;
    int animationCount=0;
    std::string category;
    bool contextual=false;
};
inline const std::vector<CharacterMove>& CharacterMoves() {
    static const std::vector<CharacterMove> moves={
#include "CharacterMoveDefinitions.inl"
#include "ContextMoveDefinitions.inl"
    };
    return moves;
}
inline const CharacterMove* FindCharacterMove(const std::string& id) {
    static const auto index=[] {
        std::unordered_map<std::string,const CharacterMove*> result;
        for(const auto& move:CharacterMoves())if(!result.emplace(move.id,&move).second)throw std::logic_error("Duplicate compiled move ID.");
        return result;
    }();
    const auto found=index.find(id);return found==index.end()?nullptr:found->second;
}
inline bool ExclusiveCharacterMoves(const std::string& left,const std::string& right) {
    const auto a=FindCharacterMove(left),b=FindCharacterMove(right);
    if(!a || !b || a->character!=b->character)return false;
    if(a->contextual && b->contextual && a->id!=b->id)return true;
    for(auto x:a->ranges)for(auto y:b->ranges)if(x.first<=y.second && y.first<=x.second)return false;
    return true;
}
}
