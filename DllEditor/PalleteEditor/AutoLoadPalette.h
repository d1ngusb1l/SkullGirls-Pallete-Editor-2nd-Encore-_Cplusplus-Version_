#pragma once
#include <vector>
#include <string>

struct Auto_Pal
{
	std::string CharName;
	int PalNum;
	std::string PalPath;
    bool Enabled=true;
    std::string Status;
};

class AutoPalette {
public:

	static void init();
	static void save();
	static void load();
	static inline std::vector<Auto_Pal> Auto_Pals;

};