#pragma once
#include "PaletteTextureMatch.h"
#include <algorithm>
namespace PaletteTextureMatch {
class VisibilityHistory {
    struct Entry {Rows colors;std::uint64_t frame;};
    std::vector<Entry> entries;
    std::uint64_t frame=0;
public:
    void Clear(){entries.clear();}
    void Advance(){++frame;entries.erase(std::remove_if(entries.begin(),entries.end(),[&](const auto& entry){return frame-entry.frame>2;}),entries.end());}
    void Record(Rows colors) {
        if(colors.empty())return;
        for(auto& entry:entries)if(entry.colors==colors){entry.frame=frame;return;}
        if(entries.size()>=24)entries.erase(entries.begin());
        entries.push_back({std::move(colors),frame});
    }
    bool Contains(const Rows& expected) const {
        if(expected.empty())return false;
        for(const auto& entry:entries) {
            if(frame-entry.frame>2 || entry.colors.size()<expected.size())continue;
            bool same=true;
            for(size_t row=0;same && row<expected.size();++row)
                for(int x=0;x<16;++x)if(!Close(entry.colors[row][x],expected[row][x])){same=false;break;}
            if(same)return true;
        }
        return false;
    }
};
}
