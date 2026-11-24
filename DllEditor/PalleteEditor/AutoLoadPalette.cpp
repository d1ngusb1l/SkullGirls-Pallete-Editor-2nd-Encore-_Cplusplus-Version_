#include "AutoLoadPalette.h"
#include "Files/LegacyConfigMigration.h"
#include "AdvancedPalette.h"
#include "pch.h"
#include "Files/Config.h"
#include <json.hpp>
#include "PlayableCharactersManager.h"
#include "Files/PaletteFiles.h"


using json = nlohmann::json;

auto AutoLoadLogger = LOGGER::createLocal("Auto Load Palettes", LogLevel::GENERAL_LOG);

void AutoPalette::init() { AdvancedPalette::ResetAutoLoad(); }

void AutoPalette::load() {
    Auto_Pals.clear();

    nlohmann::json autoPalsJson = config::get_json("AutoLoadPals");

    if (autoPalsJson.is_array()) {
        for (const auto& item : autoPalsJson) {
            if(!LegacyConfigMigration::ValidAssignment(item))continue;
            Auto_Pal pal;
            pal.CharName=item.at("CharName").get<std::string>();
            pal.PalNum=item.at("PalNumber").get<int>();
            pal.PalPath=item.at("PathToPal").get<std::string>();
            pal.Enabled=item.value("Enabled",true);
            Auto_Pals.push_back(pal);
        }
    }
}

void AutoPalette::save() {
    nlohmann::json autoPalsJson = nlohmann::json::array();


    for (const auto& pal : Auto_Pals) {
        nlohmann::json item;
        item["CharName"] = pal.CharName;
        item["PalNumber"] = pal.PalNum;
        item["PathToPal"] = pal.PalPath;
        item["Enabled"] = pal.Enabled;

        autoPalsJson.push_back(item);
    }

    config::set_json("AutoLoadPals", autoPalsJson);
    AdvancedPalette::ResetAutoLoad();
}
