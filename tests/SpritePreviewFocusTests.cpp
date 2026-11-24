#include "../DllEditor/PalleteEditor/UI/SpritePreviewPixel.h"
#include <stdexcept>
#include <iostream>
void Check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int main(){
 // Selected opaque and translucent colors are byte-identical to unhighlighted rendering.
 for(int alpha:{1,32,128,254,255})for(uint32_t color:{0x007e4c2au,0x0000ff00u,0x00ffffffu,0u}) {
  SpritePreviewPixel pixel;pixel.Blend((uint32_t(alpha)<<24)|color,true);
  Check(pixel.Pack(true,true)==pixel.Pack(false,true),"Selected color changed");
 }
 SpritePreviewPixel background;Check(background.Pack(true,true)==background.Pack(false,true),"Background changed");
 SpritePreviewPixel other;other.Blend(0xff90b060,false);
 for(int ch=0;ch<3;++ch)Check(((other.Pack(true,true)>>(ch*8))&255)<((other.Pack(false,true)>>(ch*8))&255),"Other region not dimmed");
 Check(other.Pack(true,false)==other.Pack(false,false),"Empty selection dims sprite");
 SpritePreviewPixel covered;covered.Blend(0xff7e4c2a,true);covered.Blend(0xff90b060,false);
 Check(covered.Pack(true,true)==other.Pack(true,true),"Occluded selected region stayed bright");
 SpritePreviewPixel translucent;translucent.Blend(0xff7e4c2a,true);translucent.Blend(0x8090b060,false);
 Check(translucent.Pack(true,true)==translucent.Pack(false,true),"Visible selected layer changed");
 std::cout<<"Preview pixel tests passed: selected colors byte-identical, other regions dimmed, neutral background unchanged, occlusion respected.\n";
}
