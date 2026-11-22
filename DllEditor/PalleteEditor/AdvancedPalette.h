#pragma once
#include <string>
#include "Files/GroupJSONFiles.h"
namespace AdvancedPalette {
    extern std::string Status;
    void Tick();
    void Draw();
    void DrawDualOverlayDialogs();
    void DrawGroup(const ColorGroup& group);
    void PaletteChanged();
    void ForgetMatch();
    void ResetAutoLoad();
    bool Load(const std::string& path);
    bool Save(const std::string& path,bool legacy=false);
}
