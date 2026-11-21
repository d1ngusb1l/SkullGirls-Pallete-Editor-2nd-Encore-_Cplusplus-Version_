#include "pch.h"
#include "Config.h"
#include "Data/AtomicFile.h"
#include "Data/GroupJSONFiles.h"
#include "Auto-Load-Pallete.h"

void config::init() {
    if(loaded) return;
    config_path=fs::current_path()/"PaletteEditorConfig.json";
    data={{"CharPart",""},{"Table",""},{"AutoLoadPals",json::array()}};
    auto source=config_path;
    if(!fs::exists(source)) {
        wchar_t profile[32768]{};
        DWORD size=GetEnvironmentVariableW(L"USERPROFILE",profile,32768);
        if(size && size<32768) source=fs::path(profile)/"Documents"/"Skullgirls"/"PalEditConfig.json";
    }
    bool valid=true;
    if(fs::exists(source)) {
        try {
            if(fs::file_size(source)>1024*1024) throw std::runtime_error("Config file is too large.");
            std::ifstream input(source); json parsed; input>>parsed;
            if(!parsed.is_object()) throw std::runtime_error("Expected a settings object.");
            data=std::move(parsed);
        } catch(const std::exception&) { Status="Settings file could not be read; it has been left untouched. Defaults are in use."; valid=false; }
    }
    loaded=true;
    auto groupPath=get_string("CharPart");
    if(!groupPath.empty()) {
        try {
            std::ifstream input(PaletteData::PathFromText(groupPath)); nlohmann::ordered_json groups; input>>groups;
            std::unordered_map<std::string,std::vector<ColorGroup>> parsed;
            for(auto& item:groups.items()) {
                int start=1;
                for(auto& group:item.value().items()) {
                    int count=group.value(); if(count<1 || count>255 || start+count>4096) throw std::runtime_error("Invalid group labels.");
                    parsed[item.key()].push_back({group.key(),start,count}); start+=count;
                }
            }
            GroupColorGroup::characterGroups=std::move(parsed);
        } catch(...) { Status="Custom group labels could not be loaded; bundled labels will be used."; }
    }
    AutoPallete::load();
    if(valid && source!=config_path) save();
}
std::string config::get_string(const std::string& key,const std::string& fallback) {
    try { return data.value(key,fallback); } catch(...) { return fallback; }
}
void config::set_string(const std::string& key,const std::string& value) { data[key]=value; save(); }
bool config::get_bool(const std::string& key,bool fallback) {
    try { return data.value(key,fallback); } catch(...) { return fallback; }
}
void config::set_bool(const std::string& key,bool value) { data[key]=value; save(); }
json config::get_json(const std::string& key,const json& fallback) {
    try { return data.contains(key)?data.at(key):fallback; } catch(...) { return fallback; }
}
void config::set_json(const std::string& key,const json& value) { data[key]=value; save(); }
void config::save() {
    try { PaletteData::AtomicWriteText(config_path,data.dump(2)); Status.clear(); }
    catch(const std::exception& error) { Status=std::string("Settings were not saved: ")+error.what(); }
}
