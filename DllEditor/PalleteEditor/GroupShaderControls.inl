// Uses the same applied document as Advanced, Save, Load and Auto-load.
// Labels may combine native bands, but must never cut through a native band.
void DrawGroup(const ColorGroup& group) {
    auto& selected=PlayableCharactersManager::GetCurrentCharacter();
    ImGui::PushID(PlayableCharactersManager::GetCurrentCharacterIndex());
    ImGui::PushID(selected.Current_Palette_Num);
    ImGui::PushID(group.startIndex);
    ImGui::SeparatorText("Shader assignments");
    {
        try {
            auto c=Current();auto data=OverlayEditor::AppearanceData(c);
            auto d=OverlayEditor::CapturePalette(c);
            const auto bands=OverlayControls::GroupBands(data,group);
            auto apply=[&](){
                try {
                    if(LiveOverlays::Busy())throw std::runtime_error("Wait for the overlay operation to finish.");
                    OverlayEditor::ApplyPalette(c,d,true,true);
                    Sync();
                    Status="Group shader settings applied.";
                }
                catch(const std::exception& e){Status=e.what();}
            };
            auto includes=[&](const std::set<int>& selected){return std::all_of(bands.begin(),bands.end(),[&](int b){return selected.count(b)!=0;});};
            auto outside=[&](const std::set<int>& selected){return std::any_of(selected.begin(),selected.end(),[&](int b){return !bands.count(b);});};
            ImGui::TextUnformatted(group.groupName.c_str());
            auto& draft=Draft(c);
            if(OverlayEditor::templates.empty())OverlayEditor::Catalog();
            auto slotControls=[&](PaletteData::Overlay& overlay,int slot,const char* label) {
                ImGui::PushID(slot);
                if(ImGui::BeginCombo(label,overlay.texture.c_str())) {
                    for(const auto& entry:OverlayEditor::templates)if(ImGui::Selectable(entry.first.c_str(),entry.first==overlay.texture))overlay.texture=entry.first;
                    ImGui::EndCombo();
                }
                bool assigned=std::all_of(bands.begin(),bands.end(),[&](int b){return DraftBand(draft,b)==slot;});
                if(ImGui::Checkbox("Assign to this group",&assigned))for(int b:bands)AssignDraftBand(draft,b,assigned?slot:0);
                ImGui::PopID();
            };
            ImGui::BeginDisabled(Busy());
            slotControls(draft.primary,1,"First overlay");
            if(draft.hasSecond)slotControls(draft.secondary,2,"Second overlay");
            else if(ImGui::Button("Add second texture"))draft.hasSecond=true;
            draft.secondary.lighting=draft.primary.lighting;
            ImGui::Combo("Overlay lighting",&draft.primary.lighting,"Original\0Unlit\0Lit\0");
            if(ImGui::Button("Apply overlays"))ApplyPending();
            ImGui::SameLine();ImGui::TextUnformatted(HasPending()?"Pending":"Applied");
            ImGui::EndDisabled();
            if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());
            ImGui::TextDisabled("Other shader changes apply immediately to this palette number.");
            bool rainbow=d.rainbow && includes(d.rainbow->bands);
            if(ImGui::Checkbox("Rainbow",&rainbow)) {
                if(rainbow){if(!d.rainbow)d.rainbow=PaletteData::Rainbow{{},4,8};d.rainbow->bands.insert(bands.begin(),bands.end());}
                else if(d.rainbow){for(int b:bands)d.rainbow->bands.erase(b);if(d.rainbow->bands.empty())d.rainbow.reset();}
                apply();
            }
            if(d.rainbow) {
                bool shared=outside(d.rainbow->bands);ImGui::BeginDisabled(shared);
                int speed=d.rainbow->speed,interval=d.rainbow->interval;
                if(ImGui::SliderInt("Rainbow speed",&speed,1,255)){d.rainbow->speed=speed;apply();}
                if(ImGui::SliderInt("Rainbow interval",&interval,1,255)){d.rainbow->interval=interval;apply();}
                ImGui::EndDisabled();
                if(shared)ImGui::TextDisabled("Rainbow timing is shared. Change it in Advanced.");
            }
            ImGui::SeparatorText("Borrow colors");
            ImGui::TextWrapped("Apply colors to: %s",group.groupName.c_str());
            for(int source:bands) {
                ImGui::PushID(source);
                int normal=d.colorLinks && d.colorLinks->links.count(source)?d.colorLinks->links.at(source):0;
                if(ImGui::BeginCombo(("Always on##"+std::to_string(source)).c_str(),normal?data.groups[normal].label.c_str():"Original colors")) {
                    if(ImGui::Selectable("Original colors",normal==0)) {
                        if(d.colorLinks){d.colorLinks->links.erase(source);if(d.colorLinks->links.empty())d.colorLinks.reset();}apply();
                    }
                    for(auto& dest:data.groups)if(dest.band && dest.count && dest.band!=source && ImGui::Selectable((dest.label+"##"+std::to_string(dest.band)).c_str(),normal==dest.band)) {
                        if(!d.colorLinks)d.colorLinks=PaletteData::ColorLinks{};
                        d.colorLinks->links[source]=dest.band;apply();
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopID();
            }
            ImGui::SeparatorText("Transformation rules");
            ImGui::BeginDisabled(Busy());
            OverlayEditor::DrawTransformationEditor(c,bands);
            ImGui::EndDisabled();
            if(ImGui::Button("Remove shader assignments from this group")) {
                for(int b:bands) {
                    if(d.rainbow)d.rainbow->bands.erase(b);
                    if(d.colorLinks)d.colorLinks->links.erase(b);
                    for(auto& rule:d.transformations)rule.links.erase(b);
                }
                if(d.rainbow && d.rainbow->bands.empty())d.rainbow.reset();
                if(d.colorLinks && d.colorLinks->links.empty())d.colorLinks.reset();
                d.transformations.erase(std::remove_if(d.transformations.begin(),d.transformations.end(),[](const auto& rule){return rule.links.empty();}),d.transformations.end());
                apply();
                for(int b:bands)AssignDraftBand(Draft(c),b,0);
                if(HasPending())Status="Other assignments removed. Overlay removal Pending; use Apply overlays.";
            }
            if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());
        }catch(const std::exception& e){
            ImGui::TextWrapped("%s",e.what());
            if(ImGui::Button("Open Advanced native band controls")){OpenAdvanced=true;RevealNativeBands=true;}
        }
    }
    ImGui::PopID();ImGui::PopID();ImGui::PopID();
}
