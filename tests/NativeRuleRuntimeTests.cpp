// Exercises the actual native table owner with a bounded fake process memory.
#include "Data/PaletteDocument.h"
#include "Data/NativeRedirectPlan.h"
#include <array>
#include <cstring>
#include <functional>
#include <iostream>
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
    const auto generation=ruleGeneration;ClearColorAssignments();Check(ruleGeneration!=generation && colorAssignments.empty(),"Match cleanup retained rules");
    std::cout<<"Native rule runtime checks passed: independent updates, idempotency, conflicts, partial writes, refresh rollback, reset rollback, restoration and stale identity.\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}}
