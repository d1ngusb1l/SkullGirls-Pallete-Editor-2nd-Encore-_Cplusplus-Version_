inline nlohmann::json RunNativeTriggerSmoke(Character& c) {
    auto data=AppearanceData(c); DWORD address=ColorLinkAddress(c,data);
    auto original=Read(address,12),colors=Read(data.colors,4*data.colorCount);
    if(NativeTagHash("SUPER")!=0x0dd099a4 || NativeTagHash("ATTACK")!=0x9742798d || NativeTagHash("HITSTUN")!=0x26dcc58c)
        throw std::runtime_error("Native tag identifiers do not match asset evidence.");
    TransformSettings settings; settings.links={{3,4}}; settings.mode=TransformMode::Automatic;
    for(auto& trigger:nativeTriggers) {
        settings.activationTag=trigger.tag; ApplyTransformation(c,settings);
        DWORD condition=Word(Word(address+4)-44+12);
        auto node=Read(condition,48);
        if(node[0]!=1 || Word(condition+4)!=0x27 || node[16]!=0x24 || Word(condition+20)!=NativeTagHash(trigger.tag) || node[24]!=1 || node[32]!=0x20 || Word(condition+28))
            throw std::runtime_error("Native tag predicate record mismatch.");
        auto preset=TransformationPreset(c,settings);
        if(TransformationPreset(c,ReadTransformationPreset(c,nlohmann::json::parse(preset.dump())))!=preset)
            throw std::runtime_error("Native trigger preset round-trip failed.");
    }
    auto invalid=TransformationPreset(c,settings); invalid["activationTag"]="UNVERIFIED"; invalid["trigger"]="tag:UNVERIFIED";
    bool rejected=false; try { ReadTransformationPreset(c,invalid); } catch(...) { rejected=true; }
    if(!rejected) throw std::runtime_error("Unknown trigger accepted.");
    nlohmann::json legacy={{"version",1},{"character",c.Char_Name},{"trigger",TransformTrigger(c)},
                          {"previewMode",1},{"links",std::map<int,int>{{3,4}}},{"shadeMask",255}};
    auto oldSettings=ReadTransformationPreset(c,legacy);
    if(!oldSettings.activationTag.empty() || oldSettings.mode!=TransformMode::NormalPreview || oldSettings.links!=std::map<int,int>{{3,4}})
        throw std::runtime_error("Version-1 preset compatibility failed.");
    ResetTransformation(c); Sleep(500); Tick();
    if(Read(address,12)!=original || Read(data.colors,colors.size())!=colors || Read(base+0x87dc4,1)[0]!=0x84)
        throw std::runtime_error("Native tag test did not restore the palette.");
    return {{"passed",true},{"character",c.Char_Name},{"palette",c.Current_Pallete_Num+1},
            {"triggerCount",sizeof(nativeTriggers)/sizeof(nativeTriggers[0])},{"predicateRecordsAndPresets",true},{"version1PresetsSupported",true},{"invalidTriggerRejected",true},{"restored",true}};
}
inline nlohmann::json RunNativeTagLive(Character& c,const std::string& tag) {
    auto data=AppearanceData(c); DWORD root=Word(base+0x85ea4c),entity=Word(root+0x1d4+4*c.ID);
    if(!entity || Word(entity+0x330)!=data.actor) throw std::runtime_error("Tag in the selected character first.");
    DWORD address=ColorLinkAddress(c,data); auto before=Read(address,12),colors=Read(data.colors,4*data.colorCount);
    TransformSettings settings; settings.links={{3,30}}; settings.mode=TransformMode::Automatic; settings.activationTag=tag;
    ApplyTransformation(c,settings);
    nlohmann::json events=nlohmann::json::array(); bool activeSeen=false,inactiveSeen=false; int previous=-1; bool previousActive=false;
    ULONGLONG until=GetTickCount64()+45000;
    while(GetTickCount64()<until) {
        Tick(); if(!Valid({data.actor,data.palette,address,c.ID,{}})) throw std::runtime_error("Match changed during tag test.");
        float animation=0; auto bytes=Read(entity+4*0x27,4); memcpy(&animation,bytes.data(),4);
        auto states=Read(Word(address+4)-44+9,2); bool active=states[0]!=0 || states[1]!=0;
        activeSeen|=active; inactiveSeen|=!active; int state=(int)animation;
        if(state!=previous || active!=previousActive) {
            events.push_back({{"animation",state},{"nativeRuleActive",active}}); previous=state; previousActive=active;
        }
        Sleep(40);
    }
    ResetTransformation(c); Sleep(500); Tick();
    bool restored=Read(address,12)==before && Read(data.colors,colors.size())==colors && Read(base+0x87dc4,1)[0]==0x84;
    return {{"passed",activeSeen && inactiveSeen && restored},{"tag",tag},{"character",c.Char_Name},{"palette",c.Current_Pallete_Num+1},
            {"activeSeen",activeSeen},{"inactiveSeen",inactiveSeen},{"restored",restored},{"events",events}};
}
