// An overlay assignment is staged until Apply overlays is clicked.
#pragma once

namespace OverlayControls {
struct DualOverlayDraft {
    PaletteData::Overlay primary{"Water",{},1,0};
    bool hasSecond=false;
    PaletteData::Overlay secondary{"umbrella_veins",{},1,0};
};
inline std::map<std::pair<int,int>,DualOverlayDraft> overlayDrafts;

inline std::pair<int,int> DraftKey(const Character& c) {return {c.ID,c.Current_Pallete_Num};}

inline DualOverlayDraft& Draft(const Character& c) {
    auto key=DraftKey(c);
    if(auto existing=overlayDrafts.find(key);existing!=overlayDrafts.end())return existing->second;
    DualOverlayDraft draft;
    auto live=OverlayEditor::CapturePalette(c);
    if(live.overlay)draft.primary=*live.overlay;
    if(live.secondOverlay) {draft.secondary=*live.secondOverlay;draft.hasSecond=true;}
    return overlayDrafts.emplace(key,std::move(draft)).first->second;
}

inline void AssignDraftBand(DualOverlayDraft& draft,int band,int destination) {
    draft.primary.bands.erase(band);
    draft.secondary.bands.erase(band);
    if(destination==1)draft.primary.bands.insert(band);
    if(destination==2)draft.secondary.bands.insert(band);
}

inline int DraftBand(const DualOverlayDraft& draft,int band) {
    if(draft.primary.bands.count(band))return 1;
    if(draft.secondary.bands.count(band))return 2;
    return 0;
}

inline std::set<int> GroupBands(const OverlayEditor::AppearanceContext& data,const ColorGroup& group) {
    std::set<int> bands;
    int covered=0;
    for(const auto& band:data.groups) {
        if(!band.band || !band.count)continue;
        const int begin=(std::max)(group.startIndex,band.start);
        const int end=(std::min)(group.startIndex+group.count,band.start+band.count);
        if(begin>=end)continue;
        if(begin!=band.start || end!=band.start+band.count)
            throw std::runtime_error("This display group splits a native shader band. Select native bands in Advanced.");
        bands.insert(band.band);
        covered+=band.count;
    }
    if(bands.empty() || covered!=group.count)throw std::runtime_error("This group does not match the native shader layout.");
    return bands;
}

inline void DrawDraftGroup(const Character& c,const std::set<int>& bands,std::string& Status) {
    if(!ImGui::BeginMenu("Assign overlay"))return;
    // Draft initialization can read live data. Always balance the ImGui menu.
    try {
        auto& draft=Draft(c);
        for(int slot=0;slot<=(draft.hasSecond?2:1);slot++) {
            const char* labels[]={"No overlay","Primary overlay","Secondary overlay"};
            const bool selected=std::all_of(bands.begin(),bands.end(),[&](int b){return DraftBand(draft,b)==slot;});
            if(ImGui::MenuItem(labels[slot],nullptr,selected)) {
                for(int band:bands)AssignDraftBand(draft,band,slot);
                Status="Overlay selection updated. Use Apply overlays in Advanced.";
            }
        }
    }catch(const std::exception& e){Status=e.what();}
    ImGui::EndMenu();
}

inline void DrawDraftOverlay(PaletteData::Overlay& assignment,const OverlayEditor::AppearanceContext& data,const char* label) {
    ImGui::PushID(label);
    ImGui::SeparatorText(label);
    if(ImGui::BeginCombo("Texture",assignment.texture.c_str())) {
        for(const auto& [name,record]:OverlayEditor::templates)
            if(ImGui::Selectable(name.c_str(),assignment.texture==name))assignment.texture=name;
        ImGui::EndCombo();
    }
    const auto validTint=[&](int band) {return band>0 && band<static_cast<int>(data.groups.size()) && data.groups[band].count>0;};
    const auto& tint=assignment.tintBand;
    if(ImGui::BeginCombo("Tint",validTint(tint)?data.groups[tint].label.c_str():"Texture default")) {
        if(ImGui::Selectable("Texture default",tint==0))assignment.tintBand=0;
        for(const auto& band:data.groups)if(validTint(band.band)) {
            ImGui::PushID(band.band);
            if(ImGui::Selectable(band.label.c_str(),tint==band.band))assignment.tintBand=band.band;
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::Text("%u groups",static_cast<unsigned>(assignment.bands.size()));
    ImGui::PopID();
}

template<class Apply>
void Draw(Character& c,std::string& Status,bool busy,bool liveSecond,Apply apply) {
    if(!ImGui::CollapsingHeader("Overlay textures"))return;
    auto data=OverlayEditor::AppearanceData(c);
    auto& draft=Draft(c);
    if(OverlayEditor::templates.empty())OverlayEditor::Catalog();
    ImGui::Combo("Lighting",&draft.primary.lighting,"Original\0Unlit\0Lit\0");
    DrawDraftOverlay(draft.primary,data,"Primary overlay");
    if(draft.hasSecond) {
        if(!liveSecond)ImGui::TextWrapped("The EXE previews the primary texture. Both assignments are saved for the DLL.");
        draft.secondary.lighting=draft.primary.lighting;
        DrawDraftOverlay(draft.secondary,data,"Secondary overlay");
        if(ImGui::Button("Remove second texture")) {
            draft.hasSecond=false;
            draft.secondary.bands.clear();
        }
    } else if(ImGui::Button("Add second texture")) {
        draft.hasSecond=true;
    }
    if(ImGui::BeginChild("Draft groups",ImVec2(0,180))) {
        for(const auto& group:data.groups) {
            if(!group.band || !group.count)continue;
            ImGui::PushID(group.band);
            int selected=DraftBand(draft,group.band);
            ImGui::SetNextItemWidth(130);
            if(ImGui::Combo("##assignment",&selected,draft.hasSecond?"None\0Primary\0Secondary\0":"None\0Primary\0"))AssignDraftBand(draft,group.band,selected);
            ImGui::SameLine();
            ImGui::TextUnformatted(group.label.c_str());
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    ImGui::BeginDisabled(draft.primary.bands.empty() || (draft.hasSecond && draft.secondary.bands.empty()) || busy);
    if(ImGui::Button(!liveSecond && draft.hasSecond?"Preview primary and keep both":"Apply overlays"))try {
        auto document=OverlayEditor::CapturePalette(c);
        document.overlay=draft.primary;
        if(draft.hasSecond)document.secondOverlay=draft.secondary;
        else document.secondOverlay.reset();
        apply(std::move(document));
    }catch(const std::exception& e){Status=e.what();}
    ImGui::EndDisabled();
    ImGui::SameLine();
    if(ImGui::Button("Reset overlays")) {
        OverlayEditor::Remove(OverlayEditor::Word(data.palette+0x14)+12*c.Current_Pallete_Num);
        overlayDrafts.erase(DraftKey(c));
        Status="Original overlay restored.";
    }
}
}
