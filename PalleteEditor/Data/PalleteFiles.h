#pragma once
#include "Character.h"

class PalleteFile {
public:
    inline static std::string Status;
    inline static std::filesystem::path LastSavedPath;
    static bool LoadFromFile(Character& character);
    static bool SaveToFile(const Character& character,bool colorsOnly=false);
    static bool LoadPath(Character& character,const std::filesystem::path& path);
    static bool SavePath(const Character& character,const std::filesystem::path& path,bool colorsOnly=false);
};
