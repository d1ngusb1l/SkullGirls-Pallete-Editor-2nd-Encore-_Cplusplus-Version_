#include "pch.h"
#include "Files/GroupJSONFiles.h"
#include "UI/SpriteViewer.h"
#include <cassert>
int main(int argc,char** argv){
 if(argc!=2){std::cerr<<"Usage: SpritePreviewAssetTests ASSET_DIRECTORY\n";return 2;}
 std::filesystem::current_path(argv[1]);
 const auto fixture=std::filesystem::current_path()/("preview-read-boundary-"+std::to_string(GetCurrentProcessId())+".bin");
 {std::ofstream f(fixture,std::ios::binary);f<<"123456789";}
 {SpriteViewer::PreviewInput input(fixture,std::ios::binary);input.limit=8;char bytes[9];
  bool rejected=false;try{input.read(bytes,9);}catch(...){rejected=true;}assert(rejected);}
 std::filesystem::remove(fixture);
 const char* names[]={"Filia","Cerebella","Peacock","Parasoul","MsFortune","Painwheel","Valentine","Double","Squigly","BigBand","Eliza","Fukua","Beowulf","RoboFortune","Annie","Umbrella","BlackDahlia","Marie"};
 for(const auto* name:names){
  SpriteViewer::Load(name);
  if(!SpriteViewer::error.empty()){std::cerr<<name<<": "<<SpriteViewer::error<<"\n";return 1;}
  assert(!SpriteViewer::frames.empty() && !SpriteViewer::animations.empty());
  for(const auto& frame:SpriteViewer::frames)for(const auto& layer:frame.layers){assert(layer.offsetX+layer.width<=frame.width);assert(layer.offsetY+layer.height<=frame.height);}
  SpriteViewer::original={0,0xff123456};SpriteViewer::edited={0,0xffabcdef};SpriteViewer::zoom=0.65f;
  const auto savedOriginal=SpriteViewer::original,savedEdited=SpriteViewer::edited;
  for(int selection:{-1,0,static_cast<int>(SpriteViewer::animations.size()-1)}){
   SpriteViewer::selectedAnimation=selection;SpriteViewer::frameIndex=0;
   const auto* frame=SpriteViewer::ActiveFrame();assert(frame);
   assert(SpriteViewer::original==savedOriginal && SpriteViewer::edited==savedEdited && SpriteViewer::zoom==0.65f);
   for(const auto& layer:frame->layers)assert(layer.pixels.size()==size_t(layer.width)*layer.height*3);
  }
  if(std::string(name)=="Cerebella") {
   SpriteViewer::selectedAnimation=-1;const auto fallback=SpriteViewer::fallbackSprite;
   std::set<unsigned> idleBands;
   for(const auto& layer:SpriteViewer::ActiveFrame()->layers)for(size_t i=2;i<layer.pixels.size();i+=3)idleBands.insert(layer.pixels[i]);
   bool found=false;
   for(const auto& a:SpriteViewer::animations) {
    for(size_t t=0;t<a.frames.size() && !found;++t) {
     SpriteViewer::fallbackSprite=a.frames[t].sprite;
     const auto* f=SpriteViewer::ActiveFrame();
     for(const auto& layer:f->layers)for(size_t i=2;i<layer.pixels.size();i+=3)if(layer.pixels[i] && !idleBands.count(layer.pixels[i])) {
      std::cout<<"Cerebella idle-hidden band "<<unsigned(layer.pixels[i])<<" appears in "<<a.name<<" ["<<a.id<<"] frame "<<t+1<<" sprite "<<f->id<<"\n";found=true;break;
     }
    }
    if(found)break;
   }
   assert(found);SpriteViewer::fallbackSprite=fallback;
  }
  std::cout<<name<<": "<<SpriteViewer::frames.size()<<" sprites, "<<SpriteViewer::animations.size()<<" animations checked; canvas "<<SpriteViewer::canvasWidth<<"x"<<SpriteViewer::canvasHeight<<" anchor "<<SpriteViewer::canvasLeft<<","<<SpriteViewer::canvasTop<<"\n";
 }
 SpriteViewer::ResetAsset();
}

