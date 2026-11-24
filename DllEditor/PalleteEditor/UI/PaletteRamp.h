#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace PaletteRamp {
struct HSV { float h, s, v; };
inline float Wrap(float h) { return h-std::floor(h); }
inline HSV Decode(uint32_t color) {
    const float r=((color>>16)&255)/255.f,g=((color>>8)&255)/255.f,b=(color&255)/255.f;
    const float hi=std::max({r,g,b}),lo=std::min({r,g,b}),d=hi-lo;
    float h=0;
    if(d>0) {
        if(hi==r)h=(g-b)/d;else if(hi==g)h=2+(b-r)/d;else h=4+(r-g)/d;
        h=Wrap(h/6);
    }
    return {h,hi>0?d/hi:0,hi};
}
inline uint32_t Encode(HSV c,uint32_t alpha) {
    c.h=Wrap(c.h);c.s=std::clamp(c.s,0.f,1.f);c.v=std::clamp(c.v,0.f,1.f);
    const float k=c.h*6,f=k-std::floor(k),p=c.v*(1-c.s),q=c.v*(1-c.s*f),t=c.v*(1-c.s*(1-f));
    float r,g,b;
    switch(int(k)%6) {
    case 0:r=c.v;g=t;b=p;break;case 1:r=q;g=c.v;b=p;break;
    case 2:r=p;g=c.v;b=t;break;case 3:r=p;g=q;b=c.v;break;
    case 4:r=t;g=p;b=c.v;break;default:r=c.v;g=p;b=q;break;
    }
    return (alpha&0xff000000u)|(uint32_t(std::lround(r*255))<<16)|(uint32_t(std::lround(g*255))<<8)|uint32_t(std::lround(b*255));
}
// Monotonic endpoint adjustment preserves the ramp's value order.
inline float Shift(float value,float amount) {
    return std::clamp(amount>=0?value+(1-value)*amount:value*(1+amount),0.f,1.f);
}
inline uint32_t Adjust(uint32_t color,float hue,float saturation,float brightness) {
    if(hue==0 && saturation==0 && brightness==0)return color;
    auto c=Decode(color);c.h=Wrap(c.h+hue);c.s=Shift(c.s,saturation);c.v=Shift(c.v,brightness);
    return Encode(c,color);
}
inline float HueDelta(float from,float to) { float d=to-from;return d-std::floor(d+0.5f); }
inline float ShiftTo(float from,float to) {
    if(to>=from)return from<1?(to-from)/(1-from):0;
    return from>0?(to-from)/from:0;
}
}
