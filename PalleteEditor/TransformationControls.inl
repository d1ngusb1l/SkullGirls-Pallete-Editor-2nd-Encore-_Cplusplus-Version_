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
    if(settings.hypeLevel>=0 || !settings.moveId.empty())throw std::runtime_error("Typed predicates require a .pal save; legacy presets cannot represent them.");
    return {{"version",2},{"character",c.Char_Name},{"trigger",settings.activationTag.empty()?TransformTrigger(c):"tag:"+settings.activationTag},{"activationTag",settings.activationTag},
            {"previewMode",(int)settings.mode},{"links",settings.links},{"shadeMask",settings.shadeMask}};
}
inline TransformSettings ReadTransformationPreset(const Character& c,const nlohmann::json& j) {
    if(j.contains("predicate"))throw std::runtime_error("Resource predicates require a V4 .pal file.");
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
inline int ruleTriggerCategory=0;
inline std::string ruleTriggerSelectionKey;
inline char ruleMoveFilter[97]{};
inline void DrawTransformationEditor(const Character& c,const std::set<int>& sourceBands={}) {
    try {
        auto data=AppearanceData(c); auto live=CurrentColorAssignment(c,data);
        if(ruleDraftGeneration!=ruleGeneration || ruleActor!=data.actor || rulePalette!=data.palette || rulePaletteNumber!=c.Current_Pallete_Num || ruleObserved!=live.transformations) {
            ruleDraftGeneration=ruleGeneration;ruleActor=data.actor; rulePalette=data.palette; rulePaletteNumber=c.Current_Pallete_Num;
            ruleObserved=ruleDraft=live.transformations; ruleIndex=0;ruleTriggerSelectionKey.clear();
        }
        if(!sourceBands.empty() && !sourceBands.count(transformSource))transformSource=*sourceBands.begin();
        ImGui::TextWrapped("Each rule owns its trigger and native band shades. Overlaps require mutually exclusive Hype levels or mapped moves. Changes are staged until Apply rules.");
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
            const auto move=PaletteData::FindCharacterMove(rule.moveId);
            const int currentCategory=move?1:rule.hypeLevel>=0?2:0;
            if(ruleTriggerSelectionKey!=rule.id){ruleTriggerSelectionKey=rule.id;ruleTriggerCategory=currentCategory;ruleMoveFilter[0]=0;}
            const std::string triggerLabel=move?move->label:rule.hypeLevel>=0?"Beowulf Hype level "+std::to_string(rule.hypeLevel):NativeTriggerLabel(rule.activationTag);
            ImGui::Combo("Trigger type",&ruleTriggerCategory,"General\0Character moves\0Resources\0");
            if(ruleTriggerCategory!=currentCategory)ImGui::TextWrapped("Current trigger: %s. Choose a replacement below.",triggerLabel.c_str());
            if(ruleTriggerCategory==1)ImGui::InputText("Find move",ruleMoveFilter,sizeof(ruleMoveFilter));
            if(ImGui::BeginCombo("Trigger",ruleTriggerCategory==currentCategory?triggerLabel.c_str():"Choose trigger")) {
                if(ruleTriggerCategory==0) {
                    for(const auto& entry:nativeTriggers)if(ImGui::Selectable(entry.label,currentCategory==0 && rule.activationTag==entry.tag)){rule.activationTag=entry.tag;rule.hypeLevel=-1;rule.moveId.clear();rule.mode=TransformMode::Automatic;}
                    if(TransformTrigger(c)!="preview-only" && ImGui::Selectable("Legacy character trigger",currentCategory==0 && rule.activationTag.empty())){rule.activationTag.clear();rule.hypeLevel=-1;rule.moveId.clear();rule.mode=TransformMode::Automatic;}
                } else if(ruleTriggerCategory==1) {
                    bool any=false;
                    for(const auto& entry:PaletteData::CharacterMoves())if(entry.character==c.Char_Name) {
                        if(ruleMoveFilter[0] && std::search(entry.label.begin(),entry.label.end(),ruleMoveFilter,ruleMoveFilter+strlen(ruleMoveFilter),[](unsigned char a,unsigned char b){return std::tolower(a)==std::tolower(b);})==entry.label.end())continue;
                        any=true;
                        if(ImGui::Selectable(entry.label.c_str(),rule.moveId==entry.id)){rule.moveId=entry.id;rule.activationTag.clear();rule.hypeLevel=-1;rule.mode=TransformMode::Automatic;}
                    }
                    if(!any)ImGui::TextDisabled("No mapped moves match this character/filter.");
                } else {
                    if(c.Char_Name=="Beowulf")for(int level=0;level<4;++level)if(ImGui::Selectable(("Hype level "+std::to_string(level)).c_str(),rule.hypeLevel==level)){rule.hypeLevel=level;rule.activationTag.clear();rule.moveId.clear();rule.mode=TransformMode::Automatic;}
                    if(c.Char_Name!="Beowulf")ImGui::TextDisabled("No supported resource provider for this character.");
                }
                ImGui::EndCombo();
            }
            if(ruleTriggerCategory==1)ImGui::TextWrapped("Initial mappings: Beowulf Chair Toss and Gigantic Arm. Covers mapped move phases; awaiting gameplay tests.");
            if(rule.hypeLevel>=0)ImGui::TextWrapped("Completed Hype amount only: level %d. Fractional progress stays in the lower level; this does not track a persistent boost after spending.",rule.hypeLevel);
            if(ruleTriggerCategory==0)ImGui::TextDisabled("Tags follow authored move phases; confirm them for the chosen fighter.");
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
        const auto hype=ReadHypeProvider(c,data);ImGui::TextWrapped("Hype provider: %s",hype.message.c_str());
    } catch(const std::exception& e){transformStatus=e.what();ImGui::TextWrapped("%s",transformStatus.c_str());}
}
inline void DrawTransformation(const Character& c) {
    if(ImGui::CollapsingHeader("Transformation rules"))DrawTransformationEditor(c);
}
