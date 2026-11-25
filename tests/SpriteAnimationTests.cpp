#include "pch.h"
#include "Files/GroupJSONFiles.h"
#include "UI/SpriteViewer.h"
#include <iostream>
#include <stdexcept>
void Check(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
void Raw(const std::filesystem::path& path,const std::vector<uint32_t>& pixels){std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(pixels.data()),std::streamsize(pixels.size()*4));}
int main(int argc,char** argv){try {
 Check(argc==2 || argc==3,"Supply export output directory and optional asset directory");
 const auto output=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(output);
 std::vector<uint32_t> palette={0};for(unsigned i=1;i<256;++i)palette.push_back(0xff000000u|((i*73&255)<<16)|((i*151&255)<<8)|(i*199&255));
 constexpr unsigned w=256,h=128;std::vector<SpriteAnimation::Frame> fixture(4);unsigned durations[]={1,2,3,90};uint32_t rng=0x12345678;
 for(size_t n=0;n<fixture.size();++n){fixture[n].ticks=durations[n];fixture[n].pixels.resize(w*h);
  for(auto& pixel:fixture[n].pixels){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;pixel=palette[(rng%8)?1+rng%255:0];if(n==1)pixel=0;if(n==3)pixel=(pixel&0xffffff)|((rng%4)*64u<<24);}
  Raw(output/("fixture-"+std::to_string(n)+".bgra"),fixture[n].pixels);
 }
 auto get=[&](size_t i){return fixture.at(i);};auto cancel=[](){};
 PaletteData::AtomicWrite(output/"fixture.apng",SpriteAnimation::APNG(w,h,fixture.size(),2,get,cancel));
 PaletteData::AtomicWrite(output/"fixture.gif",SpriteAnimation::GIF(w,h,fixture.size(),2,palette,get,cancel));
 PaletteData::AtomicWrite(output/"forever.gif",SpriteAnimation::GIF(w,h,1,0,palette,get,cancel));
 PaletteData::AtomicWrite(output/"once.gif",SpriteAnimation::GIF(w,h,1,1,palette,get,cancel));
 PaletteData::AtomicWrite(output/"forever.apng",SpriteAnimation::APNG(w,h,1,0,get,cancel));
 bool cancelled=false;try{SpriteAnimation::APNG(w,h,4,0,get,[](){throw std::runtime_error("cancel");});}catch(...){cancelled=true;}Check(cancelled,"Cancellation ignored");
 bool rejected=false;try{SpriteAnimation::GIF(0,h,4,0,palette,get,cancel);}catch(...){rejected=true;}Check(rejected,"Invalid dimensions accepted");
 rejected=false;try{SpriteAnimation::APNG(w,h,2001,0,get,cancel);}catch(...){rejected=true;}Check(rejected,"Unbounded frame count accepted");
 // Exercise palette quantization with more than 255 opaque colors.
 std::vector<uint32_t> many;for(unsigned i=0;i<1024;++i)many.push_back(0xff000000u|((i*19)&0xffffff));
 SpriteAnimation::Frame colorful{many,6};PaletteData::AtomicWrite(output/"quantized.gif",SpriteAnimation::GIF(32,32,1,1,many,[&](size_t){return colorful;},cancel));
 // A captured animation keeps frame geometry, timing and colors independent of the viewer.
 // Independent pane-local simulations perform one-hop full-band borrowing.
 {
  SpriteViewer::SpriteFrameAsset frame;frame.width=2;frame.height=1;
  SpriteViewer::PixelLayer pixels;pixels.width=2;pixels.height=1;pixels.pixels={0,31,1,0,31,3};frame.layers={pixels};
  const std::vector<int> layout={1,1,1,1,1};const std::vector<ImU32> colors={0,0xff0000ff,0xff00ff00,0xffff0000,0xffffffff};
  PaletteData::Transformation ra{{{1,2}},255,0,"ATTACK","","a","A",true};
  auto rb=ra;rb.id="b";rb.name="B";rb.links={{3,4}};rb.activationTag="JUMPING";
  SpriteViewer::simulatedRules={ra,rb};
  for(int active=0;active<4;++active){
   SpriteViewer::simulatedActive.clear();if(active&1)SpriteViewer::simulatedActive.insert("a");if(active&2)SpriteViewer::simulatedActive.insert("b");
   const auto image=SpriteViewer::RenderFrame(frame,colors,{},false,true,layout,SpriteViewer::SimulatedRedirects());
   Check(image[0]==colors[(active&1)?2:1] && image[1]==colors[(active&2)?4:3],"Local independent rule truth table failed");
  }
  SpriteViewer::simulatedRules[0].shadeMask=15;SpriteViewer::simulatedRules[1].enabled=false;
  Check(SpriteViewer::SimulatedRedirects().empty(),"Unsupported or disabled rule simulated");
  SpriteViewer::simulatedRules.clear();SpriteViewer::simulatedActive.clear();
 }
 SpriteViewer::SpriteFrameAsset a;a.id=7;a.width=2;a.height=1;a.originX=1;a.originY=1;
 SpriteViewer::PixelLayer layer;layer.width=2;layer.height=1;layer.pixels={0,31,1,0,31,0};a.layers={layer};
 auto b=a;b.id=8;b.originX=0;b.layers[0].pixels={0,31,0,0,31,1};
 SpriteViewer::frames={a,b};SpriteViewer::animations={{20,"Fixture",{{7,1,0,0},{7,4,0,0},{8,2,0,0}}}};
 SpriteViewer::selectedAnimation=0;SpriteViewer::frameIndex=1;SpriteViewer::snapshotCharacter="Fixture";
 SpriteViewer::sizes={1,1};SpriteViewer::original={0,0x80ff0000};SpriteViewer::edited={0,0x800000ff};SpriteViewer::showOriginal=true;SpriteViewer::highlight=true;SpriteViewer::zoom=3;
 auto capture=SpriteViewer::CaptureAnimationExport(false,0);
 const auto trimmed=SpriteViewer::CaptureAnimationExport(false,0,2,3);
 Check(trimmed.animation.frames.size()==2 && trimmed.animation.frames[0].duration==4 && trimmed.animation.frames[1].sprite==8,"Inclusive range lost frame timing or endpoints");
 const auto single=SpriteViewer::CaptureAnimationExport(true,1,3,3);
 Check(single.animation.frames.size()==1 && single.sprites.size()==1 && single.bounds.right-single.bounds.left==2,"Single-frame range included excluded sprites or bounds");
 Check(single.name.find("frames-3-3")!=std::string::npos,"Range missing from filename");
 auto rangeProgress=std::make_shared<SpriteViewer::AnimationProgress>();
 PaletteData::AtomicWrite(output/"trimmed.apng",SpriteViewer::EncodeAnimation(trimmed,rangeProgress));
 PaletteData::AtomicWrite(output/"single.gif",SpriteViewer::EncodeAnimation(single,rangeProgress));
 for(const auto limits: {std::pair<int,int>{0,2},{3,2},{1,4}}) {
  bool invalid=false;try{SpriteViewer::CaptureAnimationExport(false,0,limits.first,limits.second);}catch(const std::exception&){invalid=true;}
  Check(invalid,"Invalid export range accepted");
 }

 SpriteViewer::showOriginal=false;const auto editedCapture=SpriteViewer::CaptureAnimationExport(true,2);
 Check(editedCapture.colors[1]==0x800000ff && editedCapture.gif && editedCapture.plays==2,"Edited colors or export options were not captured");
 SpriteViewer::sizes={1,1,1};SpriteViewer::edited.push_back(0xff00ff00);
 PaletteData::Transformation simulation{{{1,2}},255,0,"ATTACK","","simulation","Local only",true};
 SpriteViewer::simulatedRules={simulation};SpriteViewer::simulatedActive={"simulation"};
 const auto simulationCapture=SpriteViewer::CaptureAnimationExport(false,0);
 SpriteViewer::simulatedRules.clear();SpriteViewer::simulatedActive.clear();
 Check(simulationCapture.redirects==std::map<int,int>{{1,2}} && capture.redirects.empty(),"Simulation was not independently snapshotted");
 SpriteViewer::sizes={1,1};SpriteViewer::edited.pop_back();
 SpriteViewer::showOriginal=true;
 Check(capture.bounds.right-capture.bounds.left==3 && capture.bounds.bottom-capture.bounds.top==1,"Shared origin canvas incorrect");
 Check(SpriteViewer::selectedAnimation==0 && SpriteViewer::frameIndex==1 && SpriteViewer::highlight && SpriteViewer::zoom==3,"Capture changed viewer state");
 SpriteViewer::original.clear();SpriteViewer::edited.clear();SpriteViewer::sizes.clear();SpriteViewer::frames.clear();SpriteViewer::animations.clear();SpriteViewer::selectedAnimation=-1;
 PaletteData::AtomicWrite(output/"simulation-snapshot.apng",SpriteViewer::EncodeAnimation(simulationCapture,std::make_shared<SpriteViewer::AnimationProgress>()));
 SpriteViewer::pendingAnimationExport=capture;SpriteViewer::StartAnimationExport(output/"snapshot.apng");
 for(unsigned i=0;SpriteViewer::AnimationBusy() && i<30000;++i){SpriteViewer::PollAnimationExport();Sleep(1);}
 Check(!SpriteViewer::AnimationBusy() && SpriteViewer::animationStatus.find("Saved APNG:")==0,"Background export failed");
 auto progress=std::make_shared<SpriteViewer::AnimationProgress>();progress->cancel=true;
 cancelled=false;try{SpriteViewer::EncodeAnimation(capture,progress);}catch(...){cancelled=true;}Check(cancelled,"Snapshot cancellation ignored");

 // An outdated standalone bundle must point users to regeneration, not a generic format error.
 std::filesystem::create_directory(output/"old-bundle");
 {std::ofstream old(output/"old-bundle"/"sprite-preview.bin",std::ios::binary);const char magic[8]={'S','G','P','O','S','E','5',0};old.write(magic,8);const unsigned fields[4]={64,1,1,1};old.write(reinterpret_cast<const char*>(fields),sizeof(fields));}
 const auto priorDirectory=std::filesystem::current_path();std::filesystem::current_path(output/"old-bundle");
 SpriteViewer::ResetAsset();SpriteViewer::Load("Cerebella");
 Check(SpriteViewer::error.find("regenerate")!=std::string::npos,"Outdated bundle did not explain regeneration");
 std::filesystem::current_path(priorDirectory);
 if(argc==3) {
  std::filesystem::current_path(std::filesystem::absolute(argv[2]));SpriteViewer::ResetAsset();SpriteViewer::Load("Cerebella");Check(SpriteViewer::error.empty(),"Load Cerebella failed");
  const auto& rows=SpriteViewer::defaultRows;SpriteViewer::sizes.assign(rows.size()/64,16);SpriteViewer::original.assign(rows.size()/4,0);
  for(size_t band=0;band<SpriteViewer::sizes.size();++band)for(size_t i=0;i<16;++i){const auto off=band*64+(15-i)*4;uint32_t color=0;for(int ch=0;ch<4;++ch)color|=uint32_t(rows[off+ch])<<(8*ch);SpriteViewer::original[band*16+i]=color;}
  SpriteViewer::edited=SpriteViewer::original;SpriteViewer::snapshotCharacter="Cerebella";SpriteViewer::showOriginal=true;
  for(const auto& selection:std::vector<std::pair<unsigned,std::string>>{{21,"crouch"},{20,"idle"},{103,"standing-heavy-punch"}}) {
   auto move=std::find_if(SpriteViewer::animations.begin(),SpriteViewer::animations.end(),[&](const auto& a){return a.id==selection.first;});Check(move!=SpriteViewer::animations.end(),"Sample timeline missing");SpriteViewer::selectedAnimation=int(move-SpriteViewer::animations.begin());
   auto actual=SpriteViewer::CaptureAnimationExport(false,0);auto state=std::make_shared<SpriteViewer::AnimationProgress>();state->total=actual.animation.frames.size();
   const auto stem="Cerebella-"+selection.second;
   PaletteData::AtomicWrite(output/(stem+".apng"),SpriteViewer::EncodeAnimation(actual,state));actual.gif=true;PaletteData::AtomicWrite(output/(stem+".gif"),SpriteViewer::EncodeAnimation(actual,state));
   nlohmann::json meta={{"name",move->name},{"frames",actual.animation.frames.size()},{"width",actual.bounds.right-actual.bounds.left},{"height",actual.bounds.bottom-actual.bounds.top},{"ticks",nlohmann::json::array()},{"sprites",nlohmann::json::array()}};
   for(const auto& entry:actual.animation.frames){meta["ticks"].push_back(entry.duration);meta["sprites"].push_back(entry.sprite);}
   PaletteData::AtomicWriteText(output/(stem+".json"),meta.dump());
   std::cout<<"Exported Cerebella "<<move->name<<" ["<<move->id<<"]: "<<actual.animation.frames.size()<<" frames in both formats.\n";
  }
 }
 std::cout<<"Animation export fixtures passed: bounds, authored timing, snapshot isolation, background completion, cancellation, limits and quantization.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
