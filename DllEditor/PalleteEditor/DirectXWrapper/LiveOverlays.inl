#include "OverlayBindingMatch.h"
namespace LiveOverlays {
using Microsoft::WRL::ComPtr;
using Texture = ComPtr<IDirect3DBaseTexture9>;
struct Binding {
    Texture feature, overlay;
    bool ambiguous = false;
};
using Bindings = std::map<IDirect3DBaseTexture9*, Binding>;
struct RenderEffect {
    Binding binding;
    Texture primary,secondary;
    PaletteTextureMatch::Rows colors;
    ComPtr<IDirect3DPixelShader9> shader;
    ComPtr<IDirect3DTexture9> bands;
    std::function<bool()> active;
    std::function<PaletteTextureMatch::Rows()> currentColors;
    bool visible = false;
};
struct Pending {
    Request request;
    ULONGLONG deadline;
    unsigned frames = 0;
    Bindings marker, observed, primary;
    std::set<OverlayBindingMatch::Id> candidates;
    int confirmation=0;
#ifdef SGPE_DLL_VALIDATION
    unsigned paletteChecks=0,paletteMatches=0;
    nlohmann::json colorMismatch;
#endif
    RenderEffect effect;
};
inline std::optional<Pending> pending;
inline std::map<std::pair<int,int>, RenderEffect> effects;

bool Busy() { return pending.has_value(); }
void Start(Request request) {
    if(Busy())throw std::runtime_error("Another two-texture palette is being prepared.");
    if(RendererTrace::OverlayDrawValidation::deadline || RendererTrace::OverlayDrawValidation::captureSlot)
        throw std::runtime_error("Wait for the timed preview to finish.");
    pending.emplace(Pending{std::move(request),GetTickCount64()+6000});
}
void Finish(const std::string& error) {
    auto complete=std::move(*pending);
    pending.reset();
#ifdef SGPE_DLL_VALIDATION
    try {PaletteData::AtomicWriteText("overlay-identification-result.json",nlohmann::json{
        {"slot",complete.request.slot},{"palette",complete.request.palette+1},{"error",error},
        {"paletteChecks",complete.paletteChecks},{"paletteMatches",complete.paletteMatches},
        {"markerBindings",complete.marker.size()},{"primaryBindings",complete.primary.size()},
        {"confirmationPhase",complete.confirmation},{"colorMismatch",complete.colorMismatch}}.dump(2));}catch(...) {}
#endif
    complete.request.finish(error);
    if(error.empty()) {
        complete.effect.active=std::move(complete.request.active);
        complete.effect.currentColors=std::move(complete.request.currentColors);
        complete.effect.colors=std::move(complete.request.finalColors);
        complete.effect.visible=true;
        effects.insert_or_assign(std::pair{complete.request.slot,complete.request.palette},std::move(complete.effect));
    }
}
void Cancel() {
    if(pending)Finish("Two-texture preparation was cancelled.");
}
void Clear() {
    Cancel();
    effects.clear();
}

inline void Tick(IDirect3DDevice9* device) {
    for(auto& [slot,effect]:effects) {
        try {
            effect.visible=effect.active();
            if(effect.visible)effect.colors=effect.currentColors();
        }catch(...) {effect.visible=false;}
    }
    if(!pending)return;
    try {
        if(!pending->request.current())throw std::runtime_error("Palette changed during two-texture preparation.");
        if(GetTickCount64()>=pending->deadline)throw std::runtime_error("Could not identify a unique draw for this fighter.");
        if(++pending->frames<8) {pending->observed.clear();return;}
        auto& task=*pending;
        if(task.marker.empty()) {
            if(task.observed.empty())return;
            task.effect.primary=RendererTrace::OverlayDrawValidation::LoadOverlayTexture(device,task.request.primary.texture);
            task.effect.secondary=RendererTrace::OverlayDrawValidation::LoadOverlayTexture(device,task.request.secondary.texture);
            RendererTrace::OverlayDrawValidation::BuildResources(device,task.request.secondary.bands,
                task.request.primary.tintBand,task.request.secondary.tintBand,task.effect.shader,task.effect.bands);
            task.marker.swap(task.observed);
            task.request.stagePrimary();
            task.frames=0;
            return;
        }
        auto snapshot=[](const Bindings& bindings) {
            OverlayBindingMatch::Snapshot result;
            for(const auto& [feature,binding]:bindings)
                result[reinterpret_cast<OverlayBindingMatch::Id>(feature)]=binding.ambiguous?0:reinterpret_cast<OverlayBindingMatch::Id>(binding.overlay.Get());
            return result;
        };
        if(task.confirmation==0) {
            task.candidates=OverlayBindingMatch::Changed(snapshot(task.marker),snapshot(task.observed));
            if(task.candidates.empty()) {
                // Some native mirror draws reuse one Chowder resource even when
                // their palette textures differ. A unique full-color signature
                // identifies this target without requiring a Chowder pointer change.
                const auto matching=snapshot(task.observed);
                if(matching.size()==1 && matching.begin()->second && task.marker.count(task.observed.begin()->first)) {
                    task.effect.binding=task.observed.begin()->second;Finish("");return;
                }
                task.observed.clear();task.frames=0;return;
            }
            task.primary.swap(task.observed);
            task.request.stageMarker();
            task.confirmation=1;task.frames=0;return;
        }
        if(task.confirmation==1) {
            // Global cache refresh can change unrelated fighters once at match start.
            // Require the same feature to follow both directions of our own marker.
            OverlayBindingMatch::Confirm(task.candidates,snapshot(task.marker),snapshot(task.observed));
            if(task.candidates.empty())throw std::runtime_error("No fighter draw followed the marker back to its original binding.");
            task.request.stagePrimary();task.observed.clear();
            task.confirmation=2;task.frames=0;return;
        }
        OverlayBindingMatch::Confirm(task.candidates,snapshot(task.primary),snapshot(task.observed));
        if(task.candidates.size()!=1)throw std::runtime_error(task.candidates.empty()?
            "The target draw did not repeat the requested texture binding.":
            "Multiple fighter draws followed the same two-texture marker. Use different palette numbers for mirror fighters.");
        const auto selected=reinterpret_cast<IDirect3DBaseTexture9*>(*task.candidates.begin());
        task.effect.binding=task.observed.at(selected);
        Finish("");
    }catch(const std::exception& e) {
        if(pending)Finish(e.what());
        else throw;
    }
}

inline bool Matches(IDirect3DBaseTexture9* base,const PaletteTextureMatch::Rows& colors) {
    if(!base || base->GetType()!=D3DRTYPE_TEXTURE || colors.empty())return false;
    auto* texture=static_cast<IDirect3DTexture9*>(base);D3DSURFACE_DESC desc{};
    if(FAILED(texture->GetLevelDesc(0,&desc)) || desc.Width!=16 || desc.Height!=256 || desc.Format!=D3DFMT_A8R8G8B8 || desc.Pool!=D3DPOOL_MANAGED)return false;
    D3DLOCKED_RECT lock{};if(FAILED(texture->LockRect(0,&lock,nullptr,D3DLOCK_READONLY)))return false;
    bool match=lock.pBits && lock.Pitch>=64;
    for(size_t row=0;match && row<colors.size();++row) {
        const auto* pixels=reinterpret_cast<const DWORD*>(static_cast<const unsigned char*>(lock.pBits)+row*lock.Pitch);
        for(int x=0;x<16;++x)if(!PaletteTextureMatch::Close(pixels[x],colors[row][x])){
#ifdef SGPE_DLL_VALIDATION
            if(pending)pending->colorMismatch={{"band",row},{"shade",x},{"actual",pixels[x]},{"expected",colors[row][x]}};
#endif
            match=false;break;
        }
    }
    texture->UnlockRect(0);return match;
}

inline void Observe(const Texture& feature,const Texture& overlay,IDirect3DBaseTexture9* palette) {
    if(!pending)return;
    const bool match=Matches(palette,pending->request.originalColors);
#ifdef SGPE_DLL_VALIDATION
    ++pending->paletteChecks;if(match)++pending->paletteMatches;
#endif
    if(!match)return;
    auto& observed=pending->observed;
    auto [it,inserted]=observed.try_emplace(feature.Get(),Binding{feature,overlay});
    if(!inserted && it->second.overlay.Get()!=overlay.Get())it->second.ambiguous=true;
}

inline const RenderEffect* Find(IDirect3DBaseTexture9* feature,IDirect3DBaseTexture9* overlay,IDirect3DBaseTexture9* palette) {
    const RenderEffect* result=nullptr;
    for(const auto& [slot,effect]:effects) {
        if(!effect.visible || effect.binding.feature.Get()!=feature || effect.binding.overlay.Get()!=overlay || !Matches(palette,effect.colors))continue;
        if(result)return nullptr;
        result=&effect;
    }
    return result;
}
}
