#pragma once
#include <cstdint>
#include <algorithm>
#include <cmath>
// Shared CPU compositor used for each uploaded preview texture pixel.
struct SpritePreviewPixel {
    float rgb[3]={0.20f,0.20f,0.20f};
    float opacity=1;
    explicit SpritePreviewPixel(bool transparent=false) {if(transparent){rgb[0]=rgb[1]=rgb[2]=0;opacity=0;}}
    float selectedCoverage=0;
    bool covered=false;
    void Blend(uint32_t color,bool selected) {
        const float alpha=((color>>24)&255)/255.f;
        opacity=opacity*(1-alpha)+alpha;
        if(alpha>0)covered=true;
        selectedCoverage=selectedCoverage*(1-alpha)+(selected?alpha:0);
        for(int ch=0;ch<3;++ch) {
            const float value=((color>>(ch*8))&255)/255.f;
            rgb[ch]=rgb[ch]*(1-alpha)+value*alpha;
        }
    }
    // PNG stores straight alpha; RGB accumulation is premultiplied over zero.
    uint32_t PackTransparent() const {
        if(opacity<=0)return 0;
        auto byte=[](float value){return uint32_t(std::clamp(std::lround(value*255),0l,255l));};
        return byte(rgb[0]/opacity)|byte(rgb[1]/opacity)<<8|byte(rgb[2]/opacity)<<16|byte(opacity)<<24;
    }
    uint32_t Pack(bool highlight,bool hasSelection) const {
        const float scale=highlight && hasSelection && covered && selectedCoverage<=0?0.25f:1.f;
        return 0xff000000u|uint32_t(rgb[0]*scale*255)|uint32_t(rgb[1]*scale*255)<<8|uint32_t(rgb[2]*scale*255)<<16;
    }
};
