#include "pch.h"
#include "PaletteFiles.h"
#include "AdvancedPalette.h"
bool PalleteFile::LoadFromFile(std::string path) { return AdvancedPalette::Load(path); }
bool PalleteFile::SaveToFile(std::string path) { return AdvancedPalette::Save(path); }
