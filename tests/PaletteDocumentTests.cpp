#include "Data/PaletteDocument.h"
#include "Data/AtomicFile.h"
#include <iostream>
#include <functional>

namespace {
int checks=0;
void Require(bool condition,const char* message) { ++checks; if(!condition) throw std::runtime_error(message); }
void Reject(const std::function<void()>& action,const char* message) {
    bool rejected=false; try { action(); } catch(const std::exception&) { rejected=true; } Require(rejected,message);
}
PaletteData::Document Example() {
    PaletteData::Document d; d.character="Annie"; d.groupSizes={1,2,3};
    d.colors={0,0xffffffff,0x80ff0080,0x010000ff,0xff123456,0x7fabcdef};
    d.lineColor=0xff000001; d.shadow1=0x11223344; d.shadow2=0xffabcdef;
    d.overlay=PaletteData::Overlay{"Fire",{1},1,2};
    d.rainbow=PaletteData::Rainbow{{1,2},4,8};
    d.colorLinks=PaletteData::ColorLinks{{{1,2}},0xa5};
    d.transformation=PaletteData::Transformation{{{2,1}},0x81,0,"SUPER",""};
    return d;
}
}
int main() {
    try {
        using namespace PaletteData;
        auto d=Example(); auto json=Encode(d); auto extended=EncodePal(d);
        auto dual=d;
        dual.secondOverlay=Overlay{"Water",{2},0,1};
        const auto dualBytes=EncodePal(dual);
        Require(Encode(DecodePal(dualBytes))==Encode(dual),"Two overlays did not round-trip.");
        Require(DecodeLegacyPrefix(dualBytes).colors==d.colors,"Two overlays changed the legacy prefix.");
        auto overlap=dual;
        overlap.secondOverlay->bands={1};
        Reject([&]{EncodePal(overlap);},"Overlapping overlay groups accepted.");
        auto missing=dual;
        missing.overlay.reset();
        Reject([&]{EncodePal(missing);},"Second overlay without a primary accepted.");
        Require(Encode(DecodePal(extended))==json,"Extended .pal did not round-trip all colors and effects.");
        auto oldReader=DecodeLegacyPrefix(extended);
        Require(oldReader.colors==d.colors && oldReader.lineColor==d.lineColor && oldReader.shadow1==d.shadow1 && oldReader.shadow2==d.shadow2,
            "Legacy reader could not read colors from extended .pal.");
        Require(oldReader.legacyHueSpeed==4 && oldReader.legacyHueInterval==8,"Legacy hue bytes were discarded.");
        auto plain=EncodePal(d,true); auto legacy=DecodePal(plain);
        Require(legacy.legacy && legacy.colors==d.colors && !legacy.overlay && !legacy.transformation,"Plain legacy palette was not supported.");
        Require(plain.size()==22+4*(d.colors.size()-1)+12,"Legacy binary layout changed.");
        auto corrupt=extended; corrupt.pop_back(); Reject([&]{DecodePal(corrupt);},"Truncated extension accepted.");
        corrupt=extended; corrupt[22]^=1; Reject([&]{DecodePal(corrupt);},"Disagreeing color prefix accepted.");
        corrupt=extended; corrupt[plain.size()+6]='2'; Reject([&]{DecodePal(corrupt);},"Unknown extension version accepted.");
        corrupt=extended; for(size_t i=plain.size()+8;i<plain.size()+12;i++) corrupt[i]=255;
        Reject([&]{DecodePal(corrupt);},"Oversized extension accepted.");
        corrupt=plain; corrupt.resize(20); Reject([&]{DecodePal(corrupt);},"Truncated legacy file accepted.");
        corrupt=plain; corrupt[16]=0; corrupt[17]=corrupt[18]=corrupt[19]=0;
        Reject([&]{DecodePal(corrupt);},"Zero color-count legacy file accepted.");
        corrupt=plain; corrupt.push_back(1); Reject([&]{DecodePal(corrupt);},"Unknown legacy suffix silently ignored.");
        auto bad=json; bad["version"]=2; Reject([&]{Decode(bad);},"Unknown schema version accepted.");
        bad=json; bad["colors"][1]=-1; Reject([&]{Decode(bad);},"Negative packed color accepted.");
        bad=json; bad["colors"][1]=4294967296ULL; Reject([&]{Decode(bad);},"Overflowing packed color accepted.");
        bad=json; bad["effects"]["rainbow"]["speed"]=1.5; Reject([&]{Decode(bad);},"Fractional timing accepted.");
        bad=json; bad["effects"]["rainbow"]["bands"]={1,1}; Reject([&]{Decode(bad);},"Duplicate effect groups accepted.");
        bad=json; bad["effects"]["overlay"]["bands"]={3}; Reject([&]{Decode(bad);},"Absent group accepted.");
        bad=json; bad["groupSizes"]={1,2,4}; Reject([&]{Decode(bad);},"Incorrect group layout accepted.");
        bad=json; bad["effects"]["colorLinks"]["links"]={{1,2},{1,2}}; Reject([&]{Decode(bad);},"Duplicate link sources accepted.");
        bad=json; bad["effects"]["transformation"]["activationTag"]="AIR"; Reject([&]{Decode(bad);},"Unsupported tag accepted.");
        bad=json; bad["effects"]["transformation"]["activationTag"]=""; Reject([&]{Decode(bad);},"Automatic transformation without a trigger accepted.");
        bad=json; bad["effects"]["transformation"]["legacyTrigger"]="annie-supers"; Reject([&]{Decode(bad);},"Ambiguous trigger accepted.");
        auto native=d; native.overlay.reset(); native.rainbow.reset(); native.colorLinks.reset(); native.transformation.reset();
        Require(Encode(DecodePal(EncodePal(native)))==Encode(native),"Native effect inheritance did not round-trip.");
        auto folder=std::filesystem::current_path()/"document-test-files";
        std::filesystem::create_directories(folder);
        auto path=folder/L"palette-unicode-\u03A9.pal";
        AtomicWrite(path,extended); Require(Encode(ReadFile(path))==json,"Unicode-path palette read/write failed.");
        AtomicWrite(path,plain); Require(ReadFile(path).legacy,"Atomic replacement failed.");
        Reject([&]{AtomicWrite(folder,extended);},"Replacing a directory should fail.");
        Require(ReadFile(path).legacy,"Failed save damaged a previously saved palette.");
        std::cout<<"Passed "<<checks<<" palette format, legacy compatibility, validation and atomic I/O checks.\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<"\n"; return 1; }
}
