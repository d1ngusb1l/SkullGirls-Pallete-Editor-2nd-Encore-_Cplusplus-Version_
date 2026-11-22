#pragma once
#include <functional>
#include <optional>
#include "../../../PalleteEditor/Data/PaletteDocument.h"

namespace LiveOverlays {
struct Request {
    int slot;
    int palette;
    PaletteData::Overlay primary, secondary;
    std::function<bool()> current;
    std::function<void()> stagePrimary;
    std::function<void(const std::string&)> finish;
    std::function<bool()> active;
};
bool Busy();
void Start(Request request);
void Clear();
void Cancel();
}
