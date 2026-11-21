// Included inside OverlayEditor. Native palette alpha is ordinary 0..255;
// Palette_00487510 packs alpha >> 1 and the shared band lighting flag for GPU use.
struct AppearanceGroup { int band, start, count; std::string label; };
struct AppearanceContext {
    DWORD actor, palette, colors;
    int colorCount;
    std::vector<AppearanceGroup> groups;
};
inline std::set<int> opacityBands;
inline int opacityAlpha=128, linkSource=1, linkDestination=2, linkShadeMask=255;
inline std::map<int,int> colorLinks;
inline std::string appearanceStatus;

inline AppearanceContext AppearanceData(const Character& c) {
    DWORD root=Word(base+0x85ea4c);
    if(!root || Word(root+0x168)!=4 || c.ID<0 || c.ID>=6) throw std::runtime_error("An active match is required.");
    AppearanceContext data{}; data.actor=Word(root+0x5e8+4*c.ID);
    if(!data.actor) throw std::runtime_error("Character is no longer loaded.");
    auto name=Read(data.actor+0x310,32); auto terminator=std::find(name.begin(),name.end(),0);
    if(terminator==name.end() || std::string(name.begin(),terminator)!=c.Char_Name) throw std::runtime_error("Character changed; select it again in the current match.");
    data.palette=Word(data.actor+0x328);
    int palettes=(int)Word(data.palette);
    if(c.Current_Pallete_Num<0 || c.Current_Pallete_Num>=palettes || Read(data.actor+0x330,1)[0]!=c.Current_Pallete_Num)
        throw std::runtime_error("Palette selection changed; select the current palette again.");
    data.colorCount=(int)Word(data.palette+4);
    if(data.colorCount<1 || data.colorCount>4096) throw std::runtime_error("Unsupported palette color count.");
    data.colors=Word(Word(data.palette+8)+4*c.Current_Pallete_Num);
    DWORD begin=Word(data.palette+0x5c),end=Word(data.palette+0x60);
    if(!begin || end<begin || (end-begin)%4 || end-begin>1024) throw std::runtime_error("Unsupported palette group table.");
    auto metadata=Read(begin,end-begin);
    auto named=GroupColorGroup::characterGroups.find(c.Char_Name);
    int color=0;
    for(int band=0;band<(int)metadata.size()/4;band++) {
        int count=metadata[4*band]; std::string label="Band "+std::to_string(band);
        if(named!=GroupColorGroup::characterGroups.end()) for(auto& group:named->second)
            if(group.startIndex==color && group.count==count) { label=group.groupName; break; }
        data.groups.push_back({band,color,count,label}); color+=count;
    }
    if(color!=data.colorCount || !data.colors) throw std::runtime_error("Palette groups do not match its color data.");
    return data;
}
inline void CheckAppearanceBand(const AppearanceContext& data,int band) {
    if(band<1 || band>=(int)data.groups.size() || !data.groups[band].count)
        throw std::runtime_error("Select a valid nontransparent group.");
}
inline void AppearanceTransaction(const Original& owner,const std::function<void()>& update) {
    EffectUpdateGuard updateGuard(process);
    try {
        if(!Valid(owner)) throw std::runtime_error("Match changed; retry in the new match.");
        update();
    } catch(...) { throw; }

}
inline void ApplyOpacity(const Character& c) {
    auto data=AppearanceData(c);
    if(opacityBands.empty() || opacityAlpha<0 || opacityAlpha>255) throw std::runtime_error("Select groups and an opacity from 0 to 255.");
    std::vector<DWORD> addresses;
    for(int band:opacityBands) {
        CheckAppearanceBand(data,band); const auto& group=data.groups[band];
        for(int color=group.start;color<group.start+group.count;color++) addresses.push_back(data.colors+4*color+3);
    }
    Original owner{data.actor,data.palette,0,c.ID,{}};
    AppearanceTransaction(owner,[&]() {
        for(DWORD address:addresses) {
            if(!originals.count(address)) originals.emplace(address,Original{data.actor,data.palette,address,c.ID,Read(address,1)});
            Write(address,Bytes{(unsigned char)opacityAlpha});
        }
    });
    Refresh(); appearanceStatus="Opacity applied to selected groups on this palette.";
}
inline void ResetOpacity(const Character& c) {
    auto data=AppearanceData(c);
    Original owner{data.actor,data.palette,0,c.ID,{}};
    AppearanceTransaction(owner,[&]() {
        for(auto item=originals.begin();item!=originals.end();) {
            const auto& original=item->second; DWORD address=item->first;
            if(original.actor==data.actor && original.palette==data.palette && original.header.size()==1 &&
               address>=data.colors && address<data.colors+4*data.colorCount && (address-data.colors)%4==3) {
                Write(address,original.header); item=originals.erase(item);
            } else ++item;
        }
    });
    Refresh(); appearanceStatus="Original opacity restored for this palette; RGB colors preserved.";
}
inline DWORD ColorLinkAddress(const Character& c,const AppearanceContext& data) {
    DWORD begin=Word(data.palette+0x20),end=Word(data.palette+0x24),address=begin+12*c.Current_Pallete_Num;
    if(!begin || address+12>end) throw std::runtime_error("Palette has no color-link table.");
    return address;
}
#include "NativeColorLinks.inl"
inline void SyncAppearanceColors(Character& c) {
    auto data=AppearanceData(c); auto colors=Read(data.colors,4*data.colorCount);
    c.Character_Colors.resize(data.colorCount); memcpy(c.Character_Colors.data(),colors.data(),colors.size());
}
inline nlohmann::json AppearancePreset(const Character& c) {
    return {{"version",1},{"character",c.Char_Name},{"opacityBands",opacityBands},{"opacity",opacityAlpha},
            {"links",colorLinks},{"shadeMask",linkShadeMask}};
}
inline void LoadAppearancePreset(const Character& c,const nlohmann::json& j) {
    auto data=AppearanceData(c);
    if(j.at("version")!=1 || j.at("character")!=c.Char_Name) throw std::runtime_error("Preset version or character does not match.");
    auto bands=j.at("opacityBands").get<std::set<int>>(); auto links=j.at("links").get<std::map<int,int>>();
    int alpha=j.at("opacity"),mask=j.at("shadeMask");
    if(alpha<0 || alpha>255 || mask<1 || mask>255) throw std::runtime_error("Preset settings out of range.");
    for(int band:bands) CheckAppearanceBand(data,band);
    for(auto& rule:links) { CheckAppearanceBand(data,rule.first); CheckAppearanceBand(data,rule.second); if(rule.first==rule.second) throw std::runtime_error("Invalid self-link."); }
    opacityBands=bands; colorLinks=links; opacityAlpha=alpha; linkShadeMask=mask;
    appearanceStatus="Preset loaded. Use Apply opacity and/or Apply color links to preview.";
}
inline void AppearanceGroupCombo(const char* label,int& value,const AppearanceContext& data) {
    if(value<1 || value>=(int)data.groups.size()) value=1;
    if(data.groups.size()<2) return;
    if(ImGui::BeginCombo(label,data.groups[value].label.c_str())) {
        for(auto& group:data.groups) if(group.band && group.count && ImGui::Selectable((group.label+"##"+std::to_string(group.band)).c_str(),value==group.band)) value=group.band;
        ImGui::EndCombo();
    }
}
inline void DrawAppearance(Character& c) {
    if(!ImGui::CollapsingHeader("Opacity and color links")) return;
    try {
        auto data=AppearanceData(c);
        ImGui::TextWrapped("Opacity changes group fill; black ink lines and shadows can remain visible. Color links borrow another group's ramp without changing its RGB colors.");
        if(ImGui::Button("Opaque")) opacityAlpha=255;
        ImGui::SameLine(); if(ImGui::Button("Half opacity")) opacityAlpha=128;
        ImGui::SameLine(); if(ImGui::Button("Quarter opacity")) opacityAlpha=64;
        ImGui::SameLine(); if(ImGui::Button("No fill")) opacityAlpha=0;
        ImGui::SliderInt("Fill opacity",&opacityAlpha,0,255);
        if(ImGui::Button("Select all opacity groups")) { opacityBands.clear(); for(auto& group:data.groups) if(group.band && group.count) opacityBands.insert(group.band); }
        ImGui::SameLine(); if(ImGui::Button("Clear opacity selection")) opacityBands.clear();
        ImGui::BeginChild("Opacity group selection",ImVec2(0,160),true);
        for(auto& group:data.groups) if(group.band && group.count) {
            bool enabled=opacityBands.count(group.band)!=0;
            if(ImGui::Checkbox((group.label+"##opacity"+std::to_string(group.band)).c_str(),&enabled)) {
                if(enabled) opacityBands.insert(group.band); else opacityBands.erase(group.band);
            }
        }
        ImGui::EndChild();
        if(ImGui::Button("Apply opacity")) { ApplyOpacity(c); SyncAppearanceColors(c); }
        ImGui::SameLine(); if(ImGui::Button("Reset opacity for this palette")) { ResetOpacity(c); SyncAppearanceColors(c); }
        ImGui::Separator();
        ImGui::TextWrapped("Always-on color links replace native rules for each chosen source group. Sampling is one step: A -> B uses B's own colors, even if B -> C also exists. Rainbow and overlay masks follow the destination band.");
        AppearanceGroupCombo("Source group",linkSource,data);
        AppearanceGroupCombo("Borrow colors from",linkDestination,data);
        if(ImGui::Button("Add / update link")) {
            CheckAppearanceBand(data,linkSource); CheckAppearanceBand(data,linkDestination);
            if(linkSource==linkDestination) throw std::runtime_error("Choose different source and destination groups.");
            colorLinks[linkSource]=linkDestination;
        }
        int erase=-1;
        for(auto& rule:colorLinks) {
            ImGui::PushID(rule.first); ImGui::Text("%s -> %s",data.groups.at(rule.first).label.c_str(),data.groups.at(rule.second).label.c_str());
            ImGui::SameLine(); if(ImGui::SmallButton("Remove")) erase=rule.first; ImGui::PopID();
        }
        if(erase!=-1) colorLinks.erase(erase);
        ImGui::TextUnformatted("Affected shader shades:");
        for(int shade=0;shade<8;shade++) {
            if(shade) ImGui::SameLine(); bool enabled=(linkShadeMask & (1<<shade))!=0;
            if(ImGui::Checkbox((std::to_string(shade+1)+"##linkshade").c_str(),&enabled)) {
                if(enabled) linkShadeMask|=1<<shade; else linkShadeMask&=~(1<<shade);
            }
        }
        if(ImGui::Button("Apply color links")) ApplyColorLinks(c);
        ImGui::SameLine(); if(ImGui::Button("Reset color links")) { ResetColorLinks(c); }
        if(ImGui::Button("Save appearance preset")) {
            std::ofstream out("appearance-preset.json"); out<<AppearancePreset(c).dump(2);
            if(!out) throw std::runtime_error("Could not save appearance preset."); appearanceStatus="Saved appearance-preset.json beside the executable.";
        }
        ImGui::SameLine(); if(ImGui::Button("Load appearance preset")) {
            nlohmann::json j; std::ifstream in("appearance-preset.json"); in>>j; LoadAppearancePreset(c,j);
        }
    } catch(const std::exception& e) { appearanceStatus=e.what(); }
    ImGui::TextWrapped("%s",appearanceStatus.c_str());
}
