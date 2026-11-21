// Animation tags use the game's case-sensitive DJB2-xor identifier. These names
// were matched against authored tag lists in Annie, Filia and Valentine FOITs.
struct NativeTrigger { const char* tag; const char* label; };
inline const NativeTrigger nativeTriggers[]={
    {"SUPER","Super move"},{"ATTACK","Any attack"},{"NORMAL","Normal attack"},
    {"SPECIAL","Special move"},{"HITSTUN","Hitstun"},{"BLOCKSTUN","Blockstun"},
    {"BLOCKING","Blocking"},{"PUSHBLOCK","Pushblock"},{"KNOCKDOWN","Knockdown"},
    {"TAUNT","Taunt"},{"THROW","Throw states"},
    {"STANDING","Standing states"},{"CROUCHING","Crouching states"},
    {"IDLE","Idle / landing recovery"},{"DASH","Dash"},{"AIRDASH","Air dash"},{"JUMP","Jump takeoff"},{"JUMPING","Jumping / air normals"},
    {"DOUBLEJUMP","Double jump"},{"SUPERJUMP","Super jump"},{"ASSIST","Assist"},
    {"BURST","Burst"},{"OTG","OTG states"},{"WALLBOUNCE","Wall bounce"},{"DEAD","Dead"}
};
inline bool KnownNativeTrigger(const std::string& tag) {
    for(auto& trigger:nativeTriggers) if(tag==trigger.tag) return true;
    return false;
}
inline DWORD NativeTagHash(const std::string& tag) {
    DWORD hash=5381; for(unsigned char ch:tag) hash=(hash*33)^ch; return hash;
}
inline const char* NativeTriggerLabel(const std::string& tag) {
    for(auto& trigger:nativeTriggers) if(tag==trigger.tag) return trigger.label;
    return "Legacy character trigger";
}
inline Bytes NativeTagCondition(const std::string& tag) {
    if(!KnownNativeTrigger(tag)) throw std::runtime_error("Unknown native animation trigger.");
    Bytes node(48);
    node[0]=1; Put(node,4,0x27); // current animation index
    node[16]=0x24; Put(node,20,NativeTagHash(tag)); // raw tag identifier
    node[24]=1; node[32]=0x20; // native HasAnimationTag comparison
    return node;
}
