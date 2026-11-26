#include "pch.h"
#include "AdvancedPalette.h"
#include "PlayableCharactersManager.h"
#include "ProcessManager.h"
#include "AutoLoadPalette.h"
#include "Files/Config.h"
#include "UI/SpriteViewer.h"
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
#include "MoveContextHost.inl"

#include "DirectXWrapper/LiveOverlays.h"
#ifdef SGPE_DLL_VALIDATION
void CancelOverlayDrawPreview();
#endif

namespace AdvancedPalette {
std::string Status;
namespace {
std::vector<DWORD> identity;
std::map<std::string,std::string> attempted;
std::map<std::string,std::string> attemptResults;
std::map<std::string,std::string> preparedColors;
std::map<std::string,ULONGLONG> retryAfter;
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
    const auto baseline=OverlayEditor::CapturePalette(c);
    const auto baselineRows=PaletteTextureMatch::Build(baseline.colors,baseline.groupSizes);
    bool uniqueColors=true;
    for(int slot=0;slot<6;++slot)if(slot!=c.ID) {
        try {
            const auto other=OverlayEditor::CapturePalette(Model(slot));
            if(PaletteTextureMatch::Build(other.colors,other.groupSizes)==baselineRows)uniqueColors=false;
        }catch(...) {}
    }
    LiveOverlays::Request request{c.ID,c.Current_Pallete_Num,primary,secondary,
        baselineRows,PaletteTextureMatch::Build(document.colors,document.groupSizes),uniqueColors,
        [current,owner,installed]() {return current() && Read(owner.address,12)==*installed;},
        [replace,targetHeader,installed]() {
            *installed=targetHeader;
            replace(targetHeader);
        },
        [replace,markerHeader,installed]() {
            *installed=markerHeader;
            replace(markerHeader);
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
        },
        [c]() {
            auto live=OverlayEditor::CapturePalette(c);
            return PaletteTextureMatch::Build(live.colors,live.groupSizes);
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
    ++MatchGeneration;
    SpriteViewer::ForgetMatch();
#ifdef SGPE_DLL_VALIDATION
    CancelOverlayDrawPreview();
#endif
    LiveOverlays::Clear();
    for(auto& [address,entry]:nativeDualOverlays)
        retiredDualOverlays.insert_or_assign(address,std::move(entry));
    nativeDualOverlays.clear();
    overlayDrafts.clear();
    OverlayEditor::ForgetMatch();identity.clear();attempted.clear();attemptResults.clear();preparedColors.clear();retryAfter.clear();nextCheck=0;
}
void ResetAutoLoad() {attempted.clear();attemptResults.clear();preparedColors.clear();retryAfter.clear();nextCheck=0;}
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
    MoveContextHost::Register();
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
                auto key=std::to_string(current[2+2*slot])+":"+std::to_string(entry.PalNum);
                try {
                    if(entry.PalPath.empty())throw std::runtime_error("Choose a .pal file.");
                    auto path=std::filesystem::absolute(PaletteData::PathFromText(entry.PalPath));
                    std::error_code error;auto time=std::filesystem::last_write_time(path,error);
                    auto fingerprint=PaletteData::PathText(path)+(error?":missing":":"+std::to_string(time.time_since_epoch().count()));
                    if(attempted.count(key) && attempted[key]==fingerprint){if(attemptResults.count(key))entry.Status=attemptResults[key];continue;}
                    auto document=PaletteData::ReadFile(path);
                    if(document.secondOverlay) {
                        if(!preparedColors.count(key) || preparedColors[key]!=fingerprint) {
                            // Validate the complete assignment before any early color write.
                            // A legacy-shaped colors-only import preserves all live effects.
                            OverlayEditor::ValidatePalette(c,document,true);
                            PaletteData::Document colors;
                            colors.character=document.character;colors.colors=document.colors;colors.groupSizes=document.groupSizes;
                            colors.lineColor=document.lineColor;colors.shadow1=document.shadow1;colors.shadow2=document.shadow2;colors.legacy=true;
                            OverlayEditor::ApplyPalette(c,colors,false,true);
                            preparedColors[key]=fingerprint;
                            if(slot==selected)Sync();
                        }
                        if(LiveOverlays::Busy()) {entry.Status="Base colors loaded; waiting for overlay preparation.";continue;}
                        if(retryAfter.count(key) && GetTickCount64()<retryAfter[key]) {entry.Status="Base colors loaded; waiting for fighter to be visible (tag in).";continue;}
                        const auto live=OverlayEditor::CapturePalette(c);
                        if(!LiveOverlays::VisiblePalette(PaletteTextureMatch::Build(live.colors,live.groupSizes))) {
                            entry.Status="Base colors loaded; waiting for fighter to be visible (tag in).";continue;
                        }
                        // Do not overwrite manual colors edited while shaders waited.
                        document.colors=live.colors;document.lineColor=live.lineColor;
                        document.shadow1=live.shadow1;document.shadow2=live.shadow2;
                        attempted[key]=fingerprint;
                        const auto character=entry.CharName;
                        const auto palette=entry.PalNum;
                        const auto savedPath=entry.PalPath;
                        ApplyDualOverlay(c,std::move(document),[character,palette,savedPath,key,draftKey=DraftKey(c)](const std::string& failure) {
                            if(failure.empty())overlayDrafts.erase(draftKey);
                            if(failure==LiveOverlays::WaitingForDraw){attempted.erase(key);retryAfter[key]=GetTickCount64()+1000;}
                            attemptResults[key]=failure.empty()?"Loaded colors and both overlays.":
                                failure==LiveOverlays::WaitingForDraw?"Base colors loaded; waiting for fighter to be visible (tag in).":"Auto-load failed: "+failure;
                            for(auto& target:AutoPalette::Auto_Pals)
                                if(target.CharName==character && target.PalNum==palette && target.PalPath==savedPath)
                                    target.Status=attemptResults[key];
                        });
                        entry.Status="Preparing both overlays.";
                        continue;
                    }
                    if(LiveOverlays::Busy())continue;
                    attempted[key]=fingerprint;
                    OverlayEditor::ApplyPalette(c,document);
                    overlayDrafts.erase(DraftKey(c));
                    if(slot==selected)Sync();entry.Status="Loaded colors and shader settings.";attemptResults[key]=entry.Status;
                } catch(const std::exception& e) {entry.Status=std::string("Auto-load failed: ")+e.what();attemptResults[key]=entry.Status;}
            }
            if(!found)entry.Status="Waiting for this character and palette number.";
        }
    } catch(const std::exception& e) {Status=e.what();}
}
std::vector<int> PreviewLayout() { return OverlayEditor::CapturePalette(Current()).groupSizes; }
std::set<int> PreviewBands(const ColorGroup& group) { return GroupBands(OverlayEditor::AppearanceData(Current()),group); }
bool Busy() { return LiveOverlays::Busy(); }
namespace {
PaletteData::Document PendingDocument() {
    auto c=Current(); auto document=OverlayEditor::CapturePalette(c); auto& draft=Draft(c);
    if(draft.primary.bands.empty())document.overlay.reset(); else document.overlay=draft.primary;
    if(draft.hasSecond && !draft.secondary.bands.empty())document.secondOverlay=draft.secondary; else document.secondOverlay.reset();
    if(!document.overlay && document.secondOverlay){document.overlay=document.secondOverlay;document.secondOverlay.reset();}
    return document;
}
}
bool HasPending() {
    try {
        auto c=Current();if(!overlayDrafts.count(DraftKey(c)))return false;
        auto live=OverlayEditor::CapturePalette(c);auto staged=PendingDocument();
        auto same=[](const auto& a,const auto& b) {
            return bool(a)==bool(b) && (!a || (a->texture==b->texture && a->bands==b->bands && a->lighting==b->lighting && a->tintBand==b->tintBand));
        };
        return !same(live.overlay,staged.overlay) || !same(live.secondOverlay,staged.secondOverlay);
    }catch(...) { return false; }
}
void DiscardPending() { try {overlayDrafts.erase(DraftKey(Current()));Status="Pending assignments discarded.";}catch(const std::exception& e){Status=e.what();} }
void ApplyPending() {
    try {
        auto c=Current();auto document=PendingDocument();
        if(document.secondOverlay) {
            ApplyDualOverlay(c,std::move(document),[key=DraftKey(c)](const std::string& error) {
                if(error.empty())overlayDrafts.erase(key);
                Status=error.empty()?"Applied":"Apply failed: "+error;
            });Status="Pending: preparing both overlays";
        } else {
            if(Busy())throw std::runtime_error("Wait for the overlay operation.");
            OverlayEditor::ApplyPalette(c,document);overlayDrafts.erase(DraftKey(c));Sync();Status="Applied";
        }
    }catch(const std::exception& e){Status=std::string("Apply failed: ")+e.what();}
}
std::vector<PaletteData::Transformation> PreviewRules() {return OverlayEditor::CapturePalette(Current()).transformations;}
std::string GroupEffects(const ColorGroup& group) {
    try {
        auto c=Current();
        static int frame=-1;static std::pair<int,int> target;
        static OverlayEditor::AppearanceContext data;static PaletteData::Document d;
        if(frame!=ImGui::GetFrameCount() || target!=DraftKey(c)) {
            data=OverlayEditor::AppearanceData(c);d=OverlayEditor::CapturePalette(c);frame=ImGui::GetFrameCount();target=DraftKey(c);
        }
        auto bands=GroupBands(data,group);
        std::string result;
        auto add=[&](const auto& effect,const std::string& name) {if(effect && std::any_of(bands.begin(),bands.end(),[&](int band){return effect->bands.count(band);})) {if(!result.empty())result+=", ";result+=name;}};
        add(d.overlay,d.overlay?d.overlay->texture:"");add(d.secondOverlay,d.secondOverlay?d.secondOverlay->texture:"");add(d.rainbow,"Rainbow");
        if(d.colorLinks)for(int b:bands)if(d.colorLinks->links.count(b)){result+=" Borrow colors";break;}
        int ruleCount=0,enabledCount=0;
        for(const auto& rule:d.transformations)for(int b:bands)if(rule.links.count(b)){++ruleCount;if(rule.enabled)++enabledCount;break;}
        if(ruleCount) {
            if(!result.empty())result+=", ";
            result+=std::to_string(ruleCount)+(ruleCount==1?" rule":" rules");
            if(enabledCount!=ruleCount)result+=" ("+std::to_string(enabledCount)+" enabled)";
        }
        return result.empty()?"No applied effects":result;
    }catch(...) {return "Native band controls in Advanced";}
}
void Draw() {
    if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());
    try {
        auto c=Current();ImGui::BeginDisabled(LiveOverlays::Busy());
        OverlayEditor::Draw(OverlayEditor::process,OverlayEditor::base,c);ImGui::EndDisabled();
        if(RevealNativeBands){ImGui::SetNextItemOpen(true);ImGui::SetScrollHereY(0.0f);RevealNativeBands=false;}
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
