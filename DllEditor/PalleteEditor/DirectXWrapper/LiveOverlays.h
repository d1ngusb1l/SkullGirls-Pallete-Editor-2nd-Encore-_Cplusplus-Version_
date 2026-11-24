#pragma once
#include <functional>
#include <optional>
#include "PaletteTextureMatch.h"
#include "../../../PalleteEditor/Data/PaletteDocument.h"

struct IDirect3DBaseTexture9;

namespace LiveOverlays {
struct Request {
    int slot;
    int palette;
    PaletteData::Overlay primary, secondary;
    PaletteTextureMatch::Rows originalColors,finalColors;
    bool uniqueOriginalColors=true;
    std::function<bool()> current;
    std::function<void()> stagePrimary;
    std::function<void()> stageMarker;
    std::function<void(const std::string&)> finish;
    std::function<bool()> active;
    std::function<PaletteTextureMatch::Rows()> currentColors;
};
inline constexpr char WaitingForDraw[]="Waiting for fighter to be visible (tag in).";
bool WatchingVisibility();
bool VisiblePalette(const PaletteTextureMatch::Rows& colors);
void RecordPalette(IDirect3DBaseTexture9* texture);
bool Busy();
void Start(Request request);
void Clear();
void Cancel();
}
