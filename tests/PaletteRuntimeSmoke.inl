// Opt-in runner integration test. Tests real persistence APIs without dialogs.
inline nlohmann::json RunPalettePersistenceSmoke(Character& c) {
    auto baseline=CapturePalette(c); auto context=AppearanceData(c);
    auto root=std::filesystem::absolute("persistence-test-files"); std::filesystem::create_directories(root);
    auto path=root/L"combined-palette-\u03A9.pal",legacyPath=root/"legacy-colors.pal",badPath=root/"invalid.pal";
    if(!PalleteFile::SavePath(c,root/"baseline.pal")) throw std::runtime_error(PalleteFile::Status);
    auto beforeHeaders=Read(Word(context.palette+0x20),12*Word(context.palette));
    auto oldAuto=AutoPallete::Auto_Pals; auto oldCharacters=PalEdit::Character_Vector;
    bool oldOpen=PalEdit::bGameOpenned,oldMatch=PalEdit::bMatchStarted;
    auto restore=[&]() {
        ApplyPalette(c,baseline); AutoPallete::Auto_Pals=oldAuto; AutoPallete::ResetSession();
        PalEdit::Character_Vector=oldCharacters; PalEdit::bGameOpenned=oldOpen; PalEdit::bMatchStarted=oldMatch;
    };
    try {
        CheckAppearanceBand(context,13); CheckAppearanceBand(context,14); CheckAppearanceBand(context,3); CheckAppearanceBand(context,4);
        auto expected=baseline;
        expected.colors[context.groups[13].start]^=0x00010101;
        for(int shade=0;shade<context.groups[13].count;shade++) {
            auto& color=expected.colors[context.groups[13].start+shade]; color=(color&0x00ffffff)|0x80000000;
        }
        expected.lineColor=0xff102030; expected.shadow1=0xff405060; expected.shadow2=0xff708090;
        expected.overlay=PaletteData::Overlay{"Fire",{13},1,0}; expected.rainbow=PaletteData::Rainbow{{14},6,12};
        expected.secondOverlay=PaletteData::Overlay{"Water",{14},1,0};
        expected.colorLinks=PaletteData::ColorLinks{{{3,4}},255};
        expected.transformation=PaletteData::Transformation{{{4,3}},0xa5,0,"SUPER",""};
        ApplyPalette(c,expected,true);
        const auto overlayAddress=Word(context.palette+0x14)+12*c.Current_Pallete_Num;
        if(Word(overlayAddress+4)-Word(overlayAddress)!=120)
            throw std::runtime_error("Standalone preview should attach exactly one native overlay.");
        if(PaletteData::Encode(CapturePalette(c))!=PaletteData::Encode(expected)) throw std::runtime_error("Applied palette capture differs from saved settings.");
        // Unapplied controls must not leak into a full palette save.
        selected={1}; texture="Water"; rainbowBands={1}; rainbowSpeed=99;
        if(!PalleteFile::SavePath(c,path)) throw std::runtime_error(PalleteFile::Status);
        auto saved=PaletteData::ReadFile(path);
        if(PaletteData::Encode(saved)!=PaletteData::Encode(expected)) throw std::runtime_error("Saving captured draft controls instead of applied effects.");
        ApplyPalette(c,baseline);
        if(!PalleteFile::LoadPath(c,path) || PaletteData::Encode(CapturePalette(c))!=PaletteData::Encode(expected)) throw std::runtime_error("Full .pal load did not restore all effects.");
        if(!PalleteFile::SavePath(c,legacyPath,true)) throw std::runtime_error(PalleteFile::Status);
        auto legacy=PaletteData::ReadFile(legacyPath);
        if(!legacy.legacy || legacy.colors!=expected.colors || legacy.legacyHueSpeed!=6 || legacy.legacyHueInterval!=12) throw std::runtime_error("Legacy export changed its prefix or hue fields.");
        ApplyPalette(c,baseline);
        if(!PalleteFile::LoadPath(c,legacyPath) || CapturePalette(c).colors!=expected.colors || !CapturePalette(c).rainbow) throw std::runtime_error("Plain .pal import failed.");
        auto unchanged=PaletteData::Encode(CapturePalette(c));
        auto bytes=PaletteData::EncodePal(expected); bytes.pop_back(); PaletteData::AtomicWrite(badPath,bytes);
        if(PalleteFile::LoadPath(c,badPath) || PaletteData::Encode(CapturePalette(c))!=unchanged) throw std::runtime_error("Invalid file changed live palette data.");
        auto wrong=expected; wrong.character=c.Char_Name=="Annie"?"Valentine":"Annie";
        PaletteData::AtomicWrite(badPath,PaletteData::EncodePal(wrong));
        if(PalleteFile::LoadPath(c,badPath) || PaletteData::Encode(CapturePalette(c))!=unchanged) throw std::runtime_error("Wrong-character file changed live palette data.");
        ApplyPalette(c,baseline);
        PalEdit::bGameOpenned=true; PalEdit::bMatchStarted=true; PalEdit::Character_Vector={c};
        AutoPallete::Auto_Pals={{c.Char_Name,c.Current_Pallete_Num,PaletteData::PathText(path),true,{}}};
        AutoPallete::ResetSession(); AutoPallete::Tick();
        if(PaletteData::Encode(CapturePalette(c))!=PaletteData::Encode(expected)) throw std::runtime_error("Auto-load did not apply colors and effects.");
        DWORD edit=context.colors+4*context.groups[13].start; auto manual=Read(edit,4); manual[0]^=7; PausedWrite(edit,manual);
        Sleep(300); AutoPallete::Tick();
        if(Read(edit,4)!=manual) throw std::runtime_error("Auto-load continuously overwrote a manual edit.");
        AutoPallete::ResetSession(); AutoPallete::Tick();
        if(PaletteData::Encode(CapturePalette(c))!=PaletteData::Encode(expected)) throw std::runtime_error("New-session auto-load did not reapply the file.");
        // Isolate configuration I/O from both user settings and the normal build.
        auto working=std::filesystem::current_path(); auto settingsFolder=root/"settings"; std::filesystem::create_directories(settingsFolder);
        PaletteData::AtomicWriteText(settingsFolder/"PaletteEditorConfig.json","{\"AutoLoadPals\":[]}");
        std::filesystem::current_path(settingsFolder);
        try {
            config::init(); AutoPallete::Assign(c.Char_Name,c.Current_Pallete_Num,PaletteData::PathText(path));
            if(!config::Status.empty()) throw std::runtime_error(config::Status);
            AutoPallete::Auto_Pals.clear(); AutoPallete::load();
            if(AutoPallete::Auto_Pals.size()!=1 || AutoPallete::Auto_Pals[0].PalPath!=PaletteData::PathText(path) || AutoPallete::Auto_Pals[0].PalNum!=c.Current_Pallete_Num)
                throw std::runtime_error("Auto-load assignment was not persisted.");
        } catch(...) { std::filesystem::current_path(working); throw; }
        std::filesystem::current_path(working);
        restore(); Sleep(500); Tick();
        if(PaletteData::Encode(CapturePalette(c))!=PaletteData::Encode(baseline) || Read(Word(context.palette+0x20),beforeHeaders.size())!=beforeHeaders || Read(base+0x87dc4,1)[0]!=0x84)
            throw std::runtime_error("Persistence test did not restore its baseline.");
        return {{"passed",true},{"character",c.Char_Name},{"palette",c.Current_Pallete_Num+1},
            {"fullEffectsRoundTrip",true},{"secondOverlayRoundTrip",true},{"singleNativePreview",true},{"appliedSettingsOnly",true},{"legacyImportAndExport",true},
            {"invalidFilesLeaveGameUnchanged",true},{"autoLoadAppliesAllEffects",true},{"autoLoadPreservesManualEdits",true},
            {"sessionReapplication",true},{"assignmentPersistence",true},{"restored",true}};
    } catch(...) { try { restore(); } catch(...) {} throw; }
}
