#pragma once
#include <string>
#include <vector>
#include <set>
#include "Files/GroupJSONFiles.h"
namespace AdvancedPalette {
    extern std::string Status;
    inline bool OpenAdvanced=false;
    std::vector<int> PreviewLayout();
    std::set<int> PreviewBands(const ColorGroup& group);
    bool Busy();
    bool HasPending();
    void DiscardPending();
    void ApplyPending();
    std::string GroupEffects(const ColorGroup& group);
    void Tick();
    void Draw();
    void DrawGroup(const ColorGroup& group);
    void PaletteChanged();
    void ForgetMatch();
    void ResetAutoLoad();
    bool Load(const std::string& path);
    bool Save(const std::string& path,bool legacy=false);
}
