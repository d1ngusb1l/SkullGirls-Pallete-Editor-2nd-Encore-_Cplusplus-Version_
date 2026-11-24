#include "../DllEditor/PalleteEditor/UI/SpritePreviewGeometry.h"
#include <cassert>
int main(){
 using SpritePreviewGeometry::Bounds;
 // Nonzero authored origin (100,200), first layer at (80,160).
 Bounds b;b.Add(80-100,160-200,32,32);
 assert(b.OffsetX(-20)==0 && b.OffsetY(-40)==0);
 assert(-b.left==20 && -b.top==40);
 b.Add(96-100,176-200,32,32);
 assert(b.OffsetX(-4)==16 && b.OffsetY(-24)==16);
 assert(b.right-b.left==48 && b.bottom-b.top==48);
 Bounds positive;positive.Add(100,200,16,16);assert(positive.OffsetX(100)==0 && -positive.left==-100);
 bool rejected=false;try{b.OffsetX(-21);}catch(...){rejected=true;}assert(rejected);
 rejected=false;try{b.Add(8193,0,16,16);}catch(...){rejected=true;}assert(rejected);
}
