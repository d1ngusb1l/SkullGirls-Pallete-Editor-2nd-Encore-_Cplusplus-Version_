#pragma once
#include <cstdint>
// Shared CPU compositor used for each uploaded preview texture pixel.
struct SpritePreviewPixel {
    float rgb[3]={0.20f,0.20f,0.20f};
    float selectedCoverage=0;
    bool covered=false;
    void Blend(uint32_t color,bool selected) {
        const float alpha=((color>>24)&255)/255.f;
        if(alpha>0)covered=true;
        selectedCoverage=selectedCoverage*(1-alpha)+(selected?alpha:0);
        for(int ch=0;ch<3;++ch) {
            const float value=((color>>(ch*8))&255)/255.f;
            rgb[ch]=rgb[ch]*(1-alpha)+value*alpha;
        }
    }
    uint32_t Pack(bool highlight,bool hasSelection) const {
        const float scale=highlight && hasSelection && covered && selectedCoverage<=0?0.25f:1.f;
        return 0xff000000u|uint32_t(rgb[0]*scale*255)|uint32_t(rgb[1]*scale*255)<<8|uint32_t(rgb[2]*scale*255)<<16;
    }
};
