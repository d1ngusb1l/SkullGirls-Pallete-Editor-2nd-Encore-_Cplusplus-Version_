#pragma once
#include "PaletteDocument.h"
#include <array>
#include <cstring>
namespace PaletteData {
struct RedirectRecord {
    std::array<unsigned char,44> bytes{};
    int conditionRule=-1;
    int conditionRange=0;
};
inline uint32_t RedirectWord(const unsigned char* bytes) {
    return uint32_t(bytes[0]) | uint32_t(bytes[1])<<8 | uint32_t(bytes[2])<<16 | uint32_t(bytes[3])<<24;
}
inline void RedirectPut(unsigned char* bytes,uint32_t value) {
    for(int i=0;i<4;++i)bytes[i]=static_cast<unsigned char>(value>>(8*i));
}
// Native records first, then static links, then disjoint conditional rules.
// A static assignment replaces only its shades, preserving all other native bytes.
inline std::vector<RedirectRecord> BuildRedirectPlan(const std::vector<unsigned char>& native,const Links& links,int mask,const std::vector<Transformation>& rules) {
    ValidateRuleConflicts(rules);
    if(native.size()%44 || native.size()>44*1024)throw std::runtime_error("Unsupported native color-link vector.");
    if(mask<1 || mask>255)throw std::runtime_error("Invalid static shade mask.");
    std::vector<RedirectRecord> result;
    for(size_t i=0;i<native.size();i+=44) {
        RedirectRecord record;std::copy_n(native.begin()+i,44,record.bytes.begin());
        if(links.count((int)RedirectWord(record.bytes.data())))record.bytes[8]&=static_cast<unsigned char>(~mask);
        if(record.bytes[8])result.push_back(record);
    }
    auto append=[&](int source,int destination,int shades,int condition,int range=0) {
        RedirectRecord record;RedirectPut(record.bytes.data(),source);RedirectPut(record.bytes.data()+4,destination);
        record.bytes[8]=static_cast<unsigned char>(shades);RedirectPut(record.bytes.data()+40,15);record.conditionRule=condition;
        record.conditionRange=range;result.push_back(record);
    };
    for(const auto& link:links)append(link.first,link.second,mask,-1);
    for(int i=0;i<(int)rules.size();++i) {
        const auto& rule=rules[i];if(!rule.enabled || rule.previewMode==1)continue;
        const auto move=FindCharacterMove(rule.moveId);
        const int rangeCount=move?(int)move->ranges.size():1;
        for(int range=0;range<rangeCount;++range)
            for(const auto& link:rule.links)append(link.first,link.second,rule.shadeMask,rule.previewMode==0?i:-1,range);
    }
    if(result.size()>2048)throw std::runtime_error("Composed native color-link table is too large.");
    return result;
}
}
