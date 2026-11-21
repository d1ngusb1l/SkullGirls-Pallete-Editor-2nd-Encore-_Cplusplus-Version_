// Included within OverlayEditor: share guarded game access and restoration.
inline std::set<int> rainbowBands;
inline int rainbowSpeed=4, rainbowInterval=8;
inline std::string rainbowStatus;
inline DWORD RainbowAddress(const Character& c) {
    DWORD root=Word(base+0x85ea4c),actor=Word(root+0x5e8+4*c.ID),pal=Word(actor+0x328);
    DWORD begin=Word(pal+0x2c),end=Word(pal+0x30),address=begin+16*c.Current_Pallete_Num;
    if(!begin || address+16>end) throw std::runtime_error("Palette has no rainbow record.");
    return address;
}
inline void ApplyRainbow(const Character& c) {
    if(rainbowBands.empty()) throw std::runtime_error("Select one or more rainbow groups.");
    DWORD root=Word(base+0x85ea4c),actor=Word(root+0x5e8+4*c.ID),pal=Word(actor+0x328),address=RainbowAddress(c);
    Original original{actor,pal,address,c.ID,Read(address,16)};
    auto old=originals.find(address); if(old!=originals.end()) original=old->second;
    int count=(Word(pal+0x60)-Word(pal+0x5c))/4;
    Bytes bands; for(int band:rainbowBands) {
        if(band<1 || band>=count || band>255) throw std::runtime_error("Invalid rainbow group.");
        bands.push_back((unsigned char)band);
    }
    DWORD memory=Allocate(bands.size()); Write(memory,bands);
    Bytes record(16); record[0]=(unsigned char)rainbowSpeed; record[1]=(unsigned char)rainbowInterval;
    Put(record,4,memory); Put(record,8,memory+(DWORD)bands.size()); Put(record,12,memory+(DWORD)bands.size());
    if(!Valid(original)) throw std::runtime_error("Match changed; retry in the new match.");
    originals[address]=original; PausedWrite(address,record); Refresh();
    rainbowStatus="Rainbow applied to this palette number.";
}
inline void DrawRainbow(const Character& c) {
    if(!ImGui::CollapsingHeader("Rainbow - Phase 2")) return;
    try {
        ImGui::TextWrapped("Animated hue cycling for selected groups. Larger hue steps cycle faster; larger frame intervals cycle slower.");
        ImGui::SliderInt("Hue step",&rainbowSpeed,1,255);
        ImGui::SliderInt("Frame interval",&rainbowInterval,1,255);
        DWORD root=Word(base+0x85ea4c),actor=Word(root+0x5e8+4*c.ID),pal=Word(actor+0x328);
        DWORD begin=Word(pal+0x5c),end=Word(pal+0x60); int count=(end-begin)/4;
        if(ImGui::Button("Select all rainbow groups")) { rainbowBands.clear(); for(int band=1;band<count && band<256;band++) rainbowBands.insert(band); }
        ImGui::SameLine(); if(ImGui::Button("Clear rainbow selection")) rainbowBands.clear();
        ImGui::BeginChild("Rainbow group selection",ImVec2(0,180),true);
        int color=0; auto groups=GroupColorGroup::characterGroups.find(c.Char_Name);
        for(int band=0;band<count && band<256;band++) {
            int n=Read(begin+4*band,1)[0];
            if(band) {
                std::string label="Band "+std::to_string(band);
                if(groups!=GroupColorGroup::characterGroups.end()) for(auto& g:groups->second) if(g.startIndex==color && g.count==n) { label=g.groupName; break; }
                label+="##rainbow"+std::to_string(band); bool enabled=rainbowBands.count(band)!=0;
                if(ImGui::Checkbox(label.c_str(),&enabled)) { if(enabled) rainbowBands.insert(band); else rainbowBands.erase(band); }
            }
            color+=n;
        }
        ImGui::EndChild();
        if(ImGui::Button("Apply rainbow")) ApplyRainbow(c);
        ImGui::SameLine(); if(ImGui::Button("Reset rainbow")) { Remove(RainbowAddress(c)); rainbowStatus="Original rainbow restored."; }
        if(ImGui::Button("Save rainbow preset")) {
            nlohmann::json j={{"version",1},{"character",c.Char_Name},{"bands",rainbowBands},{"speed",rainbowSpeed},{"interval",rainbowInterval}};
            std::ofstream out("rainbow-preset.json"); out<<j.dump(2); if(!out) throw std::runtime_error("Could not save rainbow preset.");
            rainbowStatus="Saved rainbow-preset.json beside the executable.";
        }
        ImGui::SameLine(); if(ImGui::Button("Load rainbow preset")) {
            nlohmann::json j; std::ifstream in("rainbow-preset.json"); in>>j;
            if(j.at("version")!=1 || j.at("character")!=c.Char_Name) throw std::runtime_error("Preset version or character does not match.");
            auto bands=j.at("bands").get<std::set<int>>(); for(int band:bands) if(band<1 || band>=count || band>255) throw std::runtime_error("Preset has an invalid group.");
            int speed=j.at("speed"),interval=j.at("interval"); if(speed<1||speed>255||interval<1||interval>255) throw std::runtime_error("Preset timing out of range.");
            rainbowBands=bands; rainbowSpeed=speed; rainbowInterval=interval; rainbowStatus="Preset loaded. Click Apply rainbow to preview.";
        }
    } catch(const std::exception& e) { rainbowStatus=e.what(); }
    ImGui::TextWrapped("%s",rainbowStatus.c_str());
}
