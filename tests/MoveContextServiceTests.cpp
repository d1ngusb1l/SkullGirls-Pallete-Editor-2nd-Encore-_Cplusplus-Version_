#include "Data/MoveContextService.h"
#include "Data/CharacterMoveRoutes.h"
#include <cstdlib>
#include <iostream>
#include <new>
using namespace PaletteData;
static size_t allocations=0;
void* operator new(size_t size){++allocations;if(void* value=std::malloc(size?size:1))return value;throw std::bad_alloc();}
void operator delete(void* value) noexcept{std::free(value);}
void operator delete(void* value,size_t) noexcept{std::free(value);}
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try {
    auto table=BuildCharacterMoveRoutes("Filia");MoveContextService service;
    MoveContextBinding a;a.actor=2;a.palette=4;a.headerAddress=10;a.slot=0;a.paletteIndex=4;a.character="Filia";a.table=&table;
    a.gates={{100,table.Find("filia/trap-jaw")},{101,table.Find("filia/fenrir")}};
    auto b=a;b.actor=5;b.palette=7;b.slot=3;b.paletteIndex=2;b.headerAddress=11;b.gates={{102,table.Find("filia/trap-jaw")},{103,table.Find("filia/fenrir")}};
    auto alternate=a;alternate.paletteIndex=5;alternate.headerAddress=12;alternate.gates={{104,table.Find("filia/fenrir")}};
    std::array<float,5> values={1,1,1,1,1};std::array<bool,3> headers={true,true,true};int writes=0;bool fail=false;
    auto live=[&](uintptr_t address,const auto&){return headers.at(address-10);};
    auto write=[&](uintptr_t address,float value){++writes;if(fail){fail=false;return false;}values.at(address-100)=value;return true;};
    MoveContextFrame p1{{1,2,3,4,1,0},"Filia",4,20,20},p2{{1,5,6,7,1,3},"Filia",2,20,20};
    service.Replace({a,b,alternate});service.Observe(p1,live,write);
    Check(values[0]==0 && values[1]==0 && values[4]==0,"First idle snapshot failed to clear unknown gate contents");
    p1.animation=185;p1.previous=184;service.Observe(p1,live,write);p1.animation=167;p1.previous=185;service.Observe(p1,live,write);p1.previous=167;service.Observe(p1,live,write);
    Check(values[0]==0 && values[1]==1 && values[4]==1,"Fenrir gate did not persist through repeated shared loops or inactive palette");
    p2.animation=166;p2.previous=165;service.Observe(p2,live,write);p2.animation=167;p2.previous=166;service.Observe(p2,live,write);
    Check(values[2]==1 && values[3]==0 && values[1]==1,"Opposing fighters shared route gates");
    const auto stable=writes;const auto allocated=allocations;
    for(int repeat=0;repeat<10000;++repeat){service.Observe(p1,live,write);service.Observe(p2,live,write);}
    Check(writes==stable && allocations==allocated,"Stable observations wrote gates or allocated memory");
    p1.paletteIndex=5;service.Observe(p1,live,write);Check(values[4]==1,"Palette selection lost logical move context");
    // Rebinding existing native gate nodes must force their first value, even if
    // the new CPU registration would otherwise assume they already contained zero.
    service.Replace({a,b,alternate});p1.animation=203;p1.previous=167;service.Observe(p1,live,write);
    Check(values[0]==0 && values[1]==0 && values[4]==0,"Configuration resync left an old active native gate");
    p1.animation=167;p1.previous=167;service.Observe(p1,live,write);Check(service.Status(0).status==MoveRouteStatus::Unavailable && values[1]==0,"Unknown shared origin did not fail closed");
    p1.animation=185;p1.previous=184;service.Observe(p1,live,write);headers[0]=headers[2]=false;service.Observe(p1,live,write);
    Check(service.Status(0).status==MoveRouteStatus::Unavailable && values[1]==0 && values[4]==0 && values[2]==1,"Stale header affected the wrong fighter or remained active");
    headers[0]=headers[2]=true;p1.animation=167;p1.previous=167;service.Observe(p1,live,write);Check(values[1]==0,"Restored header inherited stale route context");
    p1.animation=185;p1.previous=184;service.Observe(p1,live,write);p1.animation=203;p1.previous=185;fail=true;service.Observe(p1,live,write);
    Check(service.Status(0).status==MoveRouteStatus::Unavailable && values[1]==0 && values[4]==0,"Failed gate update was reported active or not retried closed");
    p1.animation=185;p1.previous=184;service.Observe(p1,live,write);++p1.identity.generation;p1.animation=167;p1.previous=167;service.Observe(p1,live,write);Check(values[1]==0,"New match inherited old route");
    service.Invalidate(-1,write);Check(values==std::array<float,5>{0,0,0,0,0},"Global cleanup left native gates active");
    std::cout<<"Move context service tests passed: mirrors, shared loops, palette changes, resync, stale headers, failure closure and allocation/write-free stable updates.\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}}
