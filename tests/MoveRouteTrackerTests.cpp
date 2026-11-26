#include "Data/MoveRouteTracker.h"
#include <cstdlib>
#include <iostream>
#include <new>
using namespace PaletteData;
static size_t allocations=0;
void* operator new(size_t size){++allocations;if(void* value=std::malloc(size?size:1))return value;throw std::bad_alloc();}
void operator delete(void* value) noexcept{std::free(value);}
void operator delete(void* value,size_t) noexcept{std::free(value);}
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class F>void Reject(F action){bool rejected=false;try{action();}catch(...){rejected=true;}Check(rejected,"Expected route-table rejection");}
int main(){try {
    // Current/previous assignments observed in controlled Filia Updo and whiffed
    // Fenrir traces. Both history values become 167 after the shared loop repeats.
    MoveRouteTable table(529,{{"updo",{{161,168}}},{"fenrir",{{167,168},{182,190}}}},
                         {{161,164},{164,165},{165,166},{166,167},{182,183},{183,183},{183,184},{184,185},{185,167},{167,167},{167,168},{168,20}});
    MoveRouteIdentity p1{1,2,3,4,1,0},p2{1,5,6,7,1,3};
    MoveRouteTracker tracker,mirror;
    const auto updo=tracker.Observe(table,p1,161,20);Check(updo.status==MoveRouteStatus::Active && table.Id(updo.route)=="updo","Updo entry was not recognized");
    for(auto [animation,prior]:std::vector<std::pair<int,int>>{{164,161},{165,164},{166,165},{167,166},{167,167},{167,167},{168,167}}) {
        auto result=tracker.Observe(table,p1,animation,prior);Check(result.status==MoveRouteStatus::Active && table.Id(result.route)=="updo","Updo lost its shared loop/landing route");
    }
    Check(tracker.Observe(table,p1,20,168).status==MoveRouteStatus::Inactive,"Idle retained a move");
    for(auto [animation,prior]:std::vector<std::pair<int,int>>{{182,22},{183,182},{183,183},{184,183},{185,184},{167,185},{167,167},{167,167},{168,167}}) {
        auto result=tracker.Observe(table,p1,animation,prior);Check(result.status==MoveRouteStatus::Active && table.Id(result.route)=="fenrir","Fenrir changed into Updo during its shared fall/landing");
    }
    Check(tracker.Observe(table,p1,203,168).status==MoveRouteStatus::Inactive,"Hitstun did not clear the route");
    Check(tracker.Observe(table,p1,167,167).status==MoveRouteStatus::Unavailable,"Unknown shared-state origin was guessed");
    auto inferred=tracker.Observe(table,p1,167,185);Check(table.Id(inferred.route)=="fenrir","Verified immediate incoming route could not seed tracking");
    auto opposite=mirror.Observe(table,p2,167,166);Check(table.Id(opposite.route)=="updo" && table.Id(tracker.Observe(table,p1,167,167).route)=="fenrir","Mirror route leaked between fighters");
    auto nextMatch=p1;++nextMatch.generation;Check(tracker.Observe(table,nextMatch,167,167).status==MoveRouteStatus::Unavailable,"Reused addresses inherited the previous match route");
    tracker.Observe(table,p1,185,184);Check(tracker.Observe(table,p1,168,167).status==MoveRouteStatus::Unavailable,"A skipped/unproven shared path inherited stale context");
    tracker.Observe(table,p1,166,165);Check(table.Id(tracker.Observe(table,p1,167,185).route)=="fenrir","New verified incoming route did not replace stale context");
    Check(tracker.Observe(table,p2,167,167).status==MoveRouteStatus::Unavailable,"Actor/palette change retained route state");
    auto invalid=p1;invalid.entity=0;Check(tracker.Observe(table,invalid,161,20).status==MoveRouteStatus::Unavailable,"Missing actor identity accepted");
    Check(tracker.Observe(table,p1,529,20).status==MoveRouteStatus::Unavailable,"Invalid animation accepted");
    const auto before=allocations;
    for(int repeat=0;repeat<10000;++repeat){tracker.Observe(table,p1,185,184);tracker.Observe(table,p1,167,185);tracker.Observe(table,p1,167,167);tracker.Observe(table,p1,168,167);tracker.Observe(table,p1,20,168);tracker.Reset();}
    Check(allocations==before,"Steady-state route tracking allocated memory");
    Reject([]{MoveRouteTable bad(0,{{"a",{{0,0}}}},{});});
    Reject([]{MoveRouteTable bad(4,{{"a",{{0,4}}}},{});});
    Reject([]{MoveRouteTable bad(4,{{"a",{{0,1},{1,2}}}},{});});
    Reject([]{MoveRouteTable bad(4,{{"a",{{0,0}}},{"a",{{1,1}}}},{});});
    Reject([]{MoveRouteTable bad(4,{{"a",{{0,0}}}},{{0,4}});});
    std::cout<<"Move route tests passed: recorded shared loops/landing, interruption, mirrors, generation/owner changes, unavailable origins, bounds and allocation-free updates.\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}}
