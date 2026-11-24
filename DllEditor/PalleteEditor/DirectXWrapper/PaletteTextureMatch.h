#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include <stdexcept>
namespace PaletteTextureMatch {
using Rows=std::vector<std::array<std::uint32_t,16>>;
inline Rows Build(const std::vector<std::uint32_t>& colors,const std::vector<int>& sizes) {
    Rows rows;size_t start=0;
    for(int count:sizes) {
        if(count<0 || start+count>colors.size() || rows.size()>=256)throw std::runtime_error("Invalid palette gradient layout.");
        std::array<std::uint32_t,16> row{};
        for(int x=0;count && x<16;++x) {
            float index=(count-1)*x/15.0f;int lo=int(index),hi=(lo+1<count?lo+1:lo);float t=index-lo;
            for(int ch=0;ch<3;++ch){float a=(colors[start+count-1-lo]>>(ch*8))&255,b=(colors[start+count-1-hi]>>(ch*8))&255;row[x]|=std::uint32_t(a+(b-a)*t)<<(ch*8);}
        }
        rows.push_back(row);start+=count;
    }
    if(start!=colors.size() || rows.empty())throw std::runtime_error("Palette gradient layout is incomplete.");
    return rows;
}
inline bool Close(std::uint32_t actual,std::uint32_t expected) {
    for(int c=0;c<3;++c){int d=int((actual>>(c*8))&255)-int((expected>>(c*8))&255);if(d< -2 || d>2)return false;}
    return true;
}
}
