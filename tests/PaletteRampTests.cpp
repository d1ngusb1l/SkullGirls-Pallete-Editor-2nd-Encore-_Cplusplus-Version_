#include "../DllEditor/PalleteEditor/UI/PaletteRamp.h"
#include <iostream>
#include <stdexcept>
void Check(bool ok){if(!ok)throw std::runtime_error("Ramp check failed");}
int main(){
 using namespace PaletteRamp;
 // Sample the RGB cube, including black, whites, neutrals and arbitrary alpha.
 for(int r=0;r<=255;r+=17)for(int g=0;g<=255;g+=17)for(int b=0;b<=255;b+=17){
   uint32_t c=0x7d000000u|(r<<16)|(g<<8)|b;
   Check(Encode(Decode(c),c)==c);
   Check(Adjust(c,0,0,0)==c);
   auto shifted=Adjust(c,.37f,0,0);
   Check((shifted&0xff000000u)==(c&0xff000000u));
   Check(std::abs(Decode(shifted).v-Decode(c).v)<.005f);
   Check(Adjust(c,1,0,0)==c);
 }
 Check(std::abs(HueDelta(.98f,.02f)-.04f)<.0001f);
 Check(std::abs(HueDelta(.02f,.98f)+.04f)<.0001f);
 for(float amount:{-.8f,-.2f,0.f,.2f,.8f}) {
   float previous=-1;
   for(int n=0;n<=100;++n){float v=Shift(n/100.f,amount);Check(v>=previous);previous=v;}
 }
 for(float from:{0.f,.2f,.5f,.9f,1.f})for(float to:{0.f,.1f,.7f,1.f})
   Check(std::abs(Shift(from,ShiftTo(from,to))-to)<.0001f);
 // Every swatch in a ramp keeps its own alpha, relative hue spacing and value.
 uint32_t ramp[]={0x11eebbaau,0x55996655u,0x99443322u};
 for(auto c:ramp){auto before=Decode(c),after=Decode(Adjust(c,.2f,0,0));
   Check(std::abs(HueDelta(before.h,after.h)-.2f)<.01f);
   Check(std::abs(before.v-after.v)<.005f);
 }
 std::cout<<"Palette ramp tests passed (4096 RGB samples, hue wrap, alpha, shading order, linked endpoints).\n";
}
