inline TransformSettings transformDraft;
inline int transformSource=1,transformDestination=2;
inline std::string transformStatus;
inline void SelectTransformationPalette(const Character& c) {
    transformDraft=TransformSettings{};
    transformDraft.activationTag="SUPER"; transformDraft.mode=TransformMode::Automatic;
    auto data=AppearanceData(c); auto found=colorAssignments.find(ColorLinkAddress(c,data));
    if(found!=colorAssignments.end()) {
        colorLinks=found->second.staticLinks; linkShadeMask=found->second.staticMask;
        if(!found->second.transformations.empty()) transformDraft=found->second.transformations.front();
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
// One staged collection per current target; no writes until Apply rules.
inline std::vector<TransformSettings> ruleDraft,ruleObserved;
inline DWORD ruleActor=0,rulePalette=0;
inline int rulePaletteNumber=-1,ruleIndex=0;
inline unsigned long long ruleDraftGeneration=0;
inline void DrawTransformationEditor(const Character& c,const std::set<int>& sourceBands={}) {
    try {
        auto data=AppearanceData(c); auto live=CurrentColorAssignment(c,data);
        if(ruleDraftGeneration!=ruleGeneration || ruleActor!=data.actor || rulePalette!=data.palette || rulePaletteNumber!=c.Current_Pallete_Num || ruleObserved!=live.transformations) {
            ruleDraftGeneration=ruleGeneration;ruleActor=data.actor; rulePalette=data.palette; rulePaletteNumber=c.Current_Pallete_Num;
            ruleObserved=ruleDraft=live.transformations; ruleIndex=0;
        }
        if(!sourceBands.empty() && !sourceBands.count(transformSource))transformSource=*sourceBands.begin();
        ImGui::TextWrapped("Each rule owns its trigger and native band shades. Overlapping rules are rejected, including disabled rules. Changes are staged until Apply rules.");
        if(ImGui::BeginCombo("Rule",ruleDraft.empty()?"No rules":ruleDraft[std::clamp(ruleIndex,0,(int)ruleDraft.size()-1)].name.c_str())) {
            for(int i=0;i<(int)ruleDraft.size();++i)if(ImGui::Selectable((ruleDraft[i].name+"##"+ruleDraft[i].id).c_str(),ruleIndex==i))ruleIndex=i;
            ImGui::EndCombo();
        }
        if(ImGui::Button("New rule") && ruleDraft.size()<32) {
            TransformSettings rule;rule.mode=TransformMode::Automatic;rule.activationTag="ATTACK";
            int next=1;auto exists=[&](const std::string& id){return std::any_of(ruleDraft.begin(),ruleDraft.end(),[&](const auto& r){return r.id==id;});};
            while(exists("rule-"+std::to_string(next)))++next;
            rule.id="rule-"+std::to_string(next);rule.name="Rule "+std::to_string(next);
            ruleDraft.push_back(rule);ruleIndex=(int)ruleDraft.size()-1;
            if(!sourceBands.empty())transformSource=*sourceBands.begin();
        }
        if(!ruleDraft.empty()) {
            ruleIndex=std::clamp(ruleIndex,0,(int)ruleDraft.size()-1);auto& rule=ruleDraft[ruleIndex];
            ImGui::SameLine();bool remove=ImGui::Button("Remove selected rule");
            char name[97]{};memcpy(name,rule.name.data(),(std::min)(rule.name.size(),sizeof(name)-1));
            if(ImGui::InputText("Rule name",name,sizeof(name)))rule.name=name;
            ImGui::Checkbox("Enabled",&rule.enabled);
            if(ImGui::BeginCombo("Trigger",NativeTriggerLabel(rule.activationTag))) {
                for(const auto& entry:nativeTriggers)if(ImGui::Selectable(entry.label,rule.activationTag==entry.tag)){rule.activationTag=entry.tag;rule.mode=TransformMode::Automatic;}
                if(TransformTrigger(c)!="preview-only" && ImGui::Selectable("Legacy character trigger",rule.activationTag.empty())){rule.activationTag.clear();rule.mode=TransformMode::Automatic;}
                ImGui::EndCombo();
            }
            ImGui::TextDisabled("Tags follow authored move phases; confirm them for the chosen fighter.");
            if(rule.mode!=TransformMode::Automatic) {
                ImGui::TextWrapped("Imported live override: %s. Preserved from the old previewMode field; this affects the live palette, not the isolated viewer.",rule.mode==TransformMode::NormalPreview?"forced normal":"forced transformed");
                if(ImGui::Button("Use trigger instead of imported override"))rule.mode=TransformMode::Automatic;
            }
            if(sourceBands.empty())AppearanceGroupCombo("Apply colors to",transformSource,data);
            else if(ImGui::BeginCombo("Apply colors to",data.groups.at(transformSource).label.c_str())) {
                for(int band:sourceBands)if(ImGui::Selectable(data.groups[band].label.c_str(),transformSource==band))transformSource=band;
                ImGui::EndCombo();
            }
            AppearanceGroupCombo("Copy colors from",transformDestination,data);
            if(ImGui::Button("Add / update link")) {
                CheckAppearanceBand(data,transformSource);CheckAppearanceBand(data,transformDestination);
                if(!sourceBands.empty() && !sourceBands.count(transformSource))throw std::runtime_error("Select a native band in this group.");
                if(transformSource==transformDestination)throw std::runtime_error("Choose different source and destination bands.");
                rule.links[transformSource]=transformDestination;
            }
            int erase=-1;
            for(const auto& link:rule.links) {
                ImGui::PushID(link.first);ImGui::Text("Band %d <- band %d",link.first,link.second);ImGui::SameLine();
                if(ImGui::SmallButton("Remove link"))erase=link.first;ImGui::PopID();
            }
            if(erase!=-1)rule.links.erase(erase);
            ImGui::TextUnformatted("Native shader shades (shared by this rule's links):");
            for(int shade=0;shade<8;++shade) {
                if(shade)ImGui::SameLine();bool on=(rule.shadeMask&(1<<shade))!=0;
                if(ImGui::Checkbox((std::to_string(shade+1)+"##ruleShade").c_str(),&on)){if(on)rule.shadeMask|=1<<shade;else rule.shadeMask&=~(1<<shade);}
            }
            if(!sourceBands.empty() && std::any_of(rule.links.begin(),rule.links.end(),[&](const auto& link){return !sourceBands.count(link.first);}))ImGui::TextWrapped("This rule also affects other groups. Its trigger, enabled state and shades are shared across all its links.");
            if(remove){ruleDraft.erase(ruleDraft.begin()+ruleIndex);ruleIndex=0;}
        }
        if(ImGui::Button("Apply rules")) {
            auto assignment=CurrentColorAssignment(c,data);assignment.transformations=ruleDraft;
            CommitColorAssignment(c,data,assignment);ruleObserved=ruleDraft;transformStatus="Rules applied; static links preserved.";
        }
        ImGui::SameLine();if(ImGui::Button("Discard rule edits")){ruleDraft=ruleObserved;ruleIndex=0;transformStatus.clear();}
        ImGui::TextWrapped("%s",transformStatus.c_str());
        ImGui::TextDisabled("Beowulf Hype: Unsupported (resource ownership and semantics unverified).");
    } catch(const std::exception& e){transformStatus=e.what();ImGui::TextWrapped("%s",transformStatus.c_str());}
}
inline void DrawTransformation(const Character& c) {
    if(ImGui::CollapsingHeader("Transformation rules"))DrawTransformationEditor(c);
}
