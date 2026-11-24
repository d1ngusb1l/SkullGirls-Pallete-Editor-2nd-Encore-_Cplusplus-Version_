#pragma once
#include <algorithm>
#include <stdexcept>
namespace SpritePreviewGeometry {
struct Bounds {
    int left=0,top=0,right=0,bottom=0;
    bool empty=true;
    void Add(int x,int y,unsigned width,unsigned height) {
        if(x< -8192 || x>8192 || y< -8192 || y>8192 || !width || !height || width>2048 || height>2048)
            throw std::runtime_error("Sprite layer exceeds preview geometry bounds.");
        int r=x+static_cast<int>(width),b=y+static_cast<int>(height);
        if(empty){left=x;top=y;right=r;bottom=b;empty=false;}
        else {left=(std::min)(left,x);top=(std::min)(top,y);right=(std::max)(right,r);bottom=(std::max)(bottom,b);}
        if(right-left>2048 || bottom-top>2048)throw std::runtime_error("Combined sprite layers exceed preview dimensions.");
    }
    unsigned OffsetX(int x) const {if(empty || x<left || x>=right)throw std::runtime_error("Invalid sprite X offset.");return static_cast<unsigned>(x-left);}
    unsigned OffsetY(int y) const {if(empty || y<top || y>=bottom)throw std::runtime_error("Invalid sprite Y offset.");return static_cast<unsigned>(y-top);}
};
}
