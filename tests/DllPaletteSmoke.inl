// Included inside AdvancedPalette. Only enabled in explicit validation builds.
void RunDllSmoke() {
    using namespace OverlayEditor;
    auto c=Model(0);auto baseline=CapturePalette(c);nlohmann::json report;
    auto restore=[&](){ApplyPalette(c,baseline);};
    try {
        auto data=AppearanceData(c);CheckAppearanceBand(data,13);CheckAppearanceBand(data,14);
        auto expected=baseline;
        expected.colors[data.groups[13].start]^=0x00010101;
        expected.overlay=PaletteData::Overlay{"Fire",{13},1,0};
        expected.rainbow=PaletteData::Rainbow{{14},6,12};
        expected.colorLinks=PaletteData::ColorLinks{{{3,4}},255};
        expected.transformations={PaletteData::Transformation{{{4,3}},255,0,"SUPER",""}};
        ApplyPalette(c,expected);
        auto path=std::filesystem::path("dll-smoke-palette.pal");
        PaletteData::AtomicWrite(path,PaletteData::EncodePal(CapturePalette(c)));
        restore();ApplyPalette(c,PaletteData::ReadFile(path));
        if(PaletteData::Encode(CapturePalette(c))!=PaletteData::Encode(expected))throw std::runtime_error("DLL effect round-trip mismatch.");
        auto invalid=PaletteData::EncodePal(expected);invalid.pop_back();
        bool rejected=false;try {PaletteData::DecodePal(invalid);}catch(...){rejected=true;}
        if(!rejected)throw std::runtime_error("Truncated file accepted.");
        auto legacy=PaletteData::DecodePal(PaletteData::EncodePal(expected,true));
        if(!legacy.legacy || legacy.colors!=expected.colors)throw std::runtime_error("DLL legacy export mismatch.");
        restore();
        if(PaletteData::Encode(CapturePalette(c))!=PaletteData::Encode(baseline))throw std::runtime_error("DLL baseline restoration mismatch.");
        report={{"passed",true},{"restored",true},{"renderThread",GetCurrentThreadId()},
            {"fullEffectsRoundTrip",true},{"legacyCompatibility",true},{"invalidFileRejected",true}};
    } catch(const std::exception& e) {
        bool restored=false;try{restore();restored=PaletteData::Encode(CapturePalette(c))==PaletteData::Encode(baseline);}catch(...){}
        report={{"passed",false},{"restored",restored},{"error",e.what()}};
    }
    PaletteData::AtomicWriteText("dll-smoke-result.json",report.dump(2));
}
