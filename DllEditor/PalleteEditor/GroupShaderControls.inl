// Uses the same applied document as Advanced, Save, Load and Auto-load.
// Labels may combine native bands, but must never cut through a native band.
void DrawGroup(const ColorGroup& group) {
    auto& selected=PlayableCharactersManager::GetCurrentCharacter();
    ImGui::PushID(PlayableCharactersManager::GetCurrentCharacterIndex());
    ImGui::PushID(selected.Current_Palette_Num);
    ImGui::PushID(group.startIndex);
    if(ImGui::Button("Shader settings"))ImGui::OpenPopup("Group shader settings");
    if(ImGui::BeginPopup("Group shader settings")) {
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
            DrawDraftGroup(c,bands,Status);
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
                int transformed=d.transformation && d.transformation->links.count(source)?d.transformation->links.at(source):0;
                if(ImGui::BeginCombo(("Copy colors from (when triggered)##"+std::to_string(source)).c_str(),transformed?data.groups[transformed].label.c_str():"No transformation")) {
                    if(ImGui::Selectable("No transformation",transformed==0)) {
                        if(d.transformation){d.transformation->links.erase(source);if(d.transformation->links.empty())d.transformation.reset();}apply();
                    }
                    for(auto& dest:data.groups)if(dest.band && dest.count && dest.band!=source && ImGui::Selectable((dest.label+"##"+std::to_string(dest.band)).c_str(),transformed==dest.band)) {
                        if(!d.transformation)d.transformation=PaletteData::Transformation{{},255,0,"SUPER",""};
                        d.transformation->links[source]=dest.band;apply();
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopID();
            }
            if(d.transformation) {
                bool shared=std::any_of(d.transformation->links.begin(),d.transformation->links.end(),[&](auto& link){return !bands.count(link.first);});
                ImGui::BeginDisabled(shared);
                if(ImGui::BeginCombo("Trigger",OverlayEditor::NativeTriggerLabel(d.transformation->activationTag))) {
                    for(auto& entry:OverlayEditor::nativeTriggers)if(ImGui::Selectable(entry.label,d.transformation->activationTag==entry.tag)) {
                        d.transformation->activationTag=entry.tag;d.transformation->legacyTrigger.clear();d.transformation->previewMode=0;apply();
                    }
                    ImGui::EndCombo();
                }
                ImGui::EndDisabled();
                if(shared)ImGui::TextDisabled("The transformation trigger is shared. Change it in Advanced.");
            }
            if(ImGui::Button("Remove shader assignments from this group")) {
                for(int b:bands) {
                    if(d.overlay)d.overlay->bands.erase(b);
                    if(d.rainbow)d.rainbow->bands.erase(b);
                    if(d.colorLinks)d.colorLinks->links.erase(b);
                    if(d.transformation)d.transformation->links.erase(b);
                }
                if(d.overlay && d.overlay->bands.empty())d.overlay.reset();
                if(d.rainbow && d.rainbow->bands.empty())d.rainbow.reset();
                if(d.colorLinks && d.colorLinks->links.empty())d.colorLinks.reset();
                if(d.transformation && d.transformation->links.empty())d.transformation.reset();
                apply();
            }
            if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());
        }catch(const std::exception& e){ImGui::TextWrapped("%s",e.what());}
        ImGui::EndPopup();
    }
    ImGui::PopID();ImGui::PopID();ImGui::PopID();
}
