#pragma once
#include "Files/GroupJSONFiles.h"
class ColorWheel {
public:
    static void Open(const ColorGroup& group);
    static void Draw();
    static void Close();
};
