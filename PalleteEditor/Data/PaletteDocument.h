#pragma once
#include <json.hpp>
#include <algorithm>
#include <cstdint>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include "CharacterMoves.h"

// Portable, versioned values only. No native pointers, handles or cache bytes.
namespace PaletteData {
using Json=nlohmann::json;
using Links=std::map<int,int>;
struct Overlay { std::string texture; std::set<int> bands; int lighting=0,tintBand=0; };
struct Rainbow { std::set<int> bands; int speed=4,interval=8; };
struct ColorLinks { Links links; int shadeMask=255; };
struct Transformation { Links links; int shadeMask=255,previewMode=0; std::string activationTag,legacyTrigger; std::string id="legacy",name="Imported transformation"; bool enabled=true; int hypeLevel=-1; std::string moveId; };
struct Document {
    std::string character;
    std::vector<uint32_t> colors;
    uint32_t lineColor=0,shadow1=0,shadow2=0;
    std::vector<int> groupSizes;
    std::optional<Overlay> overlay;
    std::optional<Overlay> secondOverlay;
    std::optional<Rainbow> rainbow;
    std::optional<ColorLinks> colorLinks;
    std::vector<Transformation> transformations;
    bool legacy=false;
    int legacyHueSpeed=0,legacyHueInterval=0;
};
inline int Integer(const Json& j,int low,int high,const char* name) {
    if(!j.is_number_integer() || (j.is_number_unsigned() && j.get<uint64_t>()>(uint64_t)high))
        throw std::runtime_error(std::string("Invalid ")+name+".");
    auto value=j.get<int64_t>();
    if(value<low || value>high) throw std::runtime_error(std::string(name)+" is out of range.");
    return (int)value;
}
inline uint32_t Color(const Json& j) {
    if(!j.is_number_integer() || (!j.is_number_unsigned() && j.get<int64_t>()<0) || j.get<uint64_t>()>UINT32_MAX)
        throw std::runtime_error("Colors must be unsigned 32-bit integers.");
    return j.get<uint32_t>();
}
inline std::set<int> Bands(const Json& j) {
    if(!j.is_array() || j.empty() || j.size()>255) throw std::runtime_error("Select valid effect groups.");
    std::set<int> result;
    for(auto& value:j) if(!result.insert(Integer(value,1,255,"group")).second) throw std::runtime_error("Duplicate group in palette file.");
    return result;
}
inline Links ReadLinks(const Json& j) {
    if(!j.is_array() || j.empty() || j.size()>255) throw std::runtime_error("Invalid color-link list.");
    Links result;
    for(auto& pair:j) {
        if(!pair.is_array() || pair.size()!=2) throw std::runtime_error("Color links must contain source/destination pairs.");
        int source=Integer(pair[0],1,255,"source group"),destination=Integer(pair[1],1,255,"destination group");
        if(source==destination || !result.emplace(source,destination).second) throw std::runtime_error("Duplicate source group or self-link.");
    }
    return result;
}
inline bool SupportedTag(const std::string& tag) {
    // The V1 file schema explicitly pins the currently supported native tags.
    for(auto value:{"SUPER","ATTACK","NORMAL","SPECIAL","HITSTUN","BLOCKSTUN","BLOCKING","PUSHBLOCK","KNOCKDOWN",
        "TAUNT","THROW","STANDING","CROUCHING","IDLE","DASH","AIRDASH","JUMP","JUMPING","DOUBLEJUMP","SUPERJUMP",
        "ASSIST","BURST","OTG","WALLBOUNCE","DEAD"}) if(tag==value) return true;
    return false;
}
// Disabled rules reserve their shades too: enabling cannot silently mask another rule.
inline void ValidateRuleConflicts(const std::vector<Transformation>& rules) {
    if(rules.size()>32)throw std::runtime_error("Expected at most 32 transformation rules.");
    std::set<std::string> ids;
    for(size_t index=0;index<rules.size();++index) {
        const auto& rule=rules[index];
        if(rule.id.empty() || rule.id.size()>64 || !std::all_of(rule.id.begin(),rule.id.end(),[](unsigned char c){return (c>='a'&&c<='z') || (c>='A'&&c<='Z') || (c>='0'&&c<='9') || c=='-' || c=='_';}) || !ids.insert(rule.id).second)throw std::runtime_error("Rule IDs must be unique ASCII identifiers (64 bytes maximum).");
        if(rule.name.empty() || rule.name.size()>96 || std::any_of(rule.name.begin(),rule.name.end(),[](unsigned char c){return c<32 || c==127;}))throw std::runtime_error("Rule names require 1 to 96 bytes of printable text.");
        if(rule.links.empty() || rule.shadeMask<1 || rule.shadeMask>255)throw std::runtime_error("Each rule needs links and at least one shader shade.");
        if(rule.hypeLevel < -1 || rule.hypeLevel>3)throw std::runtime_error("Unsupported Beowulf Hype level.");
        if(rule.hypeLevel>=0 && (rule.previewMode!=0 || !rule.activationTag.empty() || !rule.legacyTrigger.empty() || !rule.moveId.empty()))throw std::runtime_error("Hype predicates cannot combine with other triggers or live overrides.");
        if(!rule.moveId.empty() && (!FindCharacterMove(rule.moveId) || rule.previewMode!=0 || rule.hypeLevel!=-1 || !rule.activationTag.empty() || !rule.legacyTrigger.empty()))throw std::runtime_error("Unknown or mixed character move predicate.");
        for(size_t other=0;other<index;++other) {
            const auto& previous=rules[other];
            const bool exclusive=(rule.hypeLevel>=0 && previous.hypeLevel>=0 && rule.hypeLevel!=previous.hypeLevel) || ExclusiveCharacterMoves(rule.moveId,previous.moveId);
            if(exclusive || !(rule.shadeMask&previous.shadeMask))continue;
            for(const auto& link:rule.links)if(previous.links.count(link.first))throw std::runtime_error("Rules overlap on native band "+std::to_string(link.first)+". Use disjoint shades or mutually exclusive triggers.");
        }
    }
}
inline Document Decode(const Json& j) {
    if(!j.is_object() || j.at("format")!="skullgirls-palette")
        throw std::runtime_error("Unsupported palette file format or version.");
    const int version=Integer(j.at("version"),1,5,"palette version");
    Document d; d.character=j.at("character").get<std::string>();
    if(d.character.empty() || d.character.size()>32 || !std::all_of(d.character.begin(),d.character.end(),[](unsigned char c){return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9');}))
        throw std::runtime_error("Invalid character identifier.");
    auto& colors=j.at("colors");
    if(!colors.is_array() || colors.empty() || colors.size()>4096) throw std::runtime_error("Invalid palette color count.");
    for(auto& value:colors) d.colors.push_back(Color(value));
    auto& sizes=j.at("groupSizes");
    if(!sizes.is_array() || sizes.empty() || sizes.size()>256) throw std::runtime_error("Invalid palette group layout.");
    size_t count=0;
    for(auto& value:sizes) { int size=Integer(value,0,255,"group size"); d.groupSizes.push_back(size); count+=size; }
    if(d.groupSizes[0]!=1 || count!=d.colors.size()) throw std::runtime_error("Group layout does not match color count.");
    d.lineColor=Color(j.at("lineColor")); auto& shadows=j.at("superShadowColors");
    if(!shadows.is_array() || shadows.size()!=2) throw std::runtime_error("Expected two super-shadow colors.");
    d.shadow1=Color(shadows[0]); d.shadow2=Color(shadows[1]);
    const auto& effects=j.at("effects"); if(!effects.is_object()) throw std::runtime_error("Invalid effect settings.");
    auto readOverlay=[](const Json& e) {
        Overlay value; value.texture=e.at("texture").get<std::string>();
        if(value.texture.empty() || value.texture.size()>15 || !std::all_of(value.texture.begin(),value.texture.end(),[](unsigned char c){return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_';}))
            throw std::runtime_error("Invalid overlay texture name.");
        value.bands=Bands(e.at("bands")); value.lighting=Integer(e.at("lighting"),0,2,"overlay lighting");
        value.tintBand=Integer(e.at("tintBand"),0,255,"overlay tint group"); return value;
    };
    if(!effects.at("overlay").is_null()) d.overlay=readOverlay(effects.at("overlay"));
    if(version==2 || (version>=3 && effects.contains("secondOverlay"))) {
        if(!d.overlay)throw std::runtime_error("Two-texture palettes require a primary overlay.");
        d.secondOverlay=readOverlay(effects.at("secondOverlay"));
        for(int target:d.secondOverlay->bands)
            if(d.overlay->bands.count(target))throw std::runtime_error("Overlay assignments cannot target the same group.");
    } else if(effects.contains("secondOverlay")) {
        throw std::runtime_error("Second overlay requires palette schema version 2.");
    }
    if(!effects.at("rainbow").is_null()) {
        auto& e=effects.at("rainbow"); Rainbow value; value.bands=Bands(e.at("bands"));
        value.speed=Integer(e.at("speed"),1,255,"rainbow speed"); value.interval=Integer(e.at("interval"),1,255,"rainbow interval"); d.rainbow=value;
    }
    if(!effects.at("colorLinks").is_null()) {
        auto& e=effects.at("colorLinks"); d.colorLinks=ColorLinks{ReadLinks(e.at("links")),Integer(e.at("shadeMask"),1,255,"color-link shade mask")};
    }
    auto readTransformation=[&](const Json& e) {
        Transformation value;
        value.links=ReadLinks(e.at("links")); value.shadeMask=Integer(e.at("shadeMask"),1,255,"transformation shade mask");
        value.previewMode=Integer(e.at("previewMode"),0,2,"transformation preview mode");
        value.activationTag=e.at("activationTag").get<std::string>(); value.legacyTrigger=e.at("legacyTrigger").get<std::string>();
        if((!value.activationTag.empty() && !SupportedTag(value.activationTag)) ||
           (value.legacyTrigger!="" && value.legacyTrigger!="annie-supers" && value.legacyTrigger!="filia-color30-flag20" && value.legacyTrigger!="preview-only"))
            throw std::runtime_error("Unsupported transformation trigger.");
        if(!value.activationTag.empty() && !value.legacyTrigger.empty()) throw std::runtime_error("Ambiguous transformation trigger.");
        if(e.contains("predicate")) {
            if(version<4)throw std::runtime_error("Resource predicates require palette schema version 4.");
            const auto& predicate=e.at("predicate");
            if(!predicate.is_object() || predicate.size()!=2)throw std::runtime_error("Unsupported predicate provider.");
            if(predicate.at("provider")=="character-move") {
                if(version<5)throw std::runtime_error("Character moves require palette schema version 5.");
                value.moveId=predicate.at("move").get<std::string>();
                const auto move=FindCharacterMove(value.moveId);
                if(!move || move->character!=d.character)throw std::runtime_error("Unsupported move for this character.");
            } else if(predicate.at("provider")=="beowulf-hype") {
                if(d.character!="Beowulf")throw std::runtime_error("Beowulf Hype predicates are unsupported for this character.");
                value.hypeLevel=Integer(predicate.at("level"),0,3,"Beowulf Hype level");
            } else throw std::runtime_error("Unsupported predicate provider.");
        }
        if(value.moveId.empty() && value.hypeLevel<0 && value.previewMode==0 && value.activationTag.empty() && (value.legacyTrigger.empty() || value.legacyTrigger=="preview-only"))
            throw std::runtime_error("Automatic transformation has no trigger.");
        return value;
    };
    if(version<3) {
        if(effects.contains("transformations")) throw std::runtime_error("Rule collections require schema version 3.");
        if(!effects.at("transformation").is_null()) d.transformations.push_back(readTransformation(effects.at("transformation")));
    } else {
        if(effects.contains("transformation")) throw std::runtime_error("Version 3 requires a rule collection, not a single transformation.");
        const auto& rules=effects.at("transformations");
        if(!rules.is_array() || rules.size()>32) throw std::runtime_error("Expected at most 32 transformation rules.");
        for(const auto& e:rules) {
            auto value=readTransformation(e);
            value.id=e.at("id").get<std::string>(); value.name=e.at("name").get<std::string>();
            if(!e.at("enabled").is_boolean()) throw std::runtime_error("Rule enabled must be boolean.");
            value.enabled=e.at("enabled").get<bool>(); d.transformations.push_back(value);
        }
    }
    auto band=[&](int value) { if(value<1 || value>=(int)d.groupSizes.size() || !d.groupSizes[value]) throw std::runtime_error("Effect refers to an absent group."); };
    if(d.overlay) { for(int value:d.overlay->bands) band(value); if(d.overlay->tintBand) band(d.overlay->tintBand); }
    if(d.secondOverlay) { for(int value:d.secondOverlay->bands) band(value); if(d.secondOverlay->tintBand) band(d.secondOverlay->tintBand); }
    if(d.rainbow) for(int value:d.rainbow->bands) band(value);
    if(d.colorLinks) for(auto& rule:d.colorLinks->links) { band(rule.first); band(rule.second); }
    for(const auto& transformation:d.transformations) for(auto& rule:transformation.links) { band(rule.first); band(rule.second); }
    ValidateRuleConflicts(d.transformations);
    return d;
}
inline Json Encode(const Document& d) {
    ValidateRuleConflicts(d.transformations);
    Json effects={{"overlay",nullptr},{"rainbow",nullptr},{"colorLinks",nullptr},{"transformation",nullptr}};
    if(d.overlay) effects["overlay"]={{"texture",d.overlay->texture},{"bands",d.overlay->bands},{"lighting",d.overlay->lighting},{"tintBand",d.overlay->tintBand}};
    if(d.secondOverlay) {
        const auto& second=*d.secondOverlay;
        effects["secondOverlay"]={{"texture",second.texture},{"bands",second.bands},{"lighting",second.lighting},{"tintBand",second.tintBand}};
    }
    if(d.rainbow) effects["rainbow"]={{"bands",d.rainbow->bands},{"speed",d.rainbow->speed},{"interval",d.rainbow->interval}};
    if(d.colorLinks) effects["colorLinks"]={{"links",d.colorLinks->links},{"shadeMask",d.colorLinks->shadeMask}};
    effects.erase("transformation"); effects["transformations"]=Json::array();
    bool resources=false,moves=false;
    for(const auto& rule:d.transformations) { effects["transformations"].push_back({{"id",rule.id},{"name",rule.name},{"enabled",rule.enabled},
        {"links",rule.links},{"shadeMask",rule.shadeMask},{"previewMode",rule.previewMode},{"activationTag",rule.activationTag},{"legacyTrigger",rule.legacyTrigger}});
        if(rule.hypeLevel>=0){resources=true;effects["transformations"].back()["predicate"]={{"provider","beowulf-hype"},{"level",rule.hypeLevel}};}
        if(!rule.moveId.empty()){moves=true;effects["transformations"].back()["predicate"]={{"provider","character-move"},{"move",rule.moveId}};}
    }
    Json j={{"format","skullgirls-palette"},{"version",moves?5:resources?4:3},{"character",d.character},{"groupSizes",d.groupSizes},
        {"colors",d.colors},{"lineColor",d.lineColor},{"superShadowColors",{d.shadow1,d.shadow2}},{"effects",effects}};
    Decode(j); return j;
}
inline uint32_t LittleEndian(const std::vector<unsigned char>& bytes,size_t offset) {
    if(offset+4>bytes.size()) throw std::runtime_error("Truncated legacy palette.");
    return (uint32_t)bytes[offset]|((uint32_t)bytes[offset+1]<<8)|((uint32_t)bytes[offset+2]<<16)|((uint32_t)bytes[offset+3]<<24);
}
inline Document DecodeLegacyPrefix(const std::vector<unsigned char>& bytes) {
    if(bytes.size()<34) throw std::runtime_error("Truncated legacy palette.");
    auto end=std::find(bytes.begin(),bytes.begin()+16,0);
    if(end==bytes.begin()+16 || end==bytes.begin()) throw std::runtime_error("Invalid legacy character identifier.");
    Document d; d.legacy=true; d.character=std::string(bytes.begin(),end);
    uint32_t count=LittleEndian(bytes,16);
    if(count<1 || count>4096 || bytes.size()<22+4*(size_t)(count-1)+12) throw std::runtime_error("Legacy palette size or color count is invalid.");
    d.legacyHueSpeed=bytes[20]; d.legacyHueInterval=bytes[21];
    if(d.legacyHueSpeed && !d.legacyHueInterval) throw std::runtime_error("Legacy rainbow has an invalid interval.");
    d.colors.push_back(0); size_t offset=22;
    for(uint32_t color=1;color<count;color++,offset+=4) d.colors.push_back(LittleEndian(bytes,offset));
    d.lineColor=LittleEndian(bytes,offset); d.shadow1=LittleEndian(bytes,offset+4); d.shadow2=LittleEndian(bytes,offset+8);
    return d;
}
inline void AppendWord(std::vector<unsigned char>& bytes,uint32_t value) {
    for(int shift=0;shift<32;shift+=8) bytes.push_back((unsigned char)(value>>shift));
}
inline std::vector<unsigned char> EncodePal(const Document& d,bool colorsOnly=false) {
    auto json=Encode(d); // validate before producing either file representation
    if(d.character.size()>15 || d.colors[0]!=0) throw std::runtime_error("Legacy .pal compatibility requires a short character identifier and transparent color zero.");
    std::vector<unsigned char> bytes(16);
    std::copy(d.character.begin(),d.character.end(),bytes.begin()); AppendWord(bytes,(uint32_t)d.colors.size());
    bytes.push_back(d.rainbow?(unsigned char)d.rainbow->speed:0);
    bytes.push_back(d.rainbow?(unsigned char)d.rainbow->interval:0);
    for(size_t color=1;color<d.colors.size();color++) AppendWord(bytes,d.colors[color]);
    AppendWord(bytes,d.lineColor); AppendWord(bytes,d.shadow1); AppendWord(bytes,d.shadow2);
    if(!colorsOnly) {
        const unsigned char marker[]={'S','G','P','E','F','X','1',0}; bytes.insert(bytes.end(),std::begin(marker),std::end(marker));
        auto payload=json.dump(); AppendWord(bytes,(uint32_t)payload.size()); bytes.insert(bytes.end(),payload.begin(),payload.end());
    }
    return bytes;
}
inline Document DecodePal(const std::vector<unsigned char>& bytes) {
    auto legacy=DecodeLegacyPrefix(bytes); size_t prefix=22+4*(legacy.colors.size()-1)+12;
    if(bytes.size()==prefix) return legacy;
    const unsigned char marker[]={'S','G','P','E','F','X','1',0};
    if(bytes.size()<prefix+12 || !std::equal(std::begin(marker),std::end(marker),bytes.begin()+prefix))
        throw std::runtime_error("Unsupported or damaged .pal extension.");
    uint32_t length=LittleEndian(bytes,prefix+8);
    if(length>4*1024*1024 || bytes.size()!=prefix+12+(size_t)length) throw std::runtime_error("Truncated or oversized .pal effect block.");
    auto full=Decode(Json::parse(bytes.begin()+prefix+12,bytes.end()));
    if(full.character!=legacy.character || full.colors!=legacy.colors || full.lineColor!=legacy.lineColor || full.shadow1!=legacy.shadow1 || full.shadow2!=legacy.shadow2 ||
       (full.rainbow?full.rainbow->speed:0)!=legacy.legacyHueSpeed || (full.rainbow?full.rainbow->interval:0)!=legacy.legacyHueInterval)
        throw std::runtime_error("The .pal color prefix and effect block disagree.");
    return full;
}
inline Document ReadFile(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary|std::ios::ate);
    if(!in) throw std::runtime_error("Could not open palette file.");
    auto size=in.tellg();
    if(size<=0 || size>4*1024*1024) throw std::runtime_error("Palette file is empty or too large.");
    std::vector<unsigned char> bytes((size_t)size); in.seekg(0); in.read((char*)bytes.data(),bytes.size());
    if(!in) throw std::runtime_error("Could not read complete palette file.");
    auto extension=path.extension().string(); std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return (char)std::tolower(c);});
    if(extension==".pal") return DecodePal(bytes);
    return Decode(Json::parse(bytes.begin(),bytes.end()));
}
}
