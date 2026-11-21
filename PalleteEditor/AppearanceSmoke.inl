// Live integration checks; all changes use the same Apply/Reset paths as the UI.
inline nlohmann::json RunAppearanceSmoke(Character& c) {
    auto data=AppearanceData(c);
    CheckAppearanceBand(data,13); CheckAppearanceBand(data,14);
    DWORD address=ColorLinkAddress(c,data),outer=Word(data.palette+0x20);
    auto nativeHeaders=Read(outer,12*Word(data.palette));
    auto colors=Read(data.colors,4*data.colorCount);
    auto nativeLighting=Read(Word(data.palette+0x5c),4*data.groups.size());
    auto hue=Read(RainbowAddress(c),16);
    auto overlays=Read(Word(data.palette+0x14)+12*c.Current_Pallete_Num,12);
    opacityBands={13};
    for(int alpha:{0,64,128,255}) {
        opacityAlpha=alpha; ApplyOpacity(c); Sleep(100);
        auto expected=colors; auto group=data.groups[13];
        for(int color=group.start;color<group.start+group.count;color++) expected[4*color+3]=(unsigned char)alpha;
        if(Read(data.colors,expected.size())!=expected) throw std::runtime_error("Opacity changed RGB or an unselected group.");
    }
    opacityBands={14}; opacityAlpha=64; ApplyOpacity(c);
    ResetOpacity(c);
    if(Read(data.colors,colors.size())!=colors) throw std::runtime_error("Opacity reset did not restore all edited groups.");
    colorLinks={{13,14}}; linkShadeMask=255; ApplyColorLinks(c); Sleep(500); Tick();
    auto firstHeader=Read(address,12);
    DWORD begin=Word(address),end=Word(address+4);
    bool found=false;
    for(DWORD record=begin;record<end;record+=44) if(Word(record)==13) {
        if(found || Word(record+4)!=14 || Read(record+8,1)[0]!=255 || Word(record+12)!=0 || Word(record+40)!=15)
            throw std::runtime_error("Unconditional color-link record mismatch.");
        found=true;
    }
    if(!found) throw std::runtime_error("Color link missing.");
    // Reapply with a partial shade mask: must replace, not duplicate, our link.
    linkShadeMask=0xa5; ApplyColorLinks(c); Sleep(500); Tick();
    begin=Word(address); end=Word(address+4); int matches=0;
    for(DWORD record=begin;record<end;record+=44) if(Word(record)==13) {
        ++matches; if(Read(record+8,1)[0]!=0xa5) throw std::runtime_error("Shade mask mismatch.");
    }
    if(matches!=1 || Word(address+4)-Word(address)!=
       [&]() { DWORD a=0,b=0; memcpy(&a,firstHeader.data(),4); memcpy(&b,firstHeader.data()+4,4); return b-a; }())
        throw std::runtime_error("Repeated Apply duplicated native color links.");
    ResetColorLinks(c); Sleep(500); Tick();
    if(Read(outer,nativeHeaders.size())!=nativeHeaders) throw std::runtime_error("Color-link reset or palette isolation failed.");
    if(Read(data.colors,colors.size())!=colors || Read(Word(data.palette+0x5c),nativeLighting.size())!=nativeLighting ||
       Read(Word(data.palette+0x14)+12*c.Current_Pallete_Num,12)!=overlays)
        throw std::runtime_error("Appearance controls changed unrelated palette state.");
    auto hueAfter=Read(RainbowAddress(c),16); hueAfter[2]=hue[2]; hueAfter[3]=hue[3];
    if(hueAfter!=hue) throw std::runtime_error("Appearance controls changed rainbow settings.");
    opacityBands={13}; opacityAlpha=128; colorLinks={{13,14}}; linkShadeMask=255;
    auto preset=AppearancePreset(c); auto serialized=nlohmann::json::parse(preset.dump());
    opacityBands.clear(); colorLinks.clear(); opacityAlpha=64; linkShadeMask=1;
    LoadAppearancePreset(c,serialized);
    if(AppearancePreset(c)!=preset) throw std::runtime_error("Appearance preset round-trip failed.");
    auto invalid=preset; invalid["opacity"]=256; bool rejected=false;
    try { LoadAppearancePreset(c,invalid); } catch(...) { rejected=true; }
    if(!rejected || AppearancePreset(c)!=preset) throw std::runtime_error("Invalid preset changed control state.");
    invalid=preset; invalid["links"]=std::map<int,int>{{13,256}}; rejected=false;
    try { LoadAppearancePreset(c,invalid); } catch(...) { rejected=true; }
    if(!rejected || AppearancePreset(c)!=preset) throw std::runtime_error("Invalid color link was accepted.");
    if(Read(base+0x87dc4,1)[0]!=0x84) throw std::runtime_error("Shader refresh was not restored.");
    return {{"passed",true},{"character",c.Char_Name},{"palette",c.Current_Pallete_Num+1},
            {"opacityLevels",{0,64,128,255}},{"sourceBand",13},{"destinationBand",14},
            {"rgbAndOtherGroupsPreserved",true},{"sharedLightingUnchanged",true},
            {"repeatApplyAndShadeMask",true},{"presetRoundTripAndValidation",true},{"restored",true}};
}
