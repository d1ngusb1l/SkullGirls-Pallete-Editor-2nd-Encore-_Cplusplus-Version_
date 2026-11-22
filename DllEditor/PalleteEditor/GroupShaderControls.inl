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
            std::set<int> bands;int covered=0;
            for(auto& band:data.groups) {
                if(!band.band || !band.count)continue;
                int begin=std::max(group.startIndex,band.start);
                int end=std::min(group.startIndex+group.count,band.start+band.count);
                if(begin>=end)continue;
                if(begin!=band.start || end!=band.start+band.count)throw std::runtime_error("This display group splits a native shader band. Use Advanced to select native bands.");
                bands.insert(band.band);covered+=band.count;
            }
            if(bands.empty() || covered!=group.count)throw std::runtime_error("This group does not match the native shader layout.");
            auto apply=[&](){
                try {OverlayEditor::ApplyPalette(c,d);Sync();Status="Group shader settings applied. Save palette to keep them.";}
                catch(const std::exception& e){Status=e.what();}
            };
            auto includes=[&](const std::set<int>& selected){return std::all_of(bands.begin(),bands.end(),[&](int b){return selected.count(b)!=0;});};
            auto outside=[&](const std::set<int>& selected){return std::any_of(selected.begin(),selected.end(),[&](int b){return !bands.count(b);});};
            ImGui::TextUnformatted(group.groupName.c_str());
            ImGui::TextDisabled("Changes apply immediately to this palette number.");
            bool overlay=d.overlay && includes(d.overlay->bands);
            if(ImGui::Checkbox("Overlay texture",&overlay)) {
                if(overlay) {
                    if(!d.overlay)d.overlay=PaletteData::Overlay{"Fire",{},0,0};
                    d.overlay->bands.insert(bands.begin(),bands.end());
                } else if(d.overlay) {
                    for(int b:bands)d.overlay->bands.erase(b);
                    if(d.overlay->bands.empty())d.overlay.reset();
                }
                apply();
            }
            if(d.overlay) {
                bool shared=outside(d.overlay->bands);
                ImGui::BeginDisabled(shared);
                if(ImGui::BeginCombo("Texture",d.overlay->texture.c_str())) {
                    if(OverlayEditor::templates.empty())OverlayEditor::Catalog();
                    for(auto& entry:OverlayEditor::templates)if(ImGui::Selectable(entry.first.c_str(),entry.first==d.overlay->texture)){d.overlay->texture=entry.first;apply();}
                    ImGui::EndCombo();
                }
                int lighting=d.overlay->lighting;
                if(ImGui::Combo("Lighting",&lighting,"Original\0Unlit\0Lit\0")){d.overlay->lighting=lighting;apply();}
                ImGui::EndDisabled();
                if(shared)ImGui::TextDisabled("Texture and lighting are shared. Change them in Advanced.");
            }
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
