#pragma once
#include <functional>
#include <optional>
#include "PaletteTextureMatch.h"
#include "../../../PalleteEditor/Data/PaletteDocument.h"

namespace LiveOverlays {
struct Request {
    int slot;
    int palette;
    PaletteData::Overlay primary, secondary;
    PaletteTextureMatch::Rows originalColors,finalColors;
    std::function<bool()> current;
    std::function<void()> stagePrimary;
    std::function<void()> stageMarker;
    std::function<void(const std::string&)> finish;
    std::function<bool()> active;
    std::function<PaletteTextureMatch::Rows()> currentColors;
};
bool Busy();
void Start(Request request);
void Clear();
void Cancel();
}
