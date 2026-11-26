// Exercises the actual native table owner with a bounded fake process memory.
#include "Data/PaletteDocument.h"
#include "Data/NativeRedirectPlan.h"
#include "Data/BeowulfHype.h"
#include "Data/MoveContextService.h"
#include "Data/CharacterMoveRoutes.h"
#include <array>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
using DWORD=uint32_t;
using Bytes=std::vector<unsigned char>;
struct Character {std::string Char_Name="Annie";int ID=0,Current_Pallete_Num=0;};
struct Original {DWORD actor,palette,address;int slot;Bytes header;};
struct AppearanceContext {DWORD actor=0x1000,palette=0x2000;};
std::map<DWORD,unsigned char> memory;
std::map<DWORD,Original> originals;
std::map<int,int> colorLinks;
int linkShadeMask=255;
std::string appearanceStatus;
bool valid=true,failWrite=false,failRefresh=false;
DWORD base=0x400000;
DWORD nextAllocation=0x10000;
int allocations=0,writes=0;
Bytes Read(DWORD address,size_t size) {Bytes result;for(size_t i=0;i<size;++i)result.push_back(memory.at(address+(DWORD)i));return result;}
void Write(DWORD address,const Bytes& bytes) {
    ++writes;
    if(address==0x3000 && failWrite){failWrite=false;memory[address]=0xff;throw std::runtime_error("Injected partial header write");}
    for(size_t i=0;i<bytes.size();++i)memory[address+(DWORD)i]=bytes[i];
}
DWORD Word(DWORD address){return PaletteData::RedirectWord(Read(address,4).data());}
void Put(Bytes& bytes,size_t offset,DWORD value){PaletteData::RedirectPut(bytes.data()+offset,value);}
DWORD Allocate(size_t size){++allocations;auto address=nextAllocation;nextAllocation+=(DWORD)size+16;Write(address,Bytes(size));return address;}
bool Valid(const Original&){return valid;}
void Refresh(){if(failRefresh){failRefresh=false;throw std::runtime_error("Injected refresh failure");}}
void AppearanceTransaction(const Original& original,const std::function<void()>& update){if(!Valid(original))throw std::runtime_error("Stale actor");update();}
AppearanceContext AppearanceData(const Character&){if(!valid)throw std::runtime_error("Stale actor");return {};}
DWORD ColorLinkAddress(const Character&,const AppearanceContext&){return 0x3000;}
void CheckAppearanceBand(const AppearanceContext&,int band){if(band<1 || band>8)throw std::runtime_error("Absent band");}
#include "NativeColorLinks.inl"
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void Reject(const std::function<void()>& action){bool rejected=false;try{action();}catch(...){rejected=true;}Check(rejected,"Expected failure");}
int main(){try {
    Character c;AppearanceContext context;Write(0x3000,Bytes(12));Bytes palette(4);Put(palette,0,context.palette);Write(context.actor+0x328,palette);Write(context.actor+0x330,{0});
    const auto original=Read(0x3000,12);
    TransformSettings a;a.id="a";a.name="Attack";a.links={{1,2}};a.activationTag="ATTACK";a.mode=TransformMode::Automatic;
    auto b=a;b.id="b";b.name="Jump";b.links={{3,4}};b.activationTag="JUMPING";
    ApplyTransformation(c,a);ApplyTransformation(c,b);
    auto address=DWORD(0x3000);Check(Word(address+4)-Word(address)==88,"Two rules did not coexist");
    auto assigned=colorAssignments.at(address);Check(assigned.transformations[0]==a && assigned.transformations[1]==b,"Rule values changed");
    const int beforeRepeat=allocations;ApplyTransformation(c,b);Check(allocations==beforeRepeat,"Identical Apply allocated memory");
    const auto before=Read(address,12);a.links={{1,4}};failWrite=true;
    Reject([&]{ApplyTransformation(c,a);});Check(Read(address,12)==before && colorAssignments.at(address).transformations==assigned.transformations,"Partial write rollback lost header or rules");
    failRefresh=true;Reject([&]{ApplyTransformation(c,a);});Check(Read(address,12)==before && colorAssignments.at(address).transformations==assigned.transformations,"Refresh rollback lost header or rules");
    auto overlap=b;overlap.links={{1,4}};const int beforeConflict=allocations;Reject([&]{ApplyTransformation(c,overlap);});Check(allocations==beforeConflict && Read(address,12)==before,"Conflict mutated or allocated before rejection");
    a=assigned.transformations[0];a.enabled=false;ApplyTransformation(c,a);
    Check(Word(address+4)-Word(address)==44 && colorAssignments.at(address).transformations[1]==b,"Disabling A changed B");
    auto record=Read(Word(address),44);Check(PaletteData::RedirectWord(record.data())==3 && Word(PaletteData::RedirectWord(record.data()+12)+20)==NativeTagHash("JUMPING"),"B condition changed when A disabled");
    ResetTransformation(c,"a");Check(colorAssignments.at(address).transformations.size()==1 && colorAssignments.at(address).transformations[0]==b,"Removing A changed B");
    colorLinks={{5,6}};ApplyColorLinks(c);ResetTransformation(c,"b");Check(colorAssignments.at(address).staticLinks==colorLinks,"Removing B lost static links");
    const auto beforeReset=Read(address,12);failWrite=true;Reject([&]{ResetColorLinks(c);});Check(Read(address,12)==beforeReset && colorAssignments.count(address),"Reset rollback lost assignment");
    ResetColorLinks(c);Check(Read(address,12)==original && !colorAssignments.count(address) && !originals.count(address),"Reset did not restore original header");
    ApplyTransformation(c,b);auto stale=CurrentColorAssignment(c,context);valid=false;const auto staleAllocations=allocations;Reject([&]{CommitColorAssignment(c,context,stale);});Check(allocations==staleAllocations,"Stale actor allocated resources");valid=true;
    ResetTransformation(c,"b");c.Char_Name="Beowulf";
    auto writeWord=[](DWORD address,DWORD value){Bytes bytes(4);Put(bytes,0,value);Write(address,bytes);};
    writeWord(base+0x3c,0x80);writeWord(base+0x80+8,0x6af3723f);writeWord(base+0x80+24+56,0x8bd000);
    const DWORD root=0x5000,entity=0x6000;writeWord(base+0x85ea4c,root);writeWord(root+0x168,4);
    for(int slot=0;slot<6;++slot){writeWord(root+0x5e8+4*slot,slot==0?context.actor:0);writeWord(root+0x1d4+4*slot,slot==0?entity:0);}
    writeWord(entity+0x330,context.actor);
    auto writeAmount=[&](float value){Bytes bytes(4);memcpy(bytes.data(),&value,4);Write(entity+0x190,bytes);};writeAmount(2.34f);
    auto snapshot=ReadHypeProvider(c,context);Check(snapshot.availability==HypeAvailability::Available && snapshot.level==2,"Hype ownership/boundary check failed");
    const DWORD otherActor=0x1800,otherEntity=0x6800;writeWord(root+0x5e8+4*3,otherActor);writeWord(root+0x1d4+4*3,otherEntity);writeWord(otherEntity+0x330,otherActor);writeWord(otherActor+0x328,0x2800);
    Bytes mirrorAmount(4);float three=3;memcpy(mirrorAmount.data(),&three,4);Write(otherEntity+0x190,mirrorAmount);
    Character mirror=c;mirror.ID=3;AppearanceContext mirrorContext;mirrorContext.actor=otherActor;mirrorContext.palette=0x2800;
    Check(ReadHypeProvider(mirror,mirrorContext).level==3 && ReadHypeProvider(c,context).level==2,"Mirror resources shared an owner");
    auto resource=b;resource.id="hype-2";resource.name="Hype 2";resource.activationTag.clear();resource.hypeLevel=2;
    ApplyTransformation(c,resource);auto condition=Word(Word(address)+12);
    Check(Word(condition+4)==0x64 && Read(condition+32,1)[0]==0x13,"Hype lower comparison uses the wrong operand/opcode");
    auto upper=Word(condition+28);float threshold=0;auto rhs=Read(upper+20,4);memcpy(&threshold,rhs.data(),4);
    Check(Word(upper+4)==0x64 && Read(upper+32,1)[0]==0x10 && threshold==3.f && !Word(upper+28),"Hype upper comparison is not bounded/exclusive");
    auto another=resource;another.id="hype-3";another.hypeLevel=3;ApplyTransformation(c,another);
    Check(colorAssignments.at(address).transformations.size()==2,"Exclusive Hype levels could not share a source");
    const int stableHypeAllocations=allocations;ApplyTransformation(c,another);Check(allocations==stableHypeAllocations,"Repeated Hype Apply allocated memory");
    auto zero=resource;zero.id="hype-0";zero.hypeLevel=0;ApplyTransformation(c,zero);auto one=resource;one.id="hype-1";one.hypeLevel=1;ApplyTransformation(c,one);
    auto evaluate=[&](DWORD node,float value) {
        int count=0;bool active=true;
        while(node){Check(++count<=2,"Unbounded Hype graph");Check(Word(node+4)==0x64 && Read(node,1)[0]==1 && Read(node+16,1)[0]==0 && Read(node+24,1)[0]==1,"Hype operand shape changed");
            float rhs=0;auto raw=Read(node+20,4);memcpy(&rhs,raw.data(),4);const auto opcode=Read(node+32,1)[0];
            active=active && (opcode==0x13?value>=rhs:opcode==0x10?value<rhs:opcode==0x14?value<=rhs:false);node=Word(node+28);
        }
        Check(count==2,"Hype graph lost a bound");return active;
    };
    for(float amount:{-1.f,0.f,0.999f,1.f,1.5f,2.f,2.34f,3.f,3.01f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) {
        int matches=0;for(DWORD record=Word(address);record<Word(address+4);record+=44)if(evaluate(Word(record+12),amount))++matches;
        Check(matches==(PaletteData::BeowulfHypeLevel(amount)>=0?1:0),"Native Hype graphs overlap or activate on invalid input");
    }
    const auto beforeHypeFailure=Read(address,12);const auto beforeHypeAllocations=allocations;
    writeAmount(-1);Reject([&]{ApplyTransformation(c,resource);});Check(ReadHypeProvider(c,context).availability==HypeAvailability::Unavailable && allocations==beforeHypeAllocations && Read(address,12)==beforeHypeFailure,"Invalid resource sample mutated appearance");writeAmount(2);
    writeWord(entity+0x330,0x1111);Check(ReadHypeProvider(c,context).availability==HypeAvailability::Unavailable,"Wrong actor resource was accepted");writeWord(entity+0x330,context.actor);
    writeWord(base+0x80+8,0);Check(ReadHypeProvider(c,context).availability==HypeAvailability::Unsupported,"Unsupported build accepted");writeWord(base+0x80+8,0x6af3723f);
    c.Char_Name="Annie";Check(ReadHypeProvider(c,context).availability==HypeAvailability::Unsupported,"Hype leaked to another character");Reject([&]{ApplyTransformation(c,resource);});c.Char_Name="Beowulf";
    auto clean=CurrentColorAssignment(c,context);clean.transformations.clear();CommitColorAssignment(c,context,clean);
    auto writeAnimation=[&](DWORD target,float value){Bytes bytes(4);memcpy(bytes.data(),&value,4);Write(target+4*0x27,bytes);};
    writeAnimation(entity,21);writeAnimation(otherEntity,234);
    auto chair=b;chair.id="chair";chair.name="Chair Toss";chair.activationTag.clear();chair.links={{1,2}};chair.moveId="beowulf/chair-toss";
    auto arm=chair;arm.id="arm";arm.name="Gigantic Arm";arm.moveId="beowulf/organic-arm";arm.links={{1,4}};
    ApplyTransformation(c,chair);ApplyTransformation(c,arm);
    Check(Word(address+4)-Word(address)==88,"Exclusive mapped moves could not share a source");
    const int stableMoveAllocations=allocations;ApplyTransformation(c,arm);Check(allocations==stableMoveAllocations,"Repeated move Apply allocated memory");
    auto evaluateMove=[&](DWORD node,float animation) {
        bool active=true;int nodes=0;
        while(node){Check(++nodes<=2,"Unbounded move predicate");Check(Word(node+4)==0x27 && Read(node,1)[0]==1 && Read(node+16,1)[0]==0 && Read(node+24,1)[0]==1,"Move condition operand shape changed");
            float rhs=0;auto raw=Read(node+20,4);memcpy(&rhs,raw.data(),4);const auto opcode=Read(node+32,1)[0];
            Check(opcode==(nodes==1?0x13:0x14),"Move bounds use the wrong native comparisons");
            active=active && (opcode==0x13?animation>=rhs:animation<=rhs);node=Word(node+28);
        }
        Check(nodes==2,"Move graph lost a bound");return active;
    };
    for(int animation=0;animation<688;++animation) {
        const auto start=Word(address);const bool chairActive=evaluateMove(Word(start+12),(float)animation),armActive=evaluateMove(Word(start+44+12),(float)animation);
        Check(chairActive==(animation>=191 && animation<=198),"Chair startup/recovery or interruption boundary failed");
        Check(armActive==(animation>=234 && animation<=236),"Arm startup/loop/end or interruption boundary failed");
        Check(!(chairActive && armActive),"Mapped moves overlap");
    }
    for(float invalid:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-1.f})Check(!evaluateMove(Word(Word(address)+12),invalid),"Move condition accepts invalid animation");
    ValidateCharacterMoveSupport(mirror,mirrorContext,chair.moveId);
    const auto moveHeader=Read(address,12);const int moveAllocations=allocations;
    auto unknown=chair;unknown.moveId="other/move";Reject([&]{ApplyTransformation(c,unknown);});
    auto mixed=chair;mixed.hypeLevel=1;Reject([&]{ApplyTransformation(c,mixed);});
    c.Char_Name="Annie";Reject([&]{ApplyTransformation(c,chair);});c.Char_Name="Beowulf";
    writeWord(entity+0x330,otherActor);Reject([&]{ApplyTransformation(c,chair);});writeWord(entity+0x330,context.actor);
    writeWord(base+0x80+8,0);Reject([&]{ApplyTransformation(c,chair);});writeWord(base+0x80+8,0x6af3723f);
    for(float invalid:{-1.f,688.f,1.5f,std::numeric_limits<float>::quiet_NaN()}){writeAnimation(entity,invalid);Reject([&]{ApplyTransformation(c,chair);});}writeAnimation(entity,21);
    Check(allocations==moveAllocations && Read(address,12)==moveHeader,"Invalid move provider mutated appearance");
    auto changedArm=arm;changedArm.links={{1,3}};failWrite=true;Reject([&]{ApplyTransformation(c,changedArm);});
    Check(Read(address,12)==moveHeader && colorAssignments.at(address).transformations[1]==arm,"Move rollback lost header or independent rules");
    chair.enabled=false;ApplyTransformation(c,chair);Check(Word(address+4)-Word(address)==44 && colorAssignments.at(address).transformations[1]==arm,"Disabling Chair Toss changed Arm");
    ResetTransformation(c,"chair");ResetTransformation(c,"arm");Check(Read(address,12)==original,"Removing moves failed to restore native header");
    std::set<std::string> rosterChecked;bool checkedGapOwner=false;
    for(const auto& move:PaletteData::CharacterMoves()) {
        if(move.contextual)continue; // Shared phases require the coordinator tested below.
        c.Char_Name=move.character;ValidateCharacterMoveSupport(c,context,move.id);
        auto setting=chair;setting.id="roster";setting.enabled=true;setting.moveId=move.id;
        std::vector<DWORD> nodes;for(int range=0;range<(int)move.ranges.size();++range)nodes.push_back(TransformCondition(c,setting,range));
        for(int animation=0;animation<move.animationCount;++animation) {
            bool actual=false,expected=false;
            for(auto node:nodes)actual=actual || evaluateMove(node,(float)animation);
            for(auto range:move.ranges)expected=expected || (animation>=range.first && animation<=range.second);
            Check(actual==expected,"Roster native range union gained a gap or lost a move phase");
        }
        if(rosterChecked.insert(move.character).second) {
            auto rosterMirror=c;rosterMirror.ID=3;ValidateCharacterMoveSupport(rosterMirror,mirrorContext,move.id);
            writeAnimation(entity,(float)move.animationCount);Reject([&]{ValidateCharacterMoveSupport(c,context,move.id);});writeAnimation(entity,21);
        }
        if(!checkedGapOwner && move.ranges.size()>1) {
            ApplyTransformation(c,setting);const auto begin=Word(address),end=Word(address+4);
            Check(end-begin==44*move.ranges.size(),"Composed move owner lost disjoint ranges");
            for(int animation=0;animation<move.animationCount;++animation) {
                int active=0;for(DWORD record=begin;record<end;record+=44)if(evaluateMove(Word(record+12),(float)animation))++active;
                bool expected=false;for(auto range:move.ranges)expected=expected || (animation>=range.first && animation<=range.second);
                Check(active==(expected?1:0),"Composed move table activates a gap or overlaps its own ranges");
            }
            ResetTransformation(c,"roster");Check(Read(address,12)==original,"Multi-range owner did not restore original header");checkedGapOwner=true;
        }
    }
    Check(rosterChecked.size()==18 && checkedGapOwner,"Roster predicates did not exercise every character and disjoint ranges");
    c.Char_Name="Filia";writeAnimation(entity,20);
    auto large=CurrentColorAssignment(c,context);large.transformations.clear();
    for(const auto& move:PaletteData::CharacterMoves())if(move.character=="Filia" && !move.contextual && large.transformations.size()<16) {
        auto rule=chair;rule.id="large-"+std::to_string(large.transformations.size());rule.moveId=move.id;rule.enabled=true;rule.links.clear();
        for(int source=1;source<8;++source)rule.links[source]=8;
        large.transformations.push_back(rule);
    }
    CommitColorAssignment(c,context,large);
    const auto largeData=Word(address),largeSize=Word(address+8)-largeData;
    Check(largeSize>=4096 && largeData%32==0,"Large native vector does not satisfy teardown alignment");
    const auto largeOwner=Word(largeData-4);
    Check(largeData>=largeOwner+4 && largeData-largeOwner-4<=31,"Native teardown cannot recover the original large allocation");
    large.transformations.clear();CommitColorAssignment(c,context,large);Check(Read(address,12)==original,"Large redirect table failed to restore the native header");
    c.Char_Name="Filia";writeAnimation(entity,20);
    auto updo=chair;updo.id="updo";updo.enabled=true;updo.moveId="filia/trap-jaw";
    const int beforeUnsupported=allocations;Reject([&]{ApplyTransformation(c,updo);});Check(allocations==beforeUnsupported,"Unsupported context allocated native nodes");
    static auto filiaRoutes=PaletteData::BuildCharacterMoveRoutes("Filia");
    static PaletteData::MoveContextService service;
    static bool observerInstalled=false,failInstall=false;
    moveContextHooks={[](){return true;},[](){return observerInstalled;},[](){if(failInstall){failInstall=false;throw std::runtime_error("Injected install failure");}observerInstalled=true;},[](){observerInstalled=false;},
        [](){service.Replace(ContextBindings());},[](){service.Abandon();},[](const std::string&)->const PaletteData::MoveRouteTable*{return &filiaRoutes;},nullptr};
    failInstall=true;Reject([&]{ApplyTransformation(c,updo);});Check(Read(address,12)==original && colorAssignments.empty() && !observerInstalled,"Observer install rollback lost original header");
    ApplyTransformation(c,updo);Check(observerInstalled,"Context observer was not installed");
    const auto& binding=colorAssignments.at(address);Check(binding.contextGates.size()==1,"Context did not allocate exactly one route gate");
    auto contextLower=Word(Word(address)+12),contextUpper=Word(contextLower+28),gateNode=Word(contextUpper+28);
    Check(contextLower && contextUpper && gateNode && !Word(gateNode+28) && Read(gateNode,1)[0]==0 && gateNode+4==binding.contextGates[0].address,"Native context chain lost its owned constant gate");
    auto fenrir=updo;fenrir.id="fenrir";fenrir.moveId="filia/fenrir";fenrir.links={{1,4}};ApplyTransformation(c,fenrir);
    auto live=[](uintptr_t a,const std::array<unsigned char,12>& expected){return Read((DWORD)a,12)==Bytes(expected.begin(),expected.end());};
    auto writeGate=[](uintptr_t a,float value){Bytes bytes(4);memcpy(bytes.data(),&value,4);Write((DWORD)a,bytes);return true;};
    PaletteData::MoveContextFrame frame{{root,context.actor,entity,context.palette,ruleGeneration,0},"Filia",0,182,20};
    for(auto [current,previous]:std::vector<std::pair<int,int>>{{182,20},{183,182},{184,183},{185,184},{167,185},{167,167},{168,167}}) {
        frame.animation=current;frame.previous=previous;service.Observe(frame,live,writeGate);
        const auto& gates=colorAssignments.at(address).contextGates;Check(gates.size()==3,"Each redirect range must own its own gate");
        for(size_t i=0;i<gates.size();++i){float value=0;auto raw=Read((DWORD)gates[i].address,4);memcpy(&value,raw.data(),4);Check(value==(i?1.f:0.f),"Shared Fenrir loop activated Updo or lost its route");}
    }
    const int stableContext=allocations;ApplyTransformation(c,fenrir);Check(allocations==stableContext,"Repeated context Apply allocated native nodes");
    frame.animation=20;frame.previous=168;service.Observe(frame,live,writeGate);
    for(const auto& g:colorAssignments.at(address).contextGates)Check(Word((DWORD)g.address)==0,"Idle retained active move gate");
    ResetTransformation(c,"updo");Check(observerInstalled,"Removing one route removed observer needed by another");
    ResetTransformation(c,"fenrir");Check(!observerInstalled && Read(address,12)==original,"Last context removal failed to restore palette/observer");
    // Reproduce the native teardown ownership requirement: two links and two
    // ranges require four fully independent graphs, including constant tails.
    fenrir.links={{1,4},{3,4}};ApplyTransformation(c,fenrir);
    Check(colorAssignments.at(address).contextGates.size()==4,"Multi-link/range records share a route gate");
    std::set<DWORD> released;
    for(DWORD rec=Word(address);rec<Word(address+4);rec+=44) {
        int chainLength=0;
        for(DWORD node=Word(rec+12);node;) {
            Check(++chainLength<=3 && released.insert(node).second,"Native cleanup would double-free a shared condition node");
            const DWORD next=Word(node+28);for(int byte=0;byte<48;++byte)memory.erase(node+byte);node=next;
        }
        Check(chainLength==3,"Context record lost its bounds or constant gate");
    }
    Check(released.size()==12,"Teardown did not cover every independent native graph");
    valid=false;const auto writesBeforeCleanup=writes;
    const auto generation=ruleGeneration;ClearColorAssignments();
    Check(ruleGeneration!=generation && colorAssignments.empty() && !observerInstalled,"Match cleanup retained rules/observer");
    Check(writes==writesBeforeCleanup,"Match cleanup wrote into already released condition memory");
    std::cout<<"Native rule runtime checks passed: independent updates, idempotency, conflicts, partial writes, refresh rollback, reset rollback, restoration and stale identity.\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}}
