#include "Data/PaletteEntryPatch.h"
#include <windows.h>
#include <cstring>
#include <iostream>
#include <map>
using namespace PaletteData;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class F>void Reject(F action){bool rejected=false;try{action();}catch(...){rejected=true;}Check(rejected,"Expected palette entry rejection");}
using NativeFn=uintptr_t(__thiscall*)(void*,int,void*,void*);
NativeFn original=nullptr;
unsigned calls=0;
uintptr_t observedSelf=0,observedCache=0,observedEntity=0;int observedIndex=0;
uintptr_t __fastcall Observer(void* self,void*,int index,void* cache,void* entity) {
    ++calls;observedSelf=(uintptr_t)self;observedIndex=index;observedCache=(uintptr_t)cache;observedEntity=(uintptr_t)entity;
    return original(self,index,cache,entity);
}
int main(){try {
    std::map<uint32_t,unsigned char> memory;
    auto plan=PlanPaletteEntry(0x1000,0x8000,0x4000);
    auto read=[&](uint32_t address,size_t count){EntryBytes bytes(count);for(size_t i=0;i<count;++i)bytes[i]=memory[address+(uint32_t)i];return bytes;};
    bool failEntry=false,failDelegate=false,corrupt=false;int entryWrites=0;
    auto write=[&](uint32_t address,const EntryBytes& bytes){
        if(address==plan.entry){++entryWrites;if(failEntry){failEntry=false;memory[address]=bytes[0];throw std::runtime_error("Injected partial entry write");}}
        if(address==plan.trampoline && failDelegate)throw std::runtime_error("Injected delegate failure");
        for(size_t i=0;i<bytes.size();++i)memory[address+(uint32_t)i]=bytes[i];
        if(address==plan.entry && corrupt){corrupt=false;memory[address+5]^=1;}
    };
    write(plan.entry,plan.original);entryWrites=0;failDelegate=true;Reject([&]{InstallPaletteEntry(plan,read,write);});
    Check(entryWrites==0 && read(plan.entry,6)==plan.original,"Delegate failure touched native entry");failDelegate=false;
    failEntry=true;Reject([&]{InstallPaletteEntry(plan,read,write);});Check(read(plan.entry,6)==plan.original,"Partial install did not restore all six native bytes");
    corrupt=true;Reject([&]{InstallPaletteEntry(plan,read,write);});Check(read(plan.entry,6)==plan.original,"Verification failure did not restore entry");
    memory[plan.entry]=0xcc;const auto before=entryWrites;Reject([&]{InstallPaletteEntry(plan,read,write);});Check(entryWrites==before,"Unknown signature was overwritten");write(plan.entry,plan.original);
    InstallPaletteEntry(plan,read,write);failEntry=true;Reject([&]{RemovePaletteEntry(plan,read,write);});Check(read(plan.entry,6)==plan.redirect,"Partial removal lost the installed observer");
    memory[plan.entry]=0xcc;const auto external=read(plan.entry,6);Reject([&]{RemovePaletteEntry(plan,read,write);});Check(read(plan.entry,6)==external,"External entry writer was overwritten");write(plan.entry,plan.redirect);RemovePaletteEntry(plan,read,write);
    Reject([]{PlanPaletteEntry(0,1,2);});Reject([]{PlanPaletteEntry(0x1000,0x1002,0x4000);});Reject([]{PlanPaletteEntry(0x1000,0x8000,0x1001);});
    // Execute the exact six-byte prologue and trampoline in private test memory.
    // Synthetic thiscall body returns ECX + all three stack arguments and ret 12.
    auto page=(unsigned char*)VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    Check(page!=nullptr,"Could not allocate synthetic function");
    EntryBytes body={0x53,0x8b,0xdc,0x83,0xec,0x08,0x8b,0xc1,0x03,0x43,0x08,0x03,0x43,0x0c,0x03,0x43,0x10,0x8b,0xe3,0x5b,0xc2,0x0c,0x00};
    memcpy(page,body.data(),body.size());auto function=(NativeFn)page;
    Check(function((void*)3,5,(void*)7,(void*)11)==26,"Synthetic baseline ABI failed");
    auto real=PlanPaletteEntry((uint32_t)(uintptr_t)page,(uint32_t)(uintptr_t)&Observer,(uint32_t)(uintptr_t)(page+128));
    auto realRead=[](uint32_t at,size_t count){EntryBytes result(count);memcpy(result.data(),(void*)(uintptr_t)at,count);return result;};
    auto realWrite=[](uint32_t at,const EntryBytes& bytes){memcpy((void*)(uintptr_t)at,bytes.data(),bytes.size());FlushInstructionCache(GetCurrentProcess(),(void*)(uintptr_t)at,bytes.size());};
    original=(NativeFn)(page+128);InstallPaletteEntry(real,realRead,realWrite);
    for(int repeat=0;repeat<10000;++repeat)Check(function((void*)3,5,(void*)7,(void*)11)==26,"Observer changed original return or stack cleanup");
    Check(calls==10000 && observedSelf==3 && observedIndex==5 && observedCache==7 && observedEntity==11,"Observer did not preserve palette/entity arguments");
    RemovePaletteEntry(real,realRead,realWrite);Check(function((void*)3,5,(void*)7,(void*)11)==26 && calls==10000,"Removal did not restore direct native call");
    VirtualFree(page,0,MEM_RELEASE);
    std::cout<<"Palette entry tests passed: full instructions, ABI/stack/arguments, signature guards, partial writes, rollback and restoration.\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}}
