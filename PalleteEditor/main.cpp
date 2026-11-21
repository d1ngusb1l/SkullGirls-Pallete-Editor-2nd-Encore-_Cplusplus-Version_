#include <thread>
#include "UI.h"
#include "Config.h"
#include "Memory.h"
#include "OverlayEditor.h"
namespace OverlayEditor {
#include "AppearanceSmoke.inl"
#include "TransformationSmoke.inl"
#include "NativeTriggerSmoke.inl"
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
    wchar_t module[MAX_PATH]{}; GetModuleFileNameW(nullptr,module,MAX_PATH);
    SetCurrentDirectoryW(std::filesystem::path(module).parent_path().c_str());
    if(wcsstr(lpCmdLine,L"/overlay-smoke") || wcsstr(lpCmdLine,L"/rainbow-smoke") || wcsstr(lpCmdLine,L"/appearance-smoke") || wcsstr(lpCmdLine,L"/transformation-smoke") || wcsstr(lpCmdLine,L"/transformation-live") || wcsstr(lpCmdLine,L"/native-trigger-smoke") || wcsstr(lpCmdLine,L"/tag-live")) {
        bool nativeTriggers=wcsstr(lpCmdLine,L"/native-trigger-smoke")!=nullptr;
        bool tagLive=wcsstr(lpCmdLine,L"/tag-live")!=nullptr;
        std::string liveTag=wcsstr(lpCmdLine,L"/tag-live-jumping")?"JUMPING":wcsstr(lpCmdLine,L"/tag-live-crouching")?"CROUCHING":"SUPER";
        bool transformation=wcsstr(lpCmdLine,L"/transformation-smoke")!=nullptr;
        bool live=wcsstr(lpCmdLine,L"/transformation-live")!=nullptr;
        bool appearance=wcsstr(lpCmdLine,L"/appearance-smoke")!=nullptr;
        bool rainbow=wcsstr(lpCmdLine,L"/rainbow-smoke")!=nullptr;
        nlohmann::json result; HANDLE h=nullptr;
        try {
            DWORD pid=Memory::FindProcessId(L"Skullgirls.exe"), base=Memory::GetModuleBaseAddress(pid,L"Skullgirls.exe");
            h=OpenProcess(PROCESS_ALL_ACCESS,FALSE,pid); if(!h) throw std::runtime_error("Game access failed");
            OverlayEditor::Bind(h,base); OverlayEditor::Catalog();
            Character c{}; DWORD root=OverlayEditor::Word(base+0x85ea4c); bool found=false;
            for(int slot=0;slot<6;slot++) {
                DWORD actor=OverlayEditor::Word(root+0x5e8+4*slot); if(!actor) continue;
                auto name=OverlayEditor::Read(actor+0x310,32);
                if((nativeTriggers || tagLive)?std::string((char*)name.data())=="Valentine":(transformation || live)?std::string((char*)name.data())=="Annie":slot==0) { c.ID=slot; c.Char_Name=std::string((char*)name.data()); c.Current_Pallete_Num=OverlayEditor::Read(actor+0x330,1)[0]; found=true; break; }
            }
            if(!found || !OverlayEditor::templates.count("Fire")) throw std::runtime_error("Smoke test needs an active match and the Fire catalog.");
            DWORD actor=OverlayEditor::Word(root+0x5e8+4*c.ID),pal=OverlayEditor::Word(actor+0x328),address=OverlayEditor::Word(pal+0x14)+12*c.Current_Pallete_Num;
            if(nativeTriggers || tagLive) { result=tagLive?OverlayEditor::RunNativeTagLive(c,liveTag):OverlayEditor::RunNativeTriggerSmoke(c); } else if(transformation || live) { result=live?OverlayEditor::RunTransformationLive(c):OverlayEditor::RunTransformationSmoke(c); } else if(appearance) { result=OverlayEditor::RunAppearanceSmoke(c); } else {
            if(rainbow) address=OverlayEditor::RainbowAddress(c);
            auto before=OverlayEditor::Read(address,rainbow?16:12);
            if(rainbow) {
                OverlayEditor::rainbowBands={13}; OverlayEditor::ApplyRainbow(c);
                Sleep(500); OverlayEditor::Tick();
                auto first=OverlayEditor::Read(address,16); Sleep(500); auto second=OverlayEditor::Read(address,16);
                if(first[0]!=4 || first[1]!=8 || first[2]==second[2] || OverlayEditor::Read(OverlayEditor::Word(address+4),1)[0]!=13) throw std::runtime_error("Rainbow animation or group assignment mismatch");
            } else {
                OverlayEditor::selected={13}; OverlayEditor::texture="Fire"; OverlayEditor::tint=13;
                OverlayEditor::Apply(c); Sleep(500); OverlayEditor::Tick();
                DWORD record=OverlayEditor::Word(address);
                if(OverlayEditor::Read(record+4,4)!=OverlayEditor::Bytes{'F','i','r','e'} || OverlayEditor::Read(OverlayEditor::Word(record+32),1)[0]!=13) throw std::runtime_error("Applied record mismatch");
            }
            OverlayEditor::Remove(address); Sleep(500); OverlayEditor::Tick();
            auto after=OverlayEditor::Read(address,before.size());
            // PausedWrite verifies the complete restored record while paused.
            // The running game updates these two animation bytes even with a
            // zero increment, so only configuration is stable after 500 ms.
            if(rainbow) { before[2]=after[2]; before[3]=after[3]; }
            if(after!=before || OverlayEditor::Read(base+0x87dc4,1)[0]!=0x84) throw std::runtime_error("Restoration mismatch: "+nlohmann::json({{"before",before},{"after",after},{"branch",OverlayEditor::Read(base+0x87dc4,1)}}).dump());
            result={{"passed",true},{"character",c.Char_Name},{"palette",c.Current_Pallete_Num+1},{"effect",rainbow?"Rainbow":"Fire"},{"band",13},{"restored",true}};
            }
        } catch(const std::exception& e) { result={{"passed",false},{"error",e.what()}}; }
        OverlayEditor::Shutdown(); if(h) CloseHandle(h);
        std::ofstream(tagLive?("tag-live-"+liveTag+".json"):nativeTriggers?"native-trigger-smoke.json":live?"transformation-live.json":transformation?"transformation-smoke.json":appearance?"appearance-smoke.json":rainbow?"rainbow-smoke.json":"overlay-smoke.json")<<result.dump(2); return result.value("passed",false)?0:1;
    }
    config::init();
    if(GroupColorGroup::characterGroups.empty()) {
        try {
            ordered_json j; std::ifstream f("groups.json"); f>>j;
            for(auto& item:j.items()) { int start=1; for(auto& group:item.value().items()) {
                int count=group.value(); GroupColorGroup::characterGroups[item.key()].push_back({group.key(),start,count}); start+=count;
            }}
        } catch(...) {}
    }
    UI::Render();
    return 0;
}
