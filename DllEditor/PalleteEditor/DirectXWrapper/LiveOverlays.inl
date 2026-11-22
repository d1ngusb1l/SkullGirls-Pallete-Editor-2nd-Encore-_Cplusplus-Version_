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
    Texture secondary;
    ComPtr<IDirect3DPixelShader9> shader;
    ComPtr<IDirect3DTexture9> bands;
    std::function<bool()> active;
    bool visible = false;
};
struct Pending {
    Request request;
    ULONGLONG deadline;
    unsigned frames = 0;
    Bindings marker, observed;
    RenderEffect effect;
};
inline std::optional<Pending> pending;
inline std::map<std::pair<int,int>, RenderEffect> effects;

bool Busy() { return pending.has_value(); }
void Start(Request request) {
    if(Busy())throw std::runtime_error("Another two-texture palette is being prepared.");
    if(RendererTrace::OverlayDrawValidation::deadline || RendererTrace::OverlayDrawValidation::captureSlot)
        throw std::runtime_error("Wait for the timed preview to finish.");
    pending.emplace(Pending{std::move(request),GetTickCount64()+3000});
}
void Finish(const std::string& error) {
    auto complete=std::move(*pending);
    pending.reset();
    complete.request.finish(error);
    if(error.empty()) {
        complete.effect.active=std::move(complete.request.active);
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
    for(auto& [slot,effect]:effects)effect.visible=effect.active();
    if(!pending)return;
    try {
        if(!pending->request.current())throw std::runtime_error("Palette changed during two-texture preparation.");
        if(GetTickCount64()>=pending->deadline)throw std::runtime_error("Could not identify a unique draw for this fighter.");
        if(++pending->frames<8) {pending->observed.clear();return;}
        auto& task=*pending;
        if(task.marker.empty()) {
            if(task.observed.empty())return;
            task.effect.secondary=RendererTrace::OverlayDrawValidation::LoadOverlayTexture(device,task.request.secondary.texture);
            RendererTrace::OverlayDrawValidation::BuildResources(device,task.request.secondary.bands,
                task.request.primary.tintBand,task.request.secondary.tintBand,task.effect.shader,task.effect.bands);
            task.marker.swap(task.observed);
            task.request.stagePrimary();
            task.frames=0;
            return;
        }
        const Binding* changed=nullptr;
        for(const auto& [key,after]:task.observed) {
            auto before=task.marker.find(key);
            if(before==task.marker.end() || before->second.ambiguous || after.ambiguous ||
                !after.overlay || before->second.overlay.Get()==after.overlay.Get())continue;
            if(changed)throw std::runtime_error("Multiple fighter draws changed during two-texture preparation.");
            changed=&after;
        }
        if(!changed) {task.observed.clear();task.frames=0;return;}
        task.effect.binding=*changed;
        Finish("");
    }catch(const std::exception& e) {
        if(pending)Finish(e.what());
        else throw;
    }
}

inline void Observe(const Texture& feature,const Texture& overlay) {
    if(!pending)return;
    auto& observed=pending->observed;
    auto [it,inserted]=observed.try_emplace(feature.Get(),Binding{feature,overlay});
    if(!inserted && it->second.overlay.Get()!=overlay.Get())it->second.ambiguous=true;
}

inline const RenderEffect* Find(IDirect3DBaseTexture9* feature,IDirect3DBaseTexture9* overlay) {
    const RenderEffect* result=nullptr;
    for(const auto& [slot,effect]:effects) {
        if(!effect.visible || effect.binding.feature.Get()!=feature || effect.binding.overlay.Get()!=overlay)continue;
        if(result)return nullptr;
        result=&effect;
    }
    return result;
}
}
