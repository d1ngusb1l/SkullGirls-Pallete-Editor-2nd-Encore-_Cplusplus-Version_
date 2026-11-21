#include "pch.h"
#include "AdvancedPalette.h"
#include "PlayableCharactersManager.h"
#include "ProcessManager.h"
#include "AutoLoadPalette.h"
#include "Files/Config.h"
#include <set>
#include <map>
#include <array>
#include <functional>
#include <algorithm>

// Adapt the DLL's palette model to the shared native backend. The render
// callback owns all backend calls, including auto-load and palette switching.
struct Character {
    std::string Char_Name;
    int ID=0,Current_Pallete_Num=0,Num_Of_Color=0;
    std::vector<__int32> Character_Colors;
    __int32 LineColor=0,SuperShadowColor1=0,SuperShadowColor2=0;
};
struct GroupColorGroup {
    inline static const auto& characterGroups=GroupColorManager::GetInstance().GetAllGroups();
};
#define SGPE_DLL_HOST
#include "../../PalleteEditor/OverlayEditor.h"
#include "../../PalleteEditor/Data/AtomicFile.h"

namespace AdvancedPalette {
std::string Status;
namespace {
std::vector<DWORD> identity;
std::map<std::string,std::string> attempted;
ULONGLONG nextCheck=0;
Character Model(int slot) {
    DWORD root=OverlayEditor::Word(OverlayEditor::base+0x85ea4c);
    DWORD actor=OverlayEditor::Word(root+0x5e8+4*slot);
    if(!actor) throw std::runtime_error("Character is no longer loaded.");
    auto name=OverlayEditor::Read(actor+0x310,16);
    auto end=std::find(name.begin(),name.end(),0);
    if(end==name.end()) throw std::runtime_error("Invalid character identifier.");
    Character c; c.ID=slot;c.Char_Name=std::string(name.begin(),end);
    c.Current_Pallete_Num=OverlayEditor::Read(actor+0x330,1)[0];
    return c;
}
Character Current() {
    int slot=PlayableCharactersManager::GetCurrentCharacterIndex();
    if(slot<0 || slot>=6) throw std::runtime_error("Select a character first.");
    return Model(slot);
}
void Sync() {
    int slot=PlayableCharactersManager::GetCurrentCharacterIndex();
    if(slot>=0) PlayableCharactersManager::LoadCharacter(slot);
}
}
#ifdef SGPE_DLL_VALIDATION
#include "../../tests/DllPaletteSmoke.inl"
#endif
void ForgetMatch() {
    OverlayEditor::ForgetMatch();identity.clear();attempted.clear();nextCheck=0;
}
void ResetAutoLoad() {attempted.clear();nextCheck=0;}
void PaletteChanged() {
    try { OverlayEditor::Refresh();OverlayEditor::selectionKey.clear(); }
    catch(const std::exception& e) {Status=e.what();}
}
bool Load(const std::string& path) {
    try {
        auto c=Current();auto d=PaletteData::ReadFile(PaletteData::PathFromText(path));
        OverlayEditor::ApplyPalette(c,d);Sync();
        Status=d.legacy?"Legacy colors loaded; existing advanced assignments preserved.":"Colors and applied shader settings loaded.";
        return true;
    } catch(const std::exception& e) {Status=std::string("Load failed: ")+e.what();return false;}
}
bool Save(const std::string& path,bool legacy) {
    try {
        auto c=Current();auto d=OverlayEditor::CapturePalette(c);
        PaletteData::AtomicWrite(PaletteData::PathFromText(path),PaletteData::EncodePal(d,legacy));
        Status=legacy?"Legacy colors exported.":"Palette saved with applied shader settings.";return true;
    } catch(const std::exception& e) {Status=std::string("Save failed: ")+e.what();return false;}
}
void Tick() {
    try {
        auto& process=ProcessManager::instance();if(!process.s_SG_Process)return;
        OverlayEditor::Bind(process.s_SG_Process,process.s_BaseAddress);OverlayEditor::Tick();
        DWORD root=OverlayEditor::Word(process.s_BaseAddress+0x85ea4c);
        if(!root || OverlayEditor::Word(root+0x168)!=4) {if(!identity.empty())ForgetMatch();return;}
        std::vector<DWORD> current{root};
        for(int slot=0;slot<6;slot++) {auto actor=OverlayEditor::Word(root+0x5e8+4*slot);current.push_back(actor);current.push_back(actor?OverlayEditor::Word(actor+0x328):0);}
        if(identity!=current) {ForgetMatch();identity=current;}
        int selected=PlayableCharactersManager::GetCurrentCharacterIndex();
        if(selected>=0 && Model(selected).Current_Pallete_Num!=PlayableCharactersManager::GetCurrentCharacter().Current_Palette_Num)Sync();
        auto now=GetTickCount64();if(now<nextCheck)return;nextCheck=now+250;
#ifdef SGPE_DLL_VALIDATION
        if(std::filesystem::exists("dll-smoke.request")) {
            std::filesystem::remove("dll-smoke.request");RunDllSmoke();
        }
#endif
        std::set<std::pair<std::string,int>> entries;
        for(auto& entry:AutoPalette::Auto_Pals) {
            if(!entry.Enabled) {entry.Status="Disabled";continue;}
            if(!entries.insert({entry.CharName,entry.PalNum}).second){entry.Status="Duplicate assignment";continue;}
            bool found=false;
            for(int slot=0;slot<6;slot++) {
                if(!current[1+2*slot])continue;
                auto c=Model(slot);if(c.Char_Name!=entry.CharName || c.Current_Pallete_Num!=entry.PalNum)continue;
                found=true;
                try {
                    if(entry.PalPath.empty())throw std::runtime_error("Choose a .pal file.");
                    auto path=std::filesystem::absolute(PaletteData::PathFromText(entry.PalPath));
                    std::error_code error;auto time=std::filesystem::last_write_time(path,error);
                    auto fingerprint=PaletteData::PathText(path)+(error?":missing":":"+std::to_string(time.time_since_epoch().count()));
                    auto key=std::to_string(current[2+2*slot])+":"+std::to_string(entry.PalNum);
                    if(attempted.count(key) && attempted[key]==fingerprint)continue;
                    attempted[key]=fingerprint;
                    OverlayEditor::ApplyPalette(c,PaletteData::ReadFile(path));
                    if(slot==selected)Sync();entry.Status="Loaded colors and shader settings.";
                } catch(const std::exception& e) {entry.Status=std::string("Auto-load failed: ")+e.what();}
            }
            if(!found)entry.Status="Waiting for this character and palette number.";
        }
    } catch(const std::exception& e) {Status=e.what();}
}
void Draw() {
    if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());
    try {
        auto c=Current();OverlayEditor::Draw(OverlayEditor::process,OverlayEditor::base,c);
        // The shared controls may change alpha; keep the DLL color editor in sync.
        auto& dst=PlayableCharactersManager::GetCurrentCharacter();
        if(!c.Character_Colors.empty())dst.Character_Colors.assign(c.Character_Colors.begin(),c.Character_Colors.end());
    } catch(const std::exception& e) {ImGui::TextWrapped("%s",e.what());}
}
#include "GroupShaderControls.inl"
}
