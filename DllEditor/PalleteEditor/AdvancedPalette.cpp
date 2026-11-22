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
#include "../../PalleteEditor/OverlayControls.h"

#include "DirectXWrapper/LiveOverlays.h"
#ifdef SGPE_DLL_VALIDATION
void CancelOverlayDrawPreview();
#endif

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
#include "../../tests/DllOverlayProbe.inl"
#endif
namespace {
using OverlayCompletion=std::function<void(const std::string&)>;
struct NativeOverlayOverride {
    DWORD palette;
    int paletteNumber;
    OverlayEditor::Bytes original;
    std::shared_ptr<OverlayEditor::Bytes> written;
};
std::map<DWORD,NativeOverlayOverride> nativeDualOverlays;
std::map<DWORD,NativeOverlayOverride> retiredDualOverlays;

void RestoreRetiredOverlays(const std::vector<DWORD>& match) {
    using namespace OverlayEditor;
    bool restored=false;
    for(const auto& [address,entry]:retiredDualOverlays) {
        for(int slot=0;slot<6;slot++) {
            if(match[2+2*slot]!=entry.palette)continue;
            if(entry.paletteNumber<static_cast<int>(Word(entry.palette)) &&
                Word(entry.palette+0x14)+12*entry.paletteNumber==address &&
                Read(address,12)==*entry.written) {
                PausedWrite(address,entry.original);
                restored=true;
            }
            break;
        }
    }
    retiredDualOverlays.clear();
    if(restored)Refresh();
}

void ApplyDualOverlay(Character c,PaletteData::Document document,OverlayCompletion report) {
    using namespace OverlayEditor;
    if(LiveOverlays::Busy())throw std::runtime_error("Wait for the current overlay operation.");
    if(!std::filesystem::exists("overlay-shader/prototype.bin"))
        throw std::runtime_error("Missing overlay-shader/prototype.bin. Install the shader included with the DLL.");
    ValidatePalette(c,document,true);
    const auto& primary=*document.overlay;
    const auto& secondary=*document.secondOverlay;
    auto data=AppearanceData(c);
    const DWORD address=Word(data.palette+0x14)+12*c.Current_Pallete_Num;
    Original owner{data.actor,data.palette,address,c.ID,Read(address,12)};
    auto marker=primary;
    marker.texture=primary.texture=="Fire"?"Water":"Fire";
    const auto markerHeader=OverlayHeader(marker);
    const auto targetHeader=OverlayHeader(primary,&secondary);
    auto installed=std::make_shared<Bytes>(markerHeader);
    auto applied=std::make_shared<Bytes>();
    const auto previousEntry=nativeDualOverlays.find(address);
    const auto previous=previousEntry==nativeDualOverlays.end()?std::optional<NativeOverlayOverride>{}:previousEntry->second;
    auto current=[owner,c]() {
        try {return Valid(owner) && Read(owner.actor+0x330,1)[0]==c.Current_Pallete_Num;}
        catch(...) {return false;}
    };
    auto replace=[owner](const Bytes& header) {PausedWrite(owner.address,header);Refresh();};
    LiveOverlays::Request request{c.ID,c.Current_Pallete_Num,primary,secondary,
        [current,owner,installed]() {return current() && Read(owner.address,12)==*installed;},
        [replace,targetHeader,installed]() {
            *installed=targetHeader;
            replace(targetHeader);
        },
        [owner,c,document=std::move(document),replace,installed,applied,report,previous](const std::string& error) mutable {
            auto restoreTracking=[&]() {
                if(!Valid(owner) || Read(owner.address,12)!=owner.header)return;
                if(previous)nativeDualOverlays.insert_or_assign(owner.address,*previous);
                else nativeDualOverlays.erase(owner.address);
            };
            try {
                if(Valid(owner) && Read(owner.address,12)==*installed)replace(owner.header);
                if(!error.empty()) {restoreTracking();report(error);return;}
                OverlayEditor::ApplyPalette(c,document,true);
                *applied=Read(owner.address,12);
                nativeDualOverlays.insert_or_assign(owner.address,NativeOverlayOverride{
                    owner.palette,c.Current_Pallete_Num,originals.at(owner.address).header,applied});
                if(c.ID==PlayableCharactersManager::GetCurrentCharacterIndex())Sync();
                report("");
            }catch(const std::exception& e) {
                try {restoreTracking();}catch(...) {}
                report(e.what());
                if(error.empty())throw;
            }
        },
        [current,owner,applied]() {
            try {return current() && !applied->empty() && Read(owner.address,12)==*applied;}
            catch(...) {return false;}
        }
    };
    LiveOverlays::Start(std::move(request));
    const auto original=originals.find(address);
    nativeDualOverlays.insert_or_assign(address,NativeOverlayOverride{
        owner.palette,c.Current_Pallete_Num,original==originals.end()?owner.header:original->second.header,installed});
    try {replace(markerHeader);}
    catch(...) {LiveOverlays::Cancel();throw;}
}
}
using namespace OverlayControls;
void ForgetMatch() {
#ifdef SGPE_DLL_VALIDATION
    CancelOverlayDrawPreview();
#endif
    LiveOverlays::Clear();
    for(auto& [address,entry]:nativeDualOverlays)
        retiredDualOverlays.insert_or_assign(address,std::move(entry));
    nativeDualOverlays.clear();
    overlayDrafts.clear();
    OverlayEditor::ForgetMatch();identity.clear();attempted.clear();nextCheck=0;
}
void ResetAutoLoad() {attempted.clear();nextCheck=0;}
void PaletteChanged() {
#ifdef SGPE_DLL_VALIDATION
    CancelOverlayDrawPreview();
#endif
    try { OverlayEditor::Refresh();OverlayEditor::selectionKey.clear(); }
    catch(const std::exception& e) {Status=e.what();}
}
bool Load(const std::string& path) {
    try {
        auto c=Current();auto d=PaletteData::ReadFile(PaletteData::PathFromText(path));
        if(d.secondOverlay) {
            ApplyDualOverlay(c,d,[key=DraftKey(c)](const std::string& error) {
                if(error.empty())overlayDrafts.erase(key);
                Status=error.empty()?"Colors and both overlays applied.":"Load failed: "+error;
            });
            Status="Preparing both overlays.";
            return true;
        }
        OverlayEditor::ApplyPalette(c,d);Sync();
        overlayDrafts.erase(DraftKey(c));
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
        if(identity!=current) {
            ForgetMatch();
            RestoreRetiredOverlays(current);
            identity=current;
        }
        int selected=PlayableCharactersManager::GetCurrentCharacterIndex();
        if(selected>=0 && Model(selected).Current_Pallete_Num!=PlayableCharactersManager::GetCurrentCharacter().Current_Palette_Num) {
            PaletteChanged();
            Sync();
        }
        auto now=GetTickCount64();if(now<nextCheck)return;nextCheck=now+250;
#ifdef SGPE_DLL_VALIDATION
        if(!LiveOverlays::Busy() && TickOverlayProbe())return;
        if(std::filesystem::exists("dll-texture-resources.request")) {
            std::filesystem::remove("dll-texture-resources.request");
            nlohmann::json results=nlohmann::json::object();
            OverlayEditor::Catalog();
            for(auto& entry:OverlayEditor::templates) {
                try {OverlayEditor::ResolveOverlayTexture(entry.first); results[entry.first]="Loaded";}
                catch(const std::exception& e) {results[entry.first]=e.what();}
            }
            try {OverlayEditor::ResolveOverlayTexture("MissingTexture");results["missingResource"]="Unexpected success";}
            catch(const std::exception& e) {results["missingResource"]=e.what();}
            PaletteData::AtomicWriteText("dll-texture-resources-result.json",results.dump(2));
        }
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
                    auto document=PaletteData::ReadFile(path);
                    if(LiveOverlays::Busy())continue;
                    if(document.secondOverlay) {
                        attempted[key]=fingerprint;
                        const auto character=entry.CharName;
                        const auto palette=entry.PalNum;
                        const auto savedPath=entry.PalPath;
                        ApplyDualOverlay(c,std::move(document),[character,palette,savedPath,draftKey=DraftKey(c)](const std::string& failure) {
                            if(failure.empty())overlayDrafts.erase(draftKey);
                            for(auto& target:AutoPalette::Auto_Pals)
                                if(target.CharName==character && target.PalNum==palette && target.PalPath==savedPath)
                                    target.Status=failure.empty()?"Loaded colors and both overlays.":"Auto-load failed: "+failure;
                        });
                        entry.Status="Preparing both overlays.";
                        continue;
                    }
                    attempted[key]=fingerprint;
                    OverlayEditor::ApplyPalette(c,document);
                    overlayDrafts.erase(DraftKey(c));
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
        OverlayControls::Draw(c,Status,LiveOverlays::Busy(),true,[&](PaletteData::Document document) {
            if(document.secondOverlay) {
                ApplyDualOverlay(c,std::move(document),[](const std::string& error) {
                    Status=error.empty()?"Both overlays applied.":"Apply failed: "+error;
                });
                Status="Preparing both overlays.";
            } else {
                OverlayEditor::ApplyPalette(c,document);
                Status="Overlay applied.";
            }
        });
        // The shared controls may change alpha; keep the DLL color editor in sync.
        auto& dst=PlayableCharactersManager::GetCurrentCharacter();
        if(!c.Character_Colors.empty())dst.Character_Colors.assign(c.Character_Colors.begin(),c.Character_Colors.end());
    } catch(const std::exception& e) {ImGui::TextWrapped("%s",e.what());}
}
#include "GroupShaderControls.inl"
}
