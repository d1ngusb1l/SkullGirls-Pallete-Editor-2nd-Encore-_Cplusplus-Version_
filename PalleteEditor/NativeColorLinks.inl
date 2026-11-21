// One composed native redirect table per palette: static links form the normal
// look, and active transformation rules override only their selected shades.
#include "NativeTriggers.inl"
enum class TransformMode { Automatic=0, NormalPreview=1, TransformedPreview=2 };
struct TransformSettings {
    std::map<int,int> links;
    std::string activationTag; // empty retains version-1 character profiles
    int shadeMask=255;
    TransformMode mode=TransformMode::NormalPreview;
};
struct ColorAssignment {
    Original original;
    std::map<int,int> staticLinks;
    int staticMask=255;
    bool hasTransformation=false;
    TransformSettings transformation;
};
inline std::map<DWORD,ColorAssignment> colorAssignments;
inline void ClearColorAssignments() { colorAssignments.clear(); }
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
inline void ValidateTransformation(const Character& c,const AppearanceContext& data,const TransformSettings& settings) {
    if(settings.links.empty()) throw std::runtime_error("Add at least one transformed color link.");
    ValidateLinks(data,settings.links,settings.shadeMask);
    if((int)settings.mode<0 || (int)settings.mode>2) throw std::runtime_error("Invalid transformation preview mode.");
    if(!settings.activationTag.empty() && !KnownNativeTrigger(settings.activationTag)) throw std::runtime_error("Unknown native animation trigger.");
    if(settings.mode==TransformMode::Automatic && settings.activationTag.empty() && TransformTrigger(c)=="preview-only")
        throw std::runtime_error("Automatic activation is validated for Annie and Filia Color 30 only. Use a manual preview here.");
}
inline DWORD TransformCondition(const Character& c,const TransformSettings& settings) {
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
    if(assignment.staticLinks.empty() && !assignment.hasTransformation) {
        Remove(address); colorAssignments.erase(address); return;
    }
    DWORD begin=0,end=0;
    memcpy(&begin,assignment.original.header.data(),4); memcpy(&end,assignment.original.header.data()+4,4);
    if(end<begin || (end-begin)%44 || end-begin>44*1024) throw std::runtime_error("Unsupported native color-link vector.");
    auto native=Read(begin,end-begin); Bytes records;
    for(size_t offset=0;offset<native.size();offset+=44) {
        DWORD source=0; memcpy(&source,native.data()+offset,4);
        if(!assignment.staticLinks.count((int)source)) records.insert(records.end(),native.begin()+offset,native.begin()+offset+44);
    }
    auto append=[&](int source,int destination,int mask,DWORD condition) {
        Bytes record(44); Put(record,0,source); Put(record,4,destination);
        record[8]=(unsigned char)mask; Put(record,12,condition); record[16]=condition?1:0; Put(record,40,15);
        records.insert(records.end(),record.begin(),record.end());
    };
    for(auto& rule:assignment.staticLinks) append(rule.first,rule.second,assignment.staticMask,0);
    if(assignment.hasTransformation && assignment.transformation.mode!=TransformMode::NormalPreview) {
        for(auto& rule:assignment.transformation.links) {
            DWORD condition=assignment.transformation.mode==TransformMode::Automatic?TransformCondition(c,assignment.transformation):0;
            append(rule.first,rule.second,assignment.transformation.shadeMask,condition);
        }
    }
    // A normal preview can contain no records on palettes without native links.
    Bytes header(12);
    if(!records.empty()) {
        DWORD memory=Allocate(records.size()); Write(memory,records);
        Put(header,0,memory); Put(header,4,memory+(DWORD)records.size()); Put(header,8,memory+(DWORD)records.size());
    }
    AppearanceTransaction(assignment.original,[&]() {
        if(Word(data.actor+0x328)!=data.palette) throw std::runtime_error("Character palette data changed.");
        originals[address]=assignment.original; Write(address,header);
        if(Read(address,12)!=header) throw std::runtime_error("Color-link write verification failed.");
        colorAssignments[address]=assignment;
    });
    Refresh();
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
    auto assignment=CurrentColorAssignment(c,data); assignment.hasTransformation=true; assignment.transformation=settings;
    CommitColorAssignment(c,data,assignment);
}
inline void ResetTransformation(const Character& c) {
    auto data=AppearanceData(c); auto assignment=CurrentColorAssignment(c,data);
    assignment.hasTransformation=false; CommitColorAssignment(c,data,assignment);
}
