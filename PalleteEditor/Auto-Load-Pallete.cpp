#include "pch.h"
#include "Auto-Load-Pallete.h"
#include "PalleteEditor.h"
#include "OverlayEditor.h"
#include "Data/PalleteFiles.h"
#include "Data/AtomicFile.h"
#include "Config.h"
#include "tinyfiledialogs.h"

namespace {
std::map<std::string,std::string> attempted;
ULONGLONG nextCheck=0;
bool KnownCharacter(const std::string& name) {
    for(auto value:characterNames) if(name==value) return true; return false;
}
}
void AutoPallete::ResetSession() { attempted.clear(); nextCheck=0; }
void AutoPallete::init() { ResetSession(); Tick(true); }
void AutoPallete::Assign(const std::string& character,int palette,const std::string& path) {
    for(auto& entry:Auto_Pals) if(entry.CharName==character && entry.PalNum==palette) {
        entry.PalPath=path; entry.Enabled=true; save(); return;
    }
    Auto_Pals.push_back({character,palette,path,true,{}}); save();
}
void AutoPallete::Tick(bool force) {
    auto now=GetTickCount64(); if(!force && now<nextCheck) return; nextCheck=now+250;
    if(!PalEdit::bGameOpenned || !PalEdit::bMatchStarted) return;
    std::set<std::pair<std::string,int>> entries;
    for(auto& entry:Auto_Pals) {
        if(!entry.Enabled) { entry.Status="Disabled"; continue; }
        if(!entries.insert({entry.CharName,entry.PalNum}).second) { entry.Status="Duplicate assignment; only the first enabled entry is used."; continue; }
        if(entry.PalPath.empty()) { entry.Status="Choose a .pal file."; continue; }
        bool found=false;
        for(auto& character:PalEdit::Character_Vector) {
            if(character.Char_Name!=entry.CharName || character.Current_Pallete_Num!=entry.PalNum) continue;
            found=true;
            try {
                auto context=OverlayEditor::AppearanceData(character);
                auto path=std::filesystem::absolute(PaletteData::PathFromText(entry.PalPath));
                std::error_code error; auto modified=std::filesystem::last_write_time(path,error);
                std::string fingerprint=PaletteData::PathText(path)+":"+(error?"missing":std::to_string(modified.time_since_epoch().count()));
                std::string key=std::to_string(context.palette)+":"+std::to_string(entry.PalNum);
                auto previous=attempted.find(key);
                if(!force && previous!=attempted.end() && previous->second==fingerprint) continue;
                attempted[key]=fingerprint;
                PalleteFile::LoadPath(character,path);
                entry.Status=PalleteFile::Status;
            } catch(const std::exception& error) { entry.Status=std::string("Auto-load failed: ")+error.what(); }
        }
        if(!found) entry.Status="Waiting for this character and palette number.";
    }
}
void AutoPallete::load() {
    Auto_Pals.clear(); ResetSession(); auto list=config::get_json("AutoLoadPals");
    if(!list.is_array()) return;
    for(auto& item:list) {
        if(Auto_Pals.size()>=128) break;
        try {
            Auto_Pal entry; entry.CharName=item.at("CharName").get<std::string>(); entry.PalPath=item.at("PathToPal").get<std::string>();
            entry.PalNum=PaletteData::Integer(item.at("PalNumber"),0,255,"auto-load palette number");
            entry.Enabled=item.value("Enabled",true);
            if(!KnownCharacter(entry.CharName) || entry.PalPath.size()>32768) continue;
            Auto_Pals.push_back(std::move(entry));
        } catch(...) {}
    }
}
void AutoPallete::save() {
    nlohmann::json list=nlohmann::json::array();
    for(auto& entry:Auto_Pals) list.push_back({{"CharName",entry.CharName},{"PalNumber",entry.PalNum},{"PathToPal",entry.PalPath},{"Enabled",entry.Enabled}});
    config::set_json("AutoLoadPals",list); ResetSession();
}
void AutoPallete::Draw() {
    ImGui::TextWrapped("Assign a .pal file to a character and palette number. Colors and saved advanced settings load once when that palette is available, and again in each new match.");
    if(ImGui::Button("Add assignment")) { Auto_Pals.push_back({"Filia",0,"",true,{}}); save(); }
    ImGui::SameLine(); if(ImGui::Button("Retry assigned palettes")) init();
    if(!config::Status.empty()) ImGui::TextWrapped("%s",config::Status.c_str());
    int remove=-1;
    for(int index=0;index<(int)Auto_Pals.size();index++) {
        auto& entry=Auto_Pals[index]; ImGui::PushID(index); ImGui::Separator();
        if(ImGui::Checkbox("Enabled",&entry.Enabled)) save();
        ImGui::SetNextItemWidth(220);
        if(ImGui::BeginCombo("Character",entry.CharName.c_str())) {
            for(auto name:characterNames) if(ImGui::Selectable(name,entry.CharName==name)) { entry.CharName=name; save(); }
            ImGui::EndCombo();
        }
        int display=entry.PalNum+1; ImGui::SetNextItemWidth(150);
        if(ImGui::InputInt("Palette number",&display)) { entry.PalNum=(std::max)(0,(std::min)(255,display-1)); save(); }
        char path[32769]{}; strncpy_s(path,entry.PalPath.c_str(),_TRUNCATE);
        ImGui::SetNextItemWidth((std::max)(150.0f,ImGui::GetContentRegionAvail().x-80));
        if(ImGui::InputText("##path",path,sizeof(path))) entry.PalPath=path;
        if(ImGui::IsItemDeactivatedAfterEdit()) save();
        ImGui::SameLine();
        if(ImGui::Button("Browse")) {
            tinyfd_winUtf8=1; const char* filters[]={"*.pal"};
            const char* selected=tinyfd_openFileDialog("Choose auto-load palette",entry.PalPath.c_str(),1,filters,"Palette files (*.pal)",0);
            if(selected) { entry.PalPath=PaletteData::PathText(std::filesystem::absolute(PaletteData::PathFromText(selected))); save(); }
        }
        ImGui::TextWrapped("%s",entry.Status.c_str());
        if(ImGui::Button("Remove assignment")) remove=index;
        ImGui::PopID();
    }
    if(remove>=0) { Auto_Pals.erase(Auto_Pals.begin()+remove); save(); }
}
