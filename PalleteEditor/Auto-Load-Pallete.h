#pragma once
#include <vector>
#include <string>

struct Auto_Pal {
    std::string CharName;
    int PalNum=0;
    std::string PalPath;
    bool Enabled=true;
    std::string Status;
};
class AutoPallete {
public:
    static void init();
    static void Tick(bool force=false);
    static void ResetSession();
    static void Assign(const std::string& character,int palette,const std::string& path);
    static void Draw();
    static void save();
    static void load();
    static inline std::vector<Auto_Pal> Auto_Pals;
};
