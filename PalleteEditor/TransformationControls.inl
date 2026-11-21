inline TransformSettings transformDraft;
inline int transformSource=1,transformDestination=2;
inline std::string transformStatus;
inline void SelectTransformationPalette(const Character& c) {
    transformDraft=TransformSettings{};
    transformDraft.activationTag="SUPER"; transformDraft.mode=TransformMode::Automatic;
    auto data=AppearanceData(c); auto found=colorAssignments.find(ColorLinkAddress(c,data));
    if(found!=colorAssignments.end()) {
        colorLinks=found->second.staticLinks; linkShadeMask=found->second.staticMask;
        if(found->second.hasTransformation) transformDraft=found->second.transformation;
    }
    transformStatus.clear();
}
inline nlohmann::json TransformationPreset(const Character& c,const TransformSettings& settings) {
    ValidateTransformation(c,AppearanceData(c),settings);
    return {{"version",2},{"character",c.Char_Name},{"trigger",settings.activationTag.empty()?TransformTrigger(c):"tag:"+settings.activationTag},{"activationTag",settings.activationTag},
            {"previewMode",(int)settings.mode},{"links",settings.links},{"shadeMask",settings.shadeMask}};
}
inline TransformSettings ReadTransformationPreset(const Character& c,const nlohmann::json& j) {
    if((j.at("version")!=1 && j.at("version")!=2) || j.at("character")!=c.Char_Name) throw std::runtime_error("Preset version or character does not match.");
    TransformSettings settings; settings.activationTag=j.value("activationTag",std::string{}); settings.links=j.at("links").get<std::map<int,int>>();
    settings.shadeMask=j.at("shadeMask"); int mode=j.at("previewMode"); settings.mode=(TransformMode)mode;
    if(mode<0 || mode>2) throw std::runtime_error("Invalid preview mode.");
    if(settings.mode==TransformMode::Automatic && j.at("trigger")!=(settings.activationTag.empty()?TransformTrigger(c):"tag:"+settings.activationTag))
        throw std::runtime_error("Automatic trigger does not match this character and palette.");
    ValidateTransformation(c,AppearanceData(c),settings); return settings;
}
inline void DrawTransformation(const Character& c) {
    if(!ImGui::CollapsingHeader("Conditional transformation")) return;
    try {
        auto data=AppearanceData(c); auto trigger=TransformTrigger(c);
        ImGui::TextWrapped("Transformed groups borrow alternate color ramps. Existing always-on links form the normal appearance. Apply commits the selected activation/preview mode; it does not change gameplay flags.");
        ImGui::TextWrapped("Native animation tags work across characters. Tags follow authored move phases; some recovery frames may not carry the move's tag. Color-group mappings remain character-specific.");
        if(ImGui::BeginCombo("Trigger",NativeTriggerLabel(transformDraft.activationTag))) {
            for(auto& entry:nativeTriggers) if(ImGui::Selectable(entry.label,transformDraft.activationTag==entry.tag)) transformDraft.activationTag=entry.tag;
            if(trigger!="preview-only" && ImGui::Selectable("Legacy character trigger",transformDraft.activationTag.empty())) transformDraft.activationTag.clear();
            ImGui::EndCombo();
        }
        if(transformDraft.activationTag.empty()) {
            if(trigger=="annie-supers") ImGui::TextWrapped("Legacy Annie animation range; includes Star Power activation/deactivation, not the persistent install.");
            else if(trigger=="filia-color30-flag20") ImGui::TextWrapped("Legacy Filia Color 30 authored flag; colors only, with native flash/sparkles unchanged.");
            else ImGui::TextWrapped("Choose a named native trigger to enable automatic activation here.");
        }
        int mode=(int)transformDraft.mode;
        if(ImGui::BeginCombo("Activation / preview",mode==0?"Automatic":mode==1?"Preview normal":"Preview transformed")) {
            if((!transformDraft.activationTag.empty() || trigger!="preview-only") && ImGui::Selectable("Automatic",mode==0)) transformDraft.mode=TransformMode::Automatic;
            if(ImGui::Selectable("Preview normal",mode==1)) transformDraft.mode=TransformMode::NormalPreview;
            if(ImGui::Selectable("Preview transformed",mode==2)) transformDraft.mode=TransformMode::TransformedPreview;
            ImGui::EndCombo();
        }
        ImGui::PushID("transformation");
        AppearanceGroupCombo("Source group",transformSource,data);
        AppearanceGroupCombo("Transformed colors",transformDestination,data);
        bool add=ImGui::Button("Add / update transformed link");
        bool example=c.Char_Name=="Annie" && ImGui::Button("Example: hair / braid borrow foot colors");
        int erase=-1;
        for(auto& rule:transformDraft.links) {
            ImGui::PushID(rule.first);
            ImGui::Text("%s -> %s",data.groups.at(rule.first).label.c_str(),data.groups.at(rule.second).label.c_str());
            ImGui::SameLine(); if(ImGui::SmallButton("Remove")) erase=rule.first;
            ImGui::PopID();
        }
        if(erase!=-1) transformDraft.links.erase(erase);
        ImGui::TextUnformatted("Transformed shader shades:");
        for(int shade=0;shade<8;shade++) {
            if(shade) ImGui::SameLine(); bool enabled=(transformDraft.shadeMask&(1<<shade))!=0;
            if(ImGui::Checkbox((std::to_string(shade+1)+"##shade").c_str(),&enabled)) {
                if(enabled) transformDraft.shadeMask|=1<<shade; else transformDraft.shadeMask&=~(1<<shade);
            }
        }
        // Close the ID scope before operations that can throw.
        bool apply=ImGui::Button("Apply transformation"); ImGui::SameLine(); bool reset=ImGui::Button("Reset transformation");
        bool save=ImGui::Button("Save transformation preset"); ImGui::SameLine(); bool load=ImGui::Button("Load transformation preset");
        ImGui::PopID();
        if(add) {
            CheckAppearanceBand(data,transformSource); CheckAppearanceBand(data,transformDestination);
            if(transformSource==transformDestination) throw std::runtime_error("Choose different source and destination groups.");
            transformDraft.links[transformSource]=transformDestination;
        }
        if(example) {
            CheckAppearanceBand(data,3); CheckAppearanceBand(data,4); CheckAppearanceBand(data,26);
            transformDraft.links={{3,26},{4,26}};
            transformStatus="Example loaded. Result uses this palette's foot colors; click Apply transformation.";
        }
        if(apply) { ApplyTransformation(c,transformDraft); transformStatus="Transformation applied to this palette. Always-on links preserved."; }
        if(reset) { ResetTransformation(c); transformStatus="Native appearance restored; separately applied always-on links preserved."; }
        if(save) {
            auto preset=TransformationPreset(c,transformDraft); std::ofstream out("transformation-preset.json"); out<<preset.dump(2);
            if(!out) throw std::runtime_error("Could not save transformation preset."); transformStatus="Saved transformation-preset.json beside the executable.";
        }
        if(load) {
            nlohmann::json j; std::ifstream in("transformation-preset.json"); in>>j;
            transformDraft=ReadTransformationPreset(c,j); transformStatus="Preset loaded. Click Apply transformation.";
        }
    } catch(const std::exception& e) { transformStatus=e.what(); }
    ImGui::TextWrapped("%s",transformStatus.c_str());
}
