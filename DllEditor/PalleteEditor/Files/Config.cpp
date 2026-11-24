#include "pch.h"
#include "Config.h"
#include "LegacyConfigMigration.h"
#include <ShlObj.h>
#pragma comment(lib,"shell32.lib")
#include <json.hpp>
#include <fstream>
#include <filesystem>
#include "GroupJSONFiles.h"
#include "AutoLoadPalette.h"
#include "../../../PalleteEditor/Data/AtomicFile.h"

namespace fs = std::filesystem;
using json = nlohmann::json;
using ordered_json = nlohmann::ordered_json;

namespace {
fs::path DllDirectory() {
    HMODULE module=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&DllDirectory),&module))return fs::current_path();
    std::wstring path(32768,L'\0');DWORD length=GetModuleFileNameW(module,path.data(),DWORD(path.size()));
    if(!length || length>=path.size())return fs::current_path();path.resize(length);return fs::path(path).parent_path();
}
std::string PathText(const fs::path& path) {auto utf8=path.u8string();return {utf8.begin(),utf8.end()};}
json ReadSettings(const fs::path& path) {
    if(fs::file_size(path)>1024*1024)throw std::runtime_error("Settings file exceeds 1 MiB.");
    std::ifstream input(path);json result;input>>result;
    if(!result.is_object())throw std::runtime_error("Settings must be a JSON object.");return result;
}
}
void config::init() {
    if(loaded)return;
    loaded=true;config_path=fs::current_path()/"PaletteEditorDLLConfig.json";
    data={{"CharPart",""},{"Table",""},{"AutoLoadPals",json::array()}};
    const bool existed=fs::exists(config_path);
    if(existed) {
        try {data=ReadSettings(config_path);}
        catch(...) {writable=false;Status="DLL settings could not be read. The file is untouched; defaults are in use.";}
    }
    bool changed=false;int imported=0;
    if(writable) {
        std::vector<fs::path> sources={fs::current_path()/"PaletteEditorConfig.json"};
        wchar_t documents[MAX_PATH]{};
        if(SUCCEEDED(SHGetFolderPathW(nullptr,CSIDL_PERSONAL,nullptr,SHGFP_TYPE_CURRENT,documents)))sources.push_back(fs::path(documents)/"Skullgirls"/"PalEditConfig.json");
        wchar_t profile[32768]{};DWORD size=GetEnvironmentVariableW(L"USERPROFILE",profile,32768);
        if(size && size<32768)sources.push_back(fs::path(profile)/"Documents"/"Skullgirls"/"PalEditConfig.json");
        std::set<fs::path> visited;
        for(const auto& source:sources) {
            if(!visited.insert(source.lexically_normal()).second || !fs::exists(source))continue;
            try {
                auto legacy=ReadSettings(source);
                // Original file-picker paths are normally absolute. Resolve existing
                // relative files beside the legacy JSON, retaining missing paths as-is.
                if(legacy.contains("AutoLoadPals") && legacy["AutoLoadPals"].is_array())for(auto& entry:legacy["AutoLoadPals"]) {
                    if(!LegacyConfigMigration::ValidAssignment(entry))continue;
                    auto text=entry["PathToPal"].get<std::string>();
                    fs::path path(std::u8string(text.begin(),text.end()));
                    if(path.is_relative() && fs::exists(source.parent_path()/path))entry["PathToPal"]=PathText(fs::absolute(source.parent_path()/path).lexically_normal());
                }
                auto result=LegacyConfigMigration::Merge(data,legacy,PathText(fs::absolute(source).lexically_normal()));
                changed=changed || result.changed;imported+=result.imported;
                if(result.invalid)Status="Some legacy Autoload rows are invalid and were skipped. Original settings were left untouched.";
            }catch(...) {Status="A legacy settings file could not be read and was left untouched.";}
        }
        if(changed && existed) {
            std::error_code ec;fs::copy_file(config_path,fs::path(config_path.wstring()+L".before-legacy-import.bak"),fs::copy_options::skip_existing,ec);
            if(ec){writable=false;Status="Legacy settings loaded in memory, but the DLL settings backup failed. Nothing was overwritten.";}
        }
        if(changed || !existed)save();
        if(imported && Status.empty())Status="Imported "+std::to_string(imported)+" legacy Autoload assignments. Original JSON was left untouched.";
    }
    auto custom=get_string("CharPart");
    if(!custom.empty())GroupColorManager::GetInstance().LoadFromFile(custom);
    if(!GroupColorManager::GetInstance().HasData())GroupColorManager::GetInstance().LoadFromFile(PathText(DllDirectory()/"groups.json"));
    if(!GroupColorManager::GetInstance().HasData())GroupColorManager::GetInstance().LoadFromFile("groups.json");
    AutoPalette::load();
}

// ��������� �������
std::string config::get_string(const std::string& key, const std::string& default_val) {
    try {
        return data.value(key, default_val);
    }
    catch (...) {
        return default_val;
    }
}

void config::set_string(const std::string& key, const std::string& value) {
    data[key] = value;
    save();
}

bool config::get_bool(const std::string& key, bool default_val) {
    try {return data.value(key, default_val);}catch(...){return default_val;}
}

void config::set_bool(const std::string& key, bool value) {
    data[key] = value;
    save();
}


json config::get_json(const std::string& key, const json& default_val) {
    try {
        if (data.contains(key)) {
            return data[key];
        }
        return default_val;
    }
    catch (...) {
        return default_val;
    }
}

void config::set_json(const std::string& key, const json& value) {
    data[key] = value;
    save();
}

void config::save() {
    if(!writable)return;
    try {PaletteData::AtomicWriteText(config_path,data.dump(2));}
    catch(const std::exception& e) {Status=std::string("Settings save failed: ")+e.what();LOG_ERROR("Settings save failed: ",e.what());}
}
