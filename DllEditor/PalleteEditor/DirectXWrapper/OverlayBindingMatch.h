#pragma once
#include <cstdint>
#include <map>
#include <set>
namespace OverlayBindingMatch {
using Id=std::uintptr_t;
using Snapshot=std::map<Id,Id>;
// A zero overlay means the binding was absent or inconsistent in the frame.
inline std::set<Id> Changed(const Snapshot& marker,const Snapshot& primary) {
    std::set<Id> result;
    for(const auto& [feature,overlay]:primary) {
        auto old=marker.find(feature);
        if(overlay && old!=marker.end() && old->second && old->second!=overlay)result.insert(feature);
    }
    return result;
}
inline void Confirm(std::set<Id>& candidates,const Snapshot& expected,const Snapshot& observed) {
    for(auto i=candidates.begin();i!=candidates.end();) {
        auto a=expected.find(*i),b=observed.find(*i);
        if(a==expected.end() || b==observed.end() || !a->second || a->second!=b->second)i=candidates.erase(i);
        else ++i;
    }
}
}
