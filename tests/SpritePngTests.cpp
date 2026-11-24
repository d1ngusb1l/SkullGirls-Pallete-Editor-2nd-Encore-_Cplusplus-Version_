#include "pch.h"
#include "Files/GroupJSONFiles.h"
#include "UI/SpriteViewer.h"
#include <iostream>
#include <stdexcept>
void Check(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
std::vector<uint32_t> Decode(const std::vector<unsigned char>& bytes,unsigned& width,unsigned& height) {
 SpritePng::Apartment apartment;Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
 SpritePng::Require(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(factory.GetAddressOf())),"Decode factory");
 Microsoft::WRL::ComPtr<IWICStream> stream;SpritePng::Require(factory->CreateStream(stream.GetAddressOf()),"Decode stream");
 SpritePng::Require(stream->InitializeFromMemory(const_cast<BYTE*>(bytes.data()),DWORD(bytes.size())),"Decode memory");
 Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;SpritePng::Require(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnLoad,decoder.GetAddressOf()),"Decode PNG");
 Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;SpritePng::Require(decoder->GetFrame(0,frame.GetAddressOf()),"Decode frame");
 SpritePng::Require(frame->GetSize(&width,&height),"Decode dimensions");
 Microsoft::WRL::ComPtr<IWICFormatConverter> converter;SpritePng::Require(factory->CreateFormatConverter(converter.GetAddressOf()),"Decode converter");
 SpritePng::Require(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom),"Decode BGRA");
 std::vector<uint32_t> pixels(size_t(width)*height);SpritePng::Require(converter->CopyPixels(nullptr,width*4,UINT(pixels.size()*4),reinterpret_cast<BYTE*>(pixels.data())),"Decode pixels");return pixels;
}
std::vector<unsigned char> Read(const std::filesystem::path& path){std::ifstream input(path,std::ios::binary);return {std::istreambuf_iterator<char>(input),{}};}
int main(int argc,char** argv){try {
 Check(argc==2 || argc==3,"Supply output directory and optional local asset directory");const auto output=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(output);
 SpritePreviewPixel transparent(true);Check(transparent.PackTransparent()==0,"Empty background not transparent");
 transparent.Blend(0x80ff0000,false);Check(transparent.PackTransparent()==0x80ff0000,"Straight alpha/color lost");
 transparent.Blend(0x800000ff,false);auto blend=transparent.PackTransparent();Check((blend>>24)==192,"Layer alpha composition");
 Check((blend&255)>=169 && (blend&255)<=171 && ((blend>>16)&255)>=84 && ((blend>>16)&255)<=86,"Layer colors composed incorrectly");
 SpriteViewer::SpriteFrameAsset frame;frame.id=7;frame.width=4;frame.height=2;
 SpriteViewer::PixelLayer body;body.width=4;body.height=2;body.pixels={0,31,0, 0,31,1, 0,31,2, 0,31,1, 0,31,0, 0,31,0, 0,31,0, 0,31,0};frame.layers.push_back(body);
 SpriteViewer::PixelLayer foreground;foreground.offsetX=3;foreground.width=1;foreground.height=1;foreground.pixels={0,31,2};frame.layers.push_back(foreground);
 SpriteViewer::frames={frame};SpriteViewer::fallbackSprite=7;SpriteViewer::sizes={1,1,1};SpriteViewer::snapshotCharacter="Fixture";
 SpriteViewer::original={0,0x80ff0000,0xff00ff00};SpriteViewer::edited={0,0x800000ff,0xff00ff00};
 const auto original=SpriteViewer::original,edited=SpriteViewer::edited;SpriteViewer::showOriginal=true;SpriteViewer::highlight=true;SpriteViewer::zoom=3;
 SpriteViewer::CapturePngExport();const auto captured=SpriteViewer::pendingPngExport.value();
 Check(captured.width==4 && captured.height==2,"Export was scaled by zoom");
 Check(captured.pixels[0]==0 && captured.pixels[1]==0x80ff0000 && captured.pixels[2]==0xff00ff00 && captured.pixels[3]==0xff00ff00,"Original export pixels/focus/layer order");
 SpriteViewer::showOriginal=false;SpriteViewer::CapturePngExport();Check(SpriteViewer::pendingPngExport->pixels[1]==0x800000ff,"Edited toggle ignored");
 Check(SpriteViewer::original==original && SpriteViewer::edited==edited && SpriteViewer::highlight && SpriteViewer::zoom==3,"Export changed preview state");
 auto bytes=SpritePng::Encode(captured.width,captured.height,captured.pixels);Check(bytes.size()>8 && bytes[0]==137 && bytes[1]=='P' && bytes[2]=='N' && bytes[3]=='G',"Not a PNG file");
 unsigned width=0,height=0;Check(Decode(bytes,width,height)==captured.pixels && width==4 && height==2,"PNG round trip changed RGBA");
 const auto file=output/L"transparent-frame-\u03a9.png";
 SpriteViewer::SavePngExport(file);Check(!SpriteViewer::pendingPngExport,"Captured pixels not released after save");
 Check(SpriteViewer::exportStatus.find("Saved transparent PNG:")==0,"Save reported failure");
 const auto saved=Read(file);auto decoded=Decode(saved,width,height);Check(decoded[1]==0x800000ff,"Saved wrong toggle frame");
 bool rejected=false;try{SpritePng::Save(file,0,2,captured.pixels);}catch(...){rejected=true;}Check(rejected && Read(file)==saved,"Invalid export overwrote prior PNG");
 rejected=false;try{SpritePng::Save(output/("missing-"+std::to_string(GetTickCount64()))/"frame.png",4,2,captured.pixels);}catch(...){rejected=true;}Check(rejected,"Missing export directory accepted");
 if(argc==3) {
  const auto assetDirectory=std::filesystem::absolute(argv[2]);std::filesystem::current_path(assetDirectory);
  SpriteViewer::ResetAsset();SpriteViewer::Load("Peacock");Check(SpriteViewer::error.empty(),"Could not load local Peacock preview");
  const auto& rows=SpriteViewer::defaultRows;const size_t count=rows.size()/64;
  SpriteViewer::sizes.assign(count,16);SpriteViewer::original.assign(count*16,0);
  for(size_t band=0;band<count;++band)for(size_t i=0;i<16;++i) {
   const auto off=band*64+(15-i)*4;uint32_t color=0;for(int ch=0;ch<4;++ch)color|=uint32_t(rows[off+ch])<<(8*ch);
   SpriteViewer::original[band*16+i]=color;
  }
  SpriteViewer::edited=SpriteViewer::original;SpriteViewer::snapshotCharacter="Peacock";SpriteViewer::showOriginal=true;
  SpriteViewer::CapturePngExport();const auto actual=SpriteViewer::pendingPngExport.value();
  size_t clear=0,visible=0;for(auto color:actual.pixels){if(color>>24)++visible;else ++clear;}
  Check(clear>0 && visible>0,"Local sprite has no transparent border or visible pixels");
  const auto spriteFile=output/"Peacock-fixed-transparent.png";SpriteViewer::SavePngExport(spriteFile);
  Check(Decode(Read(spriteFile),width,height)==actual.pixels,"Local sprite PNG round trip changed pixels");
  std::cout<<"Local Peacock default-palette export: "<<actual.width<<"x"<<actual.height<<", "<<clear<<" transparent pixels.\n";
 }
 std::cout<<"Sprite PNG tests passed: transparent/partial alpha, layer composition, Original/Edited, no focus/zoom, Unicode save, WIC round trip, atomic preservation, state isolation.\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
