// Exercises the actual native table owner with a bounded fake process memory.
#include "Data/PaletteDocument.h"
#include "Data/NativeRedirectPlan.h"
#include "Data/BeowulfHype.h"
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
int allocations=0;
Bytes Read(DWORD address,size_t size) {Bytes result;for(size_t i=0;i<size;++i)result.push_back(memory.at(address+(DWORD)i));return result;}
void Write(DWORD address,const Bytes& bytes) {
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
    const auto generation=ruleGeneration;ClearColorAssignments();Check(ruleGeneration!=generation && colorAssignments.empty(),"Match cleanup retained rules");
    std::cout<<"Native rule runtime checks passed: independent updates, idempotency, conflicts, partial writes, refresh rollback, reset rollback, restoration and stale identity.\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}}
