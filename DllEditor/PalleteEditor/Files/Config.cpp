#include "Config.h"
#include <json.hpp>
#include <fstream>
#include <filesystem>
#include "GroupJSONFiles.h"
#include "pch.h"
#include "AutoLoadPalette.h"
#include "../../../PalleteEditor/Data/AtomicFile.h"

namespace fs = std::filesystem;
using json = nlohmann::json;
using ordered_json = nlohmann::ordered_json;

void config::init() {
    if (loaded) return;

    config_path=fs::current_path()/"PaletteEditorDLLConfig.json";
    if (fs::exists(config_path)) {
        std::ifstream file(config_path);
        try {file >> data;if(!data.is_object())data=json::object();}catch(...){data=json::object();}
        std::string CharPartPath = get_string("CharPart");
        std::string Table = get_string("Table");

        if (CharPartPath != "") {
            GroupColorManager::GetInstance().LoadFromFile(CharPartPath);

        }
        AutoPalette::load();
    }
    else {
        data = {
            {"CharPart", ""},
            {"Table", ""},
            {"AutoLoadPals", json::array()}
        };
        save();
    }
    if(!GroupColorManager::GetInstance().HasData())GroupColorManager::GetInstance().LoadFromFile("groups.json");
    loaded = true;
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
    return data.value(key, default_val);
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
    try {PaletteData::AtomicWriteText(config_path,data.dump(2));}
    catch(const std::exception& e) {LOG_ERROR("Settings save failed: ",e.what());}
}
