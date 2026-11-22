// Draft controls for the validation build. Drafts are separate from live effects.
namespace {
struct DualOverlayDraft {
    PaletteData::Overlay primary{"Water",{},1,0};
    PaletteData::Overlay secondary{"umbrella_veins",{},1,0};
};
std::map<std::pair<int,int>,DualOverlayDraft> overlayDrafts;
std::optional<std::pair<int,int>> pendingDraftFile;
std::optional<std::pair<int,int>> revealDraft;

std::pair<int,int> DraftKey(const Character& c) {return {c.ID,c.Current_Pallete_Num};}

void LoadDualOverlayDraft(const Character& c,const PaletteData::Document& document) {
    const auto data=OverlayEditor::AppearanceData(c);
    std::vector<int> sizes;
    for(const auto& group:data.groups)sizes.push_back(group.count);
    if(document.character!=c.Char_Name || document.groupSizes!=sizes || !document.overlay || !document.secondOverlay)
        throw std::runtime_error("Choose a two-texture file for the current character and group layout.");
    const auto key=DraftKey(c);
    overlayDrafts[key]={*document.overlay,*document.secondOverlay};
    revealDraft=key;
    Status="Both overlay assignments loaded into Advanced > Two-texture draft. The timed preview runs separately.";
}

DualOverlayDraft& Draft(const Character& c) {
    auto key=DraftKey(c);
    if(auto existing=overlayDrafts.find(key);existing!=overlayDrafts.end())return existing->second;
    DualOverlayDraft draft;
    auto live=OverlayEditor::CapturePalette(c);
    if(live.overlay)draft.primary=*live.overlay;
    return overlayDrafts.emplace(key,std::move(draft)).first->second;
}

void AssignDraftBand(DualOverlayDraft& draft,int band,int destination) {
    draft.primary.bands.erase(band);
    draft.secondary.bands.erase(band);
    if(destination==1)draft.primary.bands.insert(band);
    if(destination==2)draft.secondary.bands.insert(band);
}

int DraftBand(const DualOverlayDraft& draft,int band) {
    if(draft.primary.bands.count(band))return 1;
    if(draft.secondary.bands.count(band))return 2;
    return 0;
}

void DrawDraftGroup(const Character& c,const std::set<int>& bands) {
    if(!ImGui::BeginMenu("Two-texture draft"))return;
    // Draft initialization can read live data. Always balance the ImGui menu.
    try {
        auto& draft=Draft(c);
        for(int slot=0;slot<=2;slot++) {
            const char* labels[]={"No overlay","Primary overlay","Secondary overlay"};
            const bool selected=std::all_of(bands.begin(),bands.end(),[&](int b){return DraftBand(draft,b)==slot;});
            if(ImGui::MenuItem(labels[slot],nullptr,selected))
                for(int band:bands)AssignDraftBand(draft,band,slot);
        }
    }catch(const std::exception& e){Status=e.what();}
    ImGui::EndMenu();
}

void DrawDraftOverlay(PaletteData::Overlay& assignment,const OverlayEditor::AppearanceContext& data,const char* label) {
    ImGui::PushID(label);
    ImGui::SeparatorText(label);
    if(ImGui::BeginCombo("Texture",assignment.texture.c_str())) {
        for(const auto& [name,record]:OverlayEditor::templates)
            if(ImGui::Selectable(name.c_str(),assignment.texture==name))assignment.texture=name;
        ImGui::EndCombo();
    }
    ImGui::Combo("Lighting",&assignment.lighting,"Original\0Unlit\0Lit\0");
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

void DrawDualOverlayControls(const Character& c) {
    if(revealDraft==DraftKey(c)) {
        ImGui::SetNextItemOpen(true,ImGuiCond_Always);
        revealDraft.reset();
    }
    if(!ImGui::CollapsingHeader("Two-texture draft"))return;
    auto data=OverlayEditor::AppearanceData(c);
    auto& draft=Draft(c);
    if(OverlayEditor::templates.empty())OverlayEditor::Catalog();
    ImGui::TextWrapped("Drafts save to .pal for the validation preview. Live effects stay as applied.");
    DrawDraftOverlay(draft.primary,data,"Primary");
    DrawDraftOverlay(draft.secondary,data,"Secondary");
    if(ImGui::BeginChild("Draft groups",ImVec2(0,180))) {
        for(const auto& group:data.groups) {
            if(!group.band || !group.count)continue;
            ImGui::PushID(group.band);
            int selected=DraftBand(draft,group.band);
            ImGui::SetNextItemWidth(130);
            if(ImGui::Combo("##assignment",&selected,"None\0Primary\0Secondary\0"))AssignDraftBand(draft,group.band,selected);
            ImGui::SameLine();
            ImGui::TextUnformatted(group.label.c_str());
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    IGFD::FileDialogConfig cfg;
    cfg.path=".";
    ImGui::BeginDisabled(draft.primary.bands.empty() || draft.secondary.bands.empty());
    if(ImGui::Button("Save two-texture draft")) {
        pendingDraftFile=DraftKey(c);
        cfg.flags=ImGuiFileDialogFlags_ConfirmOverwrite;
        ImGuiFileDialog::Instance()->OpenDialog("SaveDualOverlayDraft","Save Two-Texture Draft",".pal",cfg);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if(ImGui::Button("Load two-texture draft")) {
        pendingDraftFile=DraftKey(c);
        ImGuiFileDialog::Instance()->OpenDialog("LoadDualOverlayDraft","Load Two-Texture Draft",".pal",cfg);
    }
}
}

void DrawDualOverlayDialogs() {
    for(const char* id:{"SaveDualOverlayDraft","LoadDualOverlayDraft"}) {
        auto* dialog=ImGuiFileDialog::Instance();
        if(!dialog->Display(id))continue;
        if(dialog->IsOk())try {
            auto c=Current();
            if(pendingDraftFile!=DraftKey(c))throw std::runtime_error("Palette changed while the dialog was open.");
            const auto path=PaletteData::PathFromText(dialog->GetFilePathName());
            if(std::string_view(id)=="SaveDualOverlayDraft") {
                auto d=OverlayEditor::CapturePalette(c);
                const auto& draft=Draft(c);
                d.overlay=draft.primary;
                d.secondOverlay=draft.secondary;
                PaletteData::AtomicWrite(path,PaletteData::EncodePal(d));
                Status="Two-texture draft saved.";
            } else {
                LoadDualOverlayDraft(c,PaletteData::ReadFile(path));
            }
        }catch(const std::exception& e){Status=e.what();}
        pendingDraftFile.reset();
        dialog->Close();
    }
}
