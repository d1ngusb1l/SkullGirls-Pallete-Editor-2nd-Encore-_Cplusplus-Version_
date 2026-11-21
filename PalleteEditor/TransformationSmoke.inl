inline Bytes LastColorRule(DWORD address) {
    DWORD begin=Word(address),end=Word(address+4);
    if(end<begin+44 || (end-begin)%44) throw std::runtime_error("Expected a native color rule.");
    return Read(end-44,44);
}
inline void CheckLastColorRule(DWORD address,int source,int destination,int mask,bool conditional) {
    auto record=LastColorRule(address); DWORD s=0,d=0,condition=0;
    memcpy(&s,record.data(),4); memcpy(&d,record.data()+4,4); memcpy(&condition,record.data()+12,4);
    if(s!=(DWORD)source || d!=(DWORD)destination || record[8]!=mask || (condition!=0)!=conditional)
        throw std::runtime_error("Composed color rule does not match its assignment.");
}
inline nlohmann::json RunTransformationSmoke(Character& c) {
    if(c.Char_Name!="Annie") throw std::runtime_error("Conditional smoke test requires Annie.");
    auto data=AppearanceData(c); CheckAppearanceBand(data,26);
    DWORD address=ColorLinkAddress(c,data),outer=Word(data.palette+0x20);
    auto headers=Read(outer,12*Word(data.palette)); auto colors=Read(data.colors,4*data.colorCount);
    auto overlay=Read(Word(data.palette+0x14)+12*c.Current_Pallete_Num,12);
    auto hue=Read(RainbowAddress(c),16);
    TransformSettings settings; settings.links={{3,26},{4,26}};
    settings.mode=TransformMode::TransformedPreview; ApplyTransformation(c,settings);
    CheckLastColorRule(address,4,26,255,false);
    settings.mode=TransformMode::NormalPreview; ApplyTransformation(c,settings);
    if(Read(Word(address),Word(address+4)-Word(address)).size()!=
       [&]() { DWORD b=0,e=0; memcpy(&b,headers.data()+12*c.Current_Pallete_Num,4); memcpy(&e,headers.data()+12*c.Current_Pallete_Num+4,4); return e-b; }())
        throw std::runtime_error("Normal preview changed the native rule count.");
    settings.mode=TransformMode::Automatic; settings.shadeMask=0xa5; ApplyTransformation(c,settings);
    CheckLastColorRule(address,4,26,0xa5,true);
    DWORD pointer=Word(Word(address+4)-44+12); int comparisons=0;
    for(auto spec:std::array<std::pair<int,int>,3>{{{0x13,215},{0x14,239},{0x15,219}}}) {
        auto node=Read(pointer,48); float rhs=0; memcpy(&rhs,node.data()+20,4);
        if(node[0]!=1 || Word(pointer+4)!=0x27 || rhs!=(float)spec.second || node[24]!=1 || node[32]!=spec.first)
            throw std::runtime_error("Annie native super predicate mismatch.");
        pointer=Word(pointer+28); ++comparisons;
    }
    if(pointer || comparisons!=3) throw std::runtime_error("Unexpected condition chain.");
    auto preset=TransformationPreset(c,settings);
    auto decoded=ReadTransformationPreset(c,nlohmann::json::parse(preset.dump()));
    if(TransformationPreset(c,decoded)!=preset) throw std::runtime_error("Transformation preset round-trip failed.");
    auto invalid=preset; invalid["trigger"]="unvalidated-trigger"; bool rejected=false;
    try { ReadTransformationPreset(c,invalid); } catch(...) { rejected=true; }
    if(!rejected) throw std::runtime_error("Invalid automatic trigger accepted.");
    invalid=preset; invalid["links"]=std::map<int,int>{{3,256}}; rejected=false;
    try { ReadTransformationPreset(c,invalid); } catch(...) { rejected=true; }
    if(!rejected) throw std::runtime_error("Invalid transformed group accepted.");
    // Static and transformed assignments on the SAME source must remain
    // independent through either reset order and repeated Apply.
    colorLinks={{3,4}}; linkShadeMask=255; ApplyColorLinks(c);
    CheckLastColorRule(address,4,26,0xa5,true);
    ResetTransformation(c); CheckLastColorRule(address,3,4,255,false);
    ApplyTransformation(c,settings); ResetColorLinks(c);
    CheckLastColorRule(address,4,26,0xa5,true);
    settings.mode=TransformMode::TransformedPreview; ApplyTransformation(c,settings);
    DWORD length=Word(address+4)-Word(address); ApplyTransformation(c,settings);
    if(Word(address+4)-Word(address)!=length) throw std::runtime_error("Repeated transformation Apply duplicated rules.");
    ResetTransformation(c); Sleep(500); Tick();
    if(Read(outer,headers.size())!=headers || Read(data.colors,colors.size())!=colors ||
       Read(Word(data.palette+0x14)+12*c.Current_Pallete_Num,12)!=overlay)
        throw std::runtime_error("Transformation restoration or palette isolation failed.");
    auto hueAfter=Read(RainbowAddress(c),16); hueAfter[2]=hue[2]; hueAfter[3]=hue[3];
    if(hueAfter!=hue || Read(base+0x87dc4,1)[0]!=0x84 || colorAssignments.count(address))
        throw std::runtime_error("Transformation changed unrelated effect settings.");
    return {{"passed",true},{"character",c.Char_Name},{"palette",c.Current_Pallete_Num+1},
            {"manualPreviews",true},{"nativePredicateValidated",true},{"independentResetOrders",true},
            {"repeatApply",true},{"presetRoundTripAndValidation",true},{"otherEffectsPreserved",true},{"restored",true}};
}
inline nlohmann::json RunTransformationLive(Character& c) {
    if(c.Char_Name!="Annie") throw std::runtime_error("Live super test requires Annie.");
    auto data=AppearanceData(c); DWORD root=Word(base+0x85ea4c),entity=Word(root+0x1d4+4*c.ID);
    if(!entity || Word(entity+0x330)!=data.actor) throw std::runtime_error("Tag in the selected Annie before this test.");
    DWORD address=ColorLinkAddress(c,data); auto before=Read(address,12);
    auto colors=Read(data.colors,4*data.colorCount);
    TransformSettings settings; settings.links={{3,26},{4,26}}; settings.mode=TransformMode::Automatic;
    ApplyTransformation(c,settings);
    std::ofstream("transformation-live-ready.json")<<nlohmann::json({{"ready",true},{"character",c.Char_Name},{"palette",c.Current_Pallete_Num+1},{"seconds",45}}).dump(2);
    nlohmann::json events=nlohmann::json::array(); bool activeSeen=false,inactiveSeen=false;
    ULONGLONG until=GetTickCount64()+45000; int previous=-1; bool previousActive=false;
    while(GetTickCount64()<until) {
        Tick(); if(!Valid({data.actor,data.palette,address,c.ID,{}})) throw std::runtime_error("Match changed during live test.");
        float animation=0; auto bytes=Read(entity+4*0x27,4); memcpy(&animation,bytes.data(),4);
        DWORD last=Word(address+4)-44; auto states=Read(last+9,2); bool active=states[0]!=0 || states[1]!=0;
        int state=(int)animation; bool super=state>=215 && state<=239 && state!=219;
        if(super && active) activeSeen=true;
        if(!super && !active) inactiveSeen=true;
        if(state!=previous || active!=previousActive) {
            events.push_back({{"animation",state},{"nativeRuleActive",active},{"superPredicate",super}});
            previous=state; previousActive=active;
        }
        Sleep(40);
    }
    ResetTransformation(c); Sleep(500); Tick();
    bool restored=Read(address,12)==before && Read(data.colors,colors.size())==colors && Read(base+0x87dc4,1)[0]==0x84;
    return {{"passed",activeSeen && inactiveSeen && restored},{"character",c.Char_Name},{"palette",c.Current_Pallete_Num+1},
            {"activeDuringSuper",activeSeen},{"inactiveOutsideSuper",inactiveSeen},{"restored",restored},{"events",events}};
}
