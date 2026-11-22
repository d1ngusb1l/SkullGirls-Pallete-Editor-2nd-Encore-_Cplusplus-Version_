// Included inside OverlayEditor after all effect backends.
inline void SyncPaletteModel(Character& c) {
    auto data=AppearanceData(c); auto bytes=Read(data.colors,4*data.colorCount);
    c.Num_Of_Color=data.colorCount; c.Character_Colors.resize(data.colorCount);
    memcpy(c.Character_Colors.data(),bytes.data(),bytes.size());
    c.LineColor=(__int32)Word(Word(data.palette+0x38)+4*c.Current_Pallete_Num);
    DWORD shadow=Word(Word(data.palette+0xc)+4*c.Current_Pallete_Num);
    c.SuperShadowColor1=(__int32)Word(shadow); c.SuperShadowColor2=(__int32)Word(shadow+4);
}
inline PaletteData::Document CapturePalette(const Character& c) {
    auto data=AppearanceData(c); PaletteData::Document d; d.character=c.Char_Name;
    auto bytes=Read(data.colors,4*data.colorCount); d.colors.resize(data.colorCount); memcpy(d.colors.data(),bytes.data(),bytes.size());
    d.lineColor=Word(Word(data.palette+0x38)+4*c.Current_Pallete_Num);
    DWORD shadow=Word(Word(data.palette+0xc)+4*c.Current_Pallete_Num); d.shadow1=Word(shadow); d.shadow2=Word(shadow+4);
    for(auto& group:data.groups) d.groupSizes.push_back(group.count);
    auto overlay=overlayAssignments.find(Word(data.palette+0x14)+12*c.Current_Pallete_Num);
    if(overlay!=overlayAssignments.end()) {
        auto& e=overlay->second;
        auto readOverlay=[](const nlohmann::json& value) {
            return PaletteData::Overlay{value.at("texture"),value.at("bands").get<std::set<int>>(),value.at("lighting"),value.at("tintBand")};
        };
        d.overlay=readOverlay(e);
        if(e.contains("secondOverlay"))d.secondOverlay=readOverlay(e.at("secondOverlay"));
    }
    auto rainbow=rainbowAssignments.find(RainbowAddress(c));
    if(rainbow!=rainbowAssignments.end()) {
        auto& e=rainbow->second;
        d.rainbow=PaletteData::Rainbow{e.at("bands").get<std::set<int>>(),e.at("speed"),e.at("interval")};
    }
    auto assignment=colorAssignments.find(ColorLinkAddress(c,data));
    if(assignment!=colorAssignments.end()) {
        auto& a=assignment->second;
        if(!a.staticLinks.empty()) d.colorLinks=PaletteData::ColorLinks{a.staticLinks,a.staticMask};
        if(a.hasTransformation) d.transformation=PaletteData::Transformation{a.transformation.links,a.transformation.shadeMask,
            (int)a.transformation.mode,a.transformation.activationTag,a.transformation.activationTag.empty()?TransformTrigger(c):""};
    }
    PaletteData::Encode(d); return d;
}
inline TransformSettings DocumentTransformation(const Character& c,const AppearanceContext& data,const PaletteData::Transformation& value) {
    TransformSettings settings; settings.links=value.links; settings.shadeMask=value.shadeMask;
    settings.mode=(TransformMode)value.previewMode; settings.activationTag=value.activationTag;
    if(value.activationTag.empty() && value.legacyTrigger!=TransformTrigger(c)) throw std::runtime_error("Legacy transformation trigger does not match this character/palette.");
    ValidateTransformation(c,data,settings); return settings;
}
inline void ValidatePalette(const Character& c,const PaletteData::Document& document,bool allowTwo=false) {
    if(document.secondOverlay && !allowTwo)throw std::runtime_error("This editor build cannot yet apply two overlay textures.");
    auto data=AppearanceData(c);
    if(document.character!=c.Char_Name) throw std::runtime_error("Palette file belongs to a different character.");
    if(document.colors.size()!=(size_t)data.colorCount) throw std::runtime_error("Palette file color count does not match this character.");
    if(!document.legacy) {
        PaletteData::Encode(document);
        std::vector<int> sizes; for(auto& group:data.groups) sizes.push_back(group.count);
        if(document.groupSizes!=sizes) throw std::runtime_error("Palette file group layout does not match this game build.");
    }
    for(const auto* effect:{&document.overlay,&document.secondOverlay}) {
        if(!*effect)continue;
        const auto& overlay=**effect;
        Catalog();
        if(!templates.count(overlay.texture))throw std::runtime_error("Could not load overlay texture '"+overlay.texture+"': no supported preset metadata is available.");
        for(int band:overlay.bands)CheckAppearanceBand(data,band);
        if(overlay.tintBand)CheckAppearanceBand(data,overlay.tintBand);
    }
    if(document.rainbow) for(int band:document.rainbow->bands) CheckAppearanceBand(data,band);
    if(document.colorLinks) ValidateLinks(data,document.colorLinks->links,document.colorLinks->shadeMask);
    if(document.transformation) DocumentTransformation(c,data,*document.transformation);
    // All document validation precedes resource loading or palette mutation.
    if(document.overlay) ResolveOverlayTexture(document.overlay->texture);
    if(document.secondOverlay) ResolveOverlayTexture(document.secondOverlay->texture);
}
inline void ApplyPalette(Character& c,const PaletteData::Document& document,bool allowTwo=false) {
    ValidatePalette(c,document,allowTwo); auto data=AppearanceData(c);
    DWORD line=Word(data.palette+0x38)+4*c.Current_Pallete_Num,shadow=Word(Word(data.palette+0xc)+4*c.Current_Pallete_Num);
    DWORD overlay=Word(data.palette+0x14)+12*c.Current_Pallete_Num,hue=RainbowAddress(c),links=ColorLinkAddress(c,data);
    std::map<DWORD,Bytes> rollback={{data.colors,Read(data.colors,4*data.colorCount)},{line,Read(line,4)},
        {shadow,Read(shadow,8)},{overlay,Read(overlay,12)},{hue,Read(hue,16)},{links,Read(links,12)}};
    auto savedOriginals=originals; auto savedOverlays=overlayAssignments; auto savedRainbows=rainbowAssignments; auto savedLinks=colorAssignments;
    auto draftBands=selected,draftRainbowBands=rainbowBands; auto draftTexture=texture;
    int draftLighting=lighting,draftTint=tint,draftSpeed=rainbowSpeed,draftInterval=rainbowInterval;
    auto restoreDrafts=[&]() { selected=draftBands; rainbowBands=draftRainbowBands; texture=draftTexture; lighting=draftLighting; tint=draftTint; rainbowSpeed=draftSpeed; rainbowInterval=draftInterval; };
    Original owner{data.actor,data.palette,0,c.ID,{}};
    try {
        Bytes colors(4*data.colorCount); memcpy(colors.data(),document.colors.data(),colors.size());
        Bytes lineBytes(4),shadowBytes(8); Put(lineBytes,0,document.lineColor); Put(shadowBytes,0,document.shadow1); Put(shadowBytes,4,document.shadow2);
        AppearanceTransaction(owner,[&]() {
            Write(data.colors,colors); Write(line,lineBytes); Write(shadow,shadowBytes);
            // Loaded alpha becomes part of the saved colors, rather than an
            // opacity-preview backup that would overwrite it on editor exit.
            for(auto it=originals.begin();it!=originals.end();) {
                if(it->second.header.size()==1 && it->first>=data.colors && it->first<data.colors+4*data.colorCount && (it->first-data.colors)%4==3) it=originals.erase(it); else ++it;
            }
        });
        if(!document.legacy) {
            auto assignment=CurrentColorAssignment(c,data); assignment.staticLinks.clear(); assignment.hasTransformation=false;
            if(document.colorLinks) { assignment.staticLinks=document.colorLinks->links; assignment.staticMask=document.colorLinks->shadeMask; }
            if(document.transformation) { assignment.hasTransformation=true; assignment.transformation=DocumentTransformation(c,data,*document.transformation); }
            CommitColorAssignment(c,data,assignment);
            if(document.overlay) { selected=document.overlay->bands; texture=document.overlay->texture; lighting=document.overlay->lighting; tint=document.overlay->tintBand; Apply(c,document.secondOverlay?&*document.secondOverlay:nullptr); }
            else Remove(overlay);
            if(document.rainbow) { rainbowBands=document.rainbow->bands; rainbowSpeed=document.rainbow->speed; rainbowInterval=document.rainbow->interval; ApplyRainbow(c); }
            else Remove(hue);
        } else if(document.legacyHueSpeed) {
            rainbowBands.clear(); for(auto& group:data.groups) if(group.band && group.count) rainbowBands.insert(group.band);
            rainbowSpeed=document.legacyHueSpeed; rainbowInterval=document.legacyHueInterval; ApplyRainbow(c);
        }
        Refresh(); SyncPaletteModel(c); restoreDrafts(); selectionKey.clear();
    } catch(const std::exception& error) {
        auto message=std::string(error.what());
        try {
            AppearanceTransaction(owner,[&]() { for(auto& value:rollback) Write(value.first,value.second); });
            originals=savedOriginals; overlayAssignments=savedOverlays; rainbowAssignments=savedRainbows; colorAssignments=savedLinks;
            Refresh(); SyncPaletteModel(c);
        } catch(...) { restoreDrafts(); throw std::runtime_error(message+" Palette restoration also failed; the match may have ended."); }
        restoreDrafts(); throw std::runtime_error(message+" Previous palette restored.");
    }
}
