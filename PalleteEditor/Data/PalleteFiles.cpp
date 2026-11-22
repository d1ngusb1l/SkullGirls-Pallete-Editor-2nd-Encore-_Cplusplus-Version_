#include "tinyfiledialogs.h"
#include "PalleteFiles.h"
#include "OverlayEditor.h"
#include "OverlayControls.h"
#include "AtomicFile.h"

bool PalleteFile::LoadPath(Character& character,const std::filesystem::path& path) {
    try {
        auto document=PaletteData::ReadFile(path);
        OverlayEditor::ApplyPalette(character,document,true);
        OverlayControls::overlayDrafts.erase(OverlayControls::DraftKey(character));
        Status=document.legacy?"Legacy palette loaded. Existing advanced assignments preserved.":"Palette colors and saved advanced assignments loaded.";
        if(document.secondOverlay)Status="Palette loaded. The EXE previews only the primary texture. Save Palette keeps both slots for the in-game DLL.";
        return true;
    } catch(const std::exception& error) { Status=std::string("Load failed: ")+error.what(); return false; }
}
bool PalleteFile::SavePath(const Character& character,const std::filesystem::path& path,bool colorsOnly) {
    try {
        auto document=OverlayEditor::CapturePalette(character);
        PaletteData::AtomicWrite(path,PaletteData::EncodePal(document,colorsOnly));
        LastSavedPath=std::filesystem::absolute(path);
        Status=colorsOnly?"Legacy colors-only palette exported.":"Palette saved, including applied advanced assignments.";
        return true;
    } catch(const std::exception& error) { Status=std::string("Save failed: ")+error.what(); return false; }
}
bool PalleteFile::LoadFromFile(Character& character) {
    tinyfd_winUtf8=1; const char* patterns[]={"*.pal"};
    const char* path=tinyfd_openFileDialog("Load palette","",1,patterns,"Palette files (*.pal)",0);
    return path && LoadPath(character,PaletteData::PathFromText(path));
}
bool PalleteFile::SaveToFile(const Character& character,bool colorsOnly) {
    tinyfd_winUtf8=1; const char* patterns[]={"*.pal"};
    std::string suggested=character.Char_Name+".pal";
    const char* selected=tinyfd_saveFileDialog(colorsOnly?"Export legacy colors only":"Save palette",suggested.c_str(),1,patterns,"Palette files (*.pal)");
    if(!selected) return false;
    auto path=PaletteData::PathFromText(selected); if(path.extension().empty()) path+=".pal";
    if(path.extension()!=L".pal" && path.extension()!=L".PAL") { Status="Use the .pal extension for palette files."; return false; }
    return SavePath(character,path,colorsOnly);
}
