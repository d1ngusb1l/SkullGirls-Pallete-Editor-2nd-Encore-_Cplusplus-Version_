#pragma once
#include <json.hpp>
#include <set>
#include <string>
namespace LegacyConfigMigration {
using json=nlohmann::json;
inline bool ValidAssignment(const json& item) {
    try {
        if(!item.is_object() || !item.at("CharName").is_string() || !item.at("PathToPal").is_string() || !item.at("PalNumber").is_number_integer())return false;
        auto name=item.at("CharName").get<std::string>(),path=item.at("PathToPal").get<std::string>();
        auto number=item.at("PalNumber").get<int64_t>();
        return !name.empty() && name.size()<=64 && !path.empty() && path.size()<=32768 && number>=0 && number<=255 && (!item.contains("Enabled") || item.at("Enabled").is_boolean());
    }catch(...){return false;}
}
struct Result {int imported=0,invalid=0;bool changed=false;};
inline Result Merge(json& current,const json& legacy,const std::string& source) {
    Result result;
    if(!current.is_object() || !legacy.is_object())return result;
    auto markers=current.value("ImportedLegacyConfigSources",json::array());
    if(!markers.is_array())markers=json::array();
    for(const auto& marker:markers)if(marker.is_string() && marker.get<std::string>()==source)return result;
    auto incoming=legacy.value("AutoLoadPals",json::array());
    if(!incoming.is_array()){result.invalid=1;return result;}
    auto entries=current.value("AutoLoadPals",json::array());
    // Never replace malformed current data with an empty/new list.
    if(!entries.is_array()){result.invalid=1;return result;}
    std::set<std::pair<std::string,int>> keys;
    for(const auto& entry:entries)if(ValidAssignment(entry))keys.insert({entry.at("CharName").get<std::string>(),entry.at("PalNumber").get<int>()});
    for(const auto& entry:incoming) {
        if(!ValidAssignment(entry)){++result.invalid;continue;}
        auto key=std::make_pair(entry.at("CharName").get<std::string>(),entry.at("PalNumber").get<int>());
        if(keys.insert(key).second){entries.push_back(entry);++result.imported;}
    }
    if(result.imported){current["AutoLoadPals"]=std::move(entries);result.changed=true;}
    for(const auto* key:{"CharPart","Table"}) {
        bool empty=!current.contains(key) || (current.at(key).is_string() && current.at(key).get<std::string>().empty());
        if(empty && legacy.contains(key) && legacy.at(key).is_string() && !legacy.at(key).get<std::string>().empty()) {current[key]=legacy.at(key);result.changed=true;}
    }
    // Do not reimport deliberately removed entries on the next launch.
    // A malformed legacy row keeps the source eligible after the user fixes it.
    if(!result.invalid){markers.push_back(source);current["ImportedLegacyConfigSources"]=std::move(markers);result.changed=true;}
    return result;
}
}
