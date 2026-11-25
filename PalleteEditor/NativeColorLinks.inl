// One composed native redirect table per palette: static links form the normal
// look, and active transformation rules override only their selected shades.
#include "NativeTriggers.inl"
enum class TransformMode { Automatic=0, NormalPreview=1, TransformedPreview=2 };
struct TransformSettings {
    std::map<int,int> links;
    std::string activationTag; // empty retains version-1 character profiles
    int shadeMask=255;
    TransformMode mode=TransformMode::NormalPreview;
    std::string id="legacy",name="Imported transformation";
    bool enabled=true;
    int hypeLevel=-1;
    std::string moveId;
    bool operator==(const TransformSettings&) const = default;
};
struct ColorAssignment {
    Original original;
    std::map<int,int> staticLinks;
    int staticMask=255;
    std::vector<TransformSettings> transformations;
    Bytes installedHeader;
};
inline std::map<DWORD,ColorAssignment> colorAssignments;
inline unsigned long long ruleGeneration=0;
inline void ClearColorAssignments() { colorAssignments.clear(); ++ruleGeneration; }
inline std::string TransformTrigger(const Character& c) {
    if(c.Char_Name=="Annie") return "annie-supers";
    if(c.Char_Name=="Filia" && c.Current_Pallete_Num==29) return "filia-color30-flag20";
    return "preview-only";
}
inline void ValidateLinks(const AppearanceContext& data,const std::map<int,int>& links,int mask) {
    if(mask<1 || mask>255) throw std::runtime_error("Select at least one shader shade.");
    for(auto& rule:links) {
        CheckAppearanceBand(data,rule.first); CheckAppearanceBand(data,rule.second);
        if(rule.first==rule.second) throw std::runtime_error("Source and destination groups must differ.");
    }
}
enum class HypeAvailability { Available, Unsupported, Unavailable };
struct HypeSnapshot {HypeAvailability availability=HypeAvailability::Unavailable;float amount=0;int level=-1;std::string message="Unavailable: actor ownership missing.";};
inline HypeSnapshot ReadHypeProvider(const Character& c,const AppearanceContext& data) {
    if(c.Char_Name!="Beowulf")return {HypeAvailability::Unsupported,0,-1,"Unsupported: Hype requires Beowulf."};
    try {
        const DWORD pe=Word(base+0x3c);
        if(pe>0x10000 || Word(base+pe+8)!=0x6af3723f || Word(base+pe+24+56)!=0x8bd000)return {HypeAvailability::Unsupported,0,-1,"Unsupported: Hype requires game build 26588330."};
        const DWORD root=Word(base+0x85ea4c);
        if(!root || Word(root+0x168)!=4 || c.ID<0 || c.ID>=6 || Word(root+0x5e8+4*c.ID)!=data.actor)return {};
        std::set<DWORD> entities;
        for(int slot=0;slot<6;++slot){const DWORD entity=Word(root+0x1d4+4*slot);if(entity && Word(entity+0x330)==data.actor)entities.insert(entity);}
        if(entities.size()!=1)return {};
        const DWORD entity=*entities.begin();float amount=0;auto bytes=Read(entity+0x190,4);memcpy(&amount,bytes.data(),4);
        const int level=PaletteData::BeowulfHypeLevel(amount);
        if(level<0)return {HypeAvailability::Unavailable,0,-1,"Unavailable: Hype sample outside validated range."};
        if(Word(base+0x85ea4c)!=root || Word(root+0x168)!=4 || Word(root+0x5e8+4*c.ID)!=data.actor || Word(entity+0x330)!=data.actor || Word(data.actor+0x328)!=data.palette)return {};
        return {HypeAvailability::Available,amount,level,"Available: completed Hype level "+std::to_string(level)+"."};
    } catch(...) {return {};}
}
inline void ValidateCharacterMoveSupport(const Character& c,const AppearanceContext& data,const std::string& id) {
    const auto move=PaletteData::FindCharacterMove(id);
    if(!move || move->character!=c.Char_Name)throw std::runtime_error("Unsupported: no mapped move for this character.");
    const DWORD pe=Word(base+0x3c);
    if(pe>0x10000 || Word(base+pe+8)!=0x6af3723f || Word(base+pe+24+56)!=0x8bd000)throw std::runtime_error("Unsupported: character moves require game build 26588330.");
    const DWORD root=Word(base+0x85ea4c);
    if(!root || Word(root+0x168)!=4 || c.ID<0 || c.ID>=6 || Word(root+0x5e8+4*c.ID)!=data.actor)throw std::runtime_error("Unavailable: move actor ownership missing.");
    std::set<DWORD> entities;
    for(int slot=0;slot<6;++slot){const DWORD entity=Word(root+0x1d4+4*slot);if(entity && Word(entity+0x330)==data.actor)entities.insert(entity);}
    if(entities.size()!=1)throw std::runtime_error("Unavailable: move entity ownership missing or ambiguous.");
    const DWORD entity=*entities.begin();float animation=0;auto bytes=Read(entity+4*0x27,4);memcpy(&animation,bytes.data(),4);
    // Initial catalogue contains only the 688-state researched Beowulf table.
    if(!std::isfinite(animation) || animation<0 || animation>=688 || std::floor(animation)!=animation)throw std::runtime_error("Unavailable: current animation is outside the mapped table.");
    if(Word(base+0x85ea4c)!=root || Word(root+0x168)!=4 || Word(root+0x5e8+4*c.ID)!=data.actor || Word(entity+0x330)!=data.actor || Word(data.actor+0x328)!=data.palette)throw std::runtime_error("Unavailable: move actor changed.");
}
inline void ValidateTransformation(const Character& c,const AppearanceContext& data,const TransformSettings& settings) {
    if(settings.links.empty()) throw std::runtime_error("Add at least one transformed color link.");
    ValidateLinks(data,settings.links,settings.shadeMask);
    if((int)settings.mode<0 || (int)settings.mode>2) throw std::runtime_error("Invalid transformation preview mode.");
    if(!settings.moveId.empty()) {
        if(settings.mode!=TransformMode::Automatic || settings.hypeLevel!=-1 || !settings.activationTag.empty())throw std::runtime_error("Character moves cannot combine with other triggers or live overrides.");
        ValidateCharacterMoveSupport(c,data,settings.moveId);return;
    }
    if(settings.hypeLevel!=-1) {
        if(settings.hypeLevel<0 || settings.hypeLevel>3 || settings.mode!=TransformMode::Automatic || !settings.activationTag.empty())throw std::runtime_error("Invalid or mixed Beowulf Hype predicate.");
        const auto sample=ReadHypeProvider(c,data);
        if(sample.availability!=HypeAvailability::Available)throw std::runtime_error(sample.message);
        return;
    }
    if(!settings.activationTag.empty() && !KnownNativeTrigger(settings.activationTag)) throw std::runtime_error("Unknown native animation trigger.");
    if(settings.mode==TransformMode::Automatic && settings.activationTag.empty() && TransformTrigger(c)=="preview-only")
        throw std::runtime_error("Automatic activation is validated for Annie and Filia Color 30 only. Use a manual preview here.");
}
inline DWORD TransformCondition(const Character& c,const TransformSettings& settings,int rangeIndex=0) {
    if(!settings.moveId.empty()) {
        const auto move=PaletteData::FindCharacterMove(settings.moveId);
        if(!move || move->character!=c.Char_Name || rangeIndex<0 || rangeIndex>=(int)move->ranges.size())throw std::runtime_error("Unsupported character move predicate.");
        const auto range=move->ranges[rangeIndex];DWORD next=0;
        for(auto comparison:std::array<std::pair<int,int>,2>{{{0x14,range.second},{0x13,range.first}}}) {
            Bytes node(48);node[0]=1;Put(node,4,0x27);float rhs=(float)comparison.second;memcpy(node.data()+20,&rhs,4);
            node[24]=1;Put(node,28,next);node[32]=static_cast<unsigned char>(comparison.first);
            const DWORD memory=Allocate(node.size());Write(memory,node);next=memory;
        }
        return next;
    }
    if(settings.hypeLevel>=0) {
        DWORD next=0;const auto comparisons=PaletteData::HypeComparisons(settings.hypeLevel);
        for(auto it=comparisons.rbegin();it!=comparisons.rend();++it) {
            Bytes node(48);node[0]=1;Put(node,4,0x64);memcpy(node.data()+20,&it->rhs,4);
            node[24]=1;Put(node,28,next);node[32]=static_cast<unsigned char>(it->opcode);
            const DWORD memory=Allocate(node.size());Write(memory,node);next=memory;
        }
        return next;
    }
    if(!settings.activationTag.empty()) {
        auto node=NativeTagCondition(settings.activationTag); DWORD memory=Allocate(node.size()); Write(memory,node); return memory;
    }
    if(TransformTrigger(c)=="annie-supers") {
        DWORD next=0;
        // Native linked comparisons are ANDed: animation >= 215, <= 239,
        // != 219 (the post-Autograph dispatcher). Each rule owns its own chain.
        for(auto comparison:std::array<std::pair<int,int>,3>{{{0x15,219},{0x14,239},{0x13,215}}}) {
            Bytes node(48); node[0]=1; Put(node,4,0x27);
            float rhs=(float)comparison.second; memcpy(node.data()+20,&rhs,4);
            node[24]=1; Put(node,28,next); node[32]=(unsigned char)comparison.first;
            DWORD memory=Allocate(node.size()); Write(memory,node); next=memory;
        }
        return next;
    }
    if(TransformTrigger(c)=="filia-color30-flag20") {
        Bytes node(48); node[0]=0x20; float flag=20; memcpy(node.data()+4,&flag,4);
        node[24]=1; node[32]=0x15;
        DWORD memory=Allocate(node.size()); Write(memory,node); return memory;
    }
    throw std::runtime_error("No validated automatic trigger for this character and palette.");
}
inline ColorAssignment CurrentColorAssignment(const Character& c,const AppearanceContext& data) {
    DWORD address=ColorLinkAddress(c,data);
    auto found=colorAssignments.find(address);
    if(found!=colorAssignments.end()) {
        if(!Valid(found->second.original)) throw std::runtime_error("Match changed; retry in the new match.");
        return found->second;
    }
    ColorAssignment assignment; assignment.original={data.actor,data.palette,address,c.ID,Read(address,12)};
    auto saved=originals.find(address); if(saved!=originals.end()) assignment.original=saved->second;
    return assignment;
}
inline void CommitColorAssignment(const Character& c,const AppearanceContext& data,const ColorAssignment& assignment) {
    DWORD address=assignment.original.address;
    if(!Valid(assignment.original)) throw std::runtime_error("Match changed; retry in the new match.");
    std::vector<PaletteData::Transformation> rules;
    for(const auto& settings:assignment.transformations) {
        ValidateTransformation(c,data,settings);
        rules.push_back({settings.links,settings.shadeMask,(int)settings.mode,settings.activationTag,"",settings.id,settings.name,settings.enabled,settings.hypeLevel,settings.moveId});
    }
    PaletteData::ValidateRuleConflicts(rules);
    ValidateLinks(data,assignment.staticLinks,assignment.staticMask);
    const auto current=colorAssignments.find(address);
    if(current!=colorAssignments.end() && current->second.staticLinks==assignment.staticLinks && current->second.staticMask==assignment.staticMask &&
        current->second.transformations==assignment.transformations && Read(address,12)==current->second.installedHeader) return;
    const bool restoring=assignment.staticLinks.empty() && assignment.transformations.empty();
    DWORD begin=0,end=0;
    memcpy(&begin,assignment.original.header.data(),4); memcpy(&end,assignment.original.header.data()+4,4);
    if(end<begin || (end-begin)%44 || end-begin>44*1024) throw std::runtime_error("Unsupported native color-link vector.");
    const auto plan=PaletteData::BuildRedirectPlan(Read(begin,end-begin),assignment.staticLinks,assignment.staticMask,rules);
    Bytes records;std::map<std::pair<int,int>,DWORD> conditions;
    for(auto entry:plan) {
        if(entry.conditionRule>=0) {
            auto& condition=conditions[{entry.conditionRule,entry.conditionRange}];
            if(!condition)condition=TransformCondition(c,assignment.transformations.at(entry.conditionRule),entry.conditionRange);
            PaletteData::RedirectPut(entry.bytes.data()+12,condition);entry.bytes[16]=1;
        }
        records.insert(records.end(),entry.bytes.begin(),entry.bytes.end());
    }
    // A normal preview can contain no records on palettes without native links.
    Bytes header(12);
    if(restoring)header=assignment.original.header;
    else if(!records.empty()) {
        DWORD memory=Allocate(records.size()); Write(memory,records);
        Put(header,0,memory); Put(header,4,memory+(DWORD)records.size()); Put(header,8,memory+(DWORD)records.size());
    }
    const auto previousHeader=Read(address,12);
    auto previousOriginals=originals; auto previousAssignments=colorAssignments;
    try {
        AppearanceTransaction(assignment.original,[&]() {
            if(Word(data.actor+0x328)!=data.palette || Read(data.actor+0x330,1)[0]!=c.Current_Pallete_Num) throw std::runtime_error("Character palette data changed.");
            Write(address,header);
            if(Read(address,12)!=header) throw std::runtime_error("Color-link write verification failed.");
            if(restoring){originals.erase(address);colorAssignments.erase(address);}
            else {
                originals[address]=assignment.original;
                auto installed=assignment; installed.installedHeader=header; colorAssignments[address]=installed;
            }
        });
        Refresh();
    } catch(const std::exception& error) {
        auto message=std::string(error.what());
        try {
            AppearanceTransaction(assignment.original,[&]() { Write(address,previousHeader); if(Read(address,12)!=previousHeader)throw std::runtime_error("Restore verification failed."); });
            originals=previousOriginals; colorAssignments=previousAssignments; Refresh();
        } catch(...) { throw std::runtime_error(message+" Color-link restoration also failed; match may have ended."); }
        throw std::runtime_error(message+" Previous color links restored.");
    }
}
inline void ApplyColorLinks(const Character& c) {
    auto data=AppearanceData(c);
    if(colorLinks.empty()) throw std::runtime_error("Add at least one always-on color link.");
    ValidateLinks(data,colorLinks,linkShadeMask);
    auto assignment=CurrentColorAssignment(c,data); assignment.staticLinks=colorLinks; assignment.staticMask=linkShadeMask;
    CommitColorAssignment(c,data,assignment); appearanceStatus="Always-on color links applied; transformation settings preserved.";
}
inline void ResetColorLinks(const Character& c) {
    auto data=AppearanceData(c); auto assignment=CurrentColorAssignment(c,data);
    assignment.staticLinks.clear(); CommitColorAssignment(c,data,assignment);
    appearanceStatus="Always-on links reset; transformation settings preserved.";
}
inline void ApplyTransformation(const Character& c,const TransformSettings& settings) {
    auto data=AppearanceData(c); ValidateTransformation(c,data,settings);
    auto assignment=CurrentColorAssignment(c,data);
    auto found=std::find_if(assignment.transformations.begin(),assignment.transformations.end(),[&](const auto& rule){return rule.id==settings.id;});
    if(found==assignment.transformations.end()) assignment.transformations.push_back(settings); else *found=settings;
    CommitColorAssignment(c,data,assignment);
}
inline void ResetTransformation(const Character& c,const std::string& id="legacy") {
    auto data=AppearanceData(c); auto assignment=CurrentColorAssignment(c,data);
    assignment.transformations.erase(std::remove_if(assignment.transformations.begin(),assignment.transformations.end(),[&](const auto& rule){return rule.id==id;}),assignment.transformations.end()); CommitColorAssignment(c,data,assignment);
}
