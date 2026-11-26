#include "Data/PaletteDocument.h"
#include "Data/AtomicFile.h"
#include "Data/NativeRedirectPlan.h"
#include "Data/BeowulfHype.h"
#include <limits>
#include <iostream>
#include <functional>

namespace {
int checks=0;
void Require(bool condition,const char* message) { ++checks; if(!condition) throw std::runtime_error(message); }
void Reject(const std::function<void()>& action,const char* message) {
    bool rejected=false; try { action(); } catch(const std::exception&) { rejected=true; } Require(rejected,message);
}
PaletteData::Document Example() {
    PaletteData::Document d; d.character="Annie"; d.groupSizes={1,2,3};
    d.colors={0,0xffffffff,0x80ff0080,0x010000ff,0xff123456,0x7fabcdef};
    d.lineColor=0xff000001; d.shadow1=0x11223344; d.shadow2=0xffabcdef;
    d.overlay=PaletteData::Overlay{"Fire",{1},1,2};
    d.rainbow=PaletteData::Rainbow{{1,2},4,8};
    d.colorLinks=PaletteData::ColorLinks{{{1,2}},0xa5};
    d.transformations={PaletteData::Transformation{{{2,1}},0x81,0,"SUPER",""}};
    return d;
}
}
int main() {
    try {
        using namespace PaletteData;
        auto d=Example(); auto json=Encode(d); auto extended=EncodePal(d);
        auto dual=d;
        dual.secondOverlay=Overlay{"Water",{2},0,1};
        const auto dualBytes=EncodePal(dual);
        Require(Encode(DecodePal(dualBytes))==Encode(dual),"Two overlays did not round-trip.");
        Require(DecodeLegacyPrefix(dualBytes).colors==d.colors,"Two overlays changed the legacy prefix.");
        auto overlap=dual;
        overlap.secondOverlay->bands={1};
        Reject([&]{EncodePal(overlap);},"Overlapping overlay groups accepted.");
        auto missing=dual;
        missing.overlay.reset();
        Reject([&]{EncodePal(missing);},"Second overlay without a primary accepted.");
        Require(Encode(DecodePal(extended))==json,"Extended .pal did not round-trip all colors and effects.");
        auto oldReader=DecodeLegacyPrefix(extended);
        Require(oldReader.colors==d.colors && oldReader.lineColor==d.lineColor && oldReader.shadow1==d.shadow1 && oldReader.shadow2==d.shadow2,
            "Legacy reader could not read colors from extended .pal.");
        Require(oldReader.legacyHueSpeed==4 && oldReader.legacyHueInterval==8,"Legacy hue bytes were discarded.");
        auto plain=EncodePal(d,true); auto legacy=DecodePal(plain);
        Require(legacy.legacy && legacy.colors==d.colors && !legacy.overlay && legacy.transformations.empty(),"Plain legacy palette was not supported.");
        Require(plain.size()==22+4*(d.colors.size()-1)+12,"Legacy binary layout changed.");
        auto corrupt=extended; corrupt.pop_back(); Reject([&]{DecodePal(corrupt);},"Truncated extension accepted.");
        corrupt=extended; corrupt[22]^=1; Reject([&]{DecodePal(corrupt);},"Disagreeing color prefix accepted.");
        corrupt=extended; corrupt[plain.size()+6]='2'; Reject([&]{DecodePal(corrupt);},"Unknown extension version accepted.");
        corrupt=extended; for(size_t i=plain.size()+8;i<plain.size()+12;i++) corrupt[i]=255;
        Reject([&]{DecodePal(corrupt);},"Oversized extension accepted.");
        corrupt=plain; corrupt.resize(20); Reject([&]{DecodePal(corrupt);},"Truncated legacy file accepted.");
        corrupt=plain; corrupt[16]=0; corrupt[17]=corrupt[18]=corrupt[19]=0;
        Reject([&]{DecodePal(corrupt);},"Zero color-count legacy file accepted.");
        corrupt=plain; corrupt.push_back(1); Reject([&]{DecodePal(corrupt);},"Unknown legacy suffix silently ignored.");
        auto bad=json; bad["version"]=6; Reject([&]{Decode(bad);},"Unknown schema version accepted.");
        bad=json; bad["colors"][1]=-1; Reject([&]{Decode(bad);},"Negative packed color accepted.");
        bad=json; bad["colors"][1]=4294967296ULL; Reject([&]{Decode(bad);},"Overflowing packed color accepted.");
        bad=json; bad["effects"]["rainbow"]["speed"]=1.5; Reject([&]{Decode(bad);},"Fractional timing accepted.");
        bad=json; bad["effects"]["rainbow"]["bands"]={1,1}; Reject([&]{Decode(bad);},"Duplicate effect groups accepted.");
        bad=json; bad["effects"]["overlay"]["bands"]={3}; Reject([&]{Decode(bad);},"Absent group accepted.");
        bad=json; bad["groupSizes"]={1,2,4}; Reject([&]{Decode(bad);},"Incorrect group layout accepted.");
        bad=json; bad["effects"]["colorLinks"]["links"]={{1,2},{1,2}}; Reject([&]{Decode(bad);},"Duplicate link sources accepted.");
        bad=json; bad["effects"]["transformations"][0]["activationTag"]="AIR"; Reject([&]{Decode(bad);},"Unsupported tag accepted.");
        bad=json; bad["effects"]["transformations"][0]["activationTag"]=""; Reject([&]{Decode(bad);},"Automatic transformation without a trigger accepted.");
        bad=json; bad["effects"]["transformations"][0]["legacyTrigger"]="annie-supers"; Reject([&]{Decode(bad);},"Ambiguous trigger accepted.");
        auto native=d; native.overlay.reset(); native.rainbow.reset(); native.colorLinks.reset(); native.transformations.clear();
        Require(Encode(DecodePal(EncodePal(native)))==Encode(native),"Native effect inheritance did not round-trip.");
        // V1/V2 migrate to exactly one stable rule, retaining forced live modes.
        for(int version:{1,2}) for(int mode:{0,1,2}) {
            auto old=version==1?json:Encode(dual);old["version"]=version;
            auto rule=old["effects"]["transformations"][0];rule["previewMode"]=mode;
            rule.erase("id");rule.erase("name");rule.erase("enabled");
            old["effects"].erase("transformations");old["effects"]["transformation"]=rule;
            auto migrated=Decode(old);
            Require(migrated.transformations.size()==1 && migrated.transformations[0].id=="legacy" && migrated.transformations[0].enabled && migrated.transformations[0].previewMode==mode,"Old transformation mode changed during migration.");
            Require(Encode(DecodePal(EncodePal(migrated)))==Encode(migrated),"Migration did not round-trip as V3.");
        }
        auto multiple=d;multiple.groupSizes={1,2,3,2,2};multiple.colors.resize(10,0xff123456);
        Transformation a{{{1,2}},255,0,"ATTACK","","a","Attack hair",true};
        Transformation b{{{3,4}},255,0,"JUMPING","","b","Jump clothes",true};
        multiple.transformations={a,b};
        const auto both=Encode(multiple);
        Require(both["version"]==3 && Encode(DecodePal(EncodePal(multiple)))==both,"Multiple independent rules did not round-trip.");
        auto modified=multiple;modified.transformations[0].enabled=false;
        Require(Encode(modified)["effects"]["transformations"][1]==both["effects"]["transformations"][1],"Disabling A changed B.");
        modified.transformations[0].activationTag="SUPER";modified.transformations[0].links={{2,1}};
        Require(Encode(modified)["effects"]["transformations"][1]==both["effects"]["transformations"][1],"Editing A changed B.");
        modified.transformations.erase(modified.transformations.begin());
        Require(Encode(modified)["effects"]["transformations"][0]==both["effects"]["transformations"][1],"Removing A changed B.");
        auto invalidRules=both;invalidRules["effects"]["transformations"][1]["id"]="a";
        Reject([&]{Decode(invalidRules);},"Duplicate stable rule ID accepted.");
        invalidRules=both;invalidRules["effects"]["transformations"][1]["enabled"]=1;
        Reject([&]{Decode(invalidRules);},"Numeric enabled accepted.");
        invalidRules=both;invalidRules["effects"]["transformations"][1]["name"]="";
        Reject([&]{Decode(invalidRules);},"Empty name accepted.");
        invalidRules=both;invalidRules["effects"]["transformations"][1]["activationTag"]="HYPE";
        Reject([&]{Decode(invalidRules);},"Unverified resource trigger accepted.");
        invalidRules=both;invalidRules["version"]=2;
        Reject([&]{Decode(invalidRules);},"Lossy collection downgrade accepted.");
        invalidRules=both;invalidRules["effects"]["transformation"]=nullptr;
        Reject([&]{Decode(invalidRules);},"Ambiguous V3 single-rule field accepted.");
        auto conflicting=multiple;conflicting.transformations[1].links={{1,4}};
        Reject([&]{Encode(conflicting);},"Overlapping rule shades accepted.");
        conflicting.transformations[1].enabled=false;
        Reject([&]{Encode(conflicting);},"Disabled rule lost its shade reservation.");
        conflicting.transformations[0].shadeMask=15;conflicting.transformations[1].shadeMask=240;
        Require(Decode(Encode(conflicting)).transformations.size()==2,"Disjoint shades on one source rejected.");
        auto plan=BuildRedirectPlan({},{{1,4}},255,multiple.transformations);
        Require(plan.size()==3 && plan[0].conditionRule==-1 && plan[1].conditionRule==0 && plan[2].conditionRule==1,"Native composition lost conditions or static baseline.");
        // Emulate the observed native forward overwrites for neither/A/B/both.
        for(int active=0;active<4;++active) {
            int destinations[5]={0,1,2,3,4};
            for(const auto& record:plan)if(record.conditionRule<0 || (active&(1<<record.conditionRule)))destinations[RedirectWord(record.bytes.data())]=RedirectWord(record.bytes.data()+4);
            Require(destinations[1]==((active&1)?2:4) && destinations[3]==((active&2)?4:3),"Independent activation truth table failed.");
        }
        auto disabled=multiple.transformations;disabled[0].enabled=false;
        auto disabledPlan=BuildRedirectPlan({}, {},255,disabled);
        Require(disabledPlan.size()==1 && disabledPlan[0].conditionRule==1,"Disabling A removed B's native record.");
        std::vector<unsigned char> nativeRecord(44);RedirectPut(nativeRecord.data(),1);RedirectPut(nativeRecord.data()+4,3);nativeRecord[8]=255;nativeRecord[9]=7;nativeRecord[16]=1;
        auto masked=BuildRedirectPlan(nativeRecord,{{1,2}},15,{});
        Require(masked.size()==2 && masked[0].bytes[8]==240 && masked[0].bytes[9]==7 && masked[0].bytes[16]==1 && masked[1].bytes[8]==15,"Static mask destroyed unrelated native shades or fields.");
        Require(BuildRedirectPlan(nativeRecord,{},255,{})[0].bytes[8]==255,"No rules changed native data.");
        Reject([&]{BuildRedirectPlan({1,2,3},{},255,{});},"Partial native record accepted.");
        Require(DecodePal(EncodePal(multiple,true)).transformations.empty(),"Explicit color-only export retained rules.");
        auto hype=multiple;hype.character="Beowulf";hype.transformations.clear();
        for(int level=0;level<4;++level) {
            Transformation rule{{{1,2}},255,0,"","","hype-"+std::to_string(level),"Hype "+std::to_string(level),true,level};
            hype.transformations.push_back(rule);
        }
        auto invalidInternal=hype;invalidInternal.transformations[0].hypeLevel=-2;
        Reject([&]{Encode(invalidInternal);},"Invalid internal predicate was silently discarded.");
        const auto hypeJson=Encode(hype);
        Require(hypeJson["version"]==4 && Encode(DecodePal(EncodePal(hype)))==hypeJson,"Four Hype predicates did not round-trip.");
        auto badHype=hypeJson;badHype["version"]=3;
        Reject([&]{Decode(badHype);},"Resource predicates silently downgraded to V3.");
        badHype=hypeJson;badHype["character"]="Annie";
        Reject([&]{Decode(badHype);},"Hype resource applied to another character.");
        badHype=hypeJson;badHype["effects"]["transformations"][0]["predicate"]["provider"]="unknown";
        Reject([&]{Decode(badHype);},"Unknown resource provider accepted.");
        badHype=hypeJson;badHype["effects"]["transformations"][0]["predicate"]["offset"]=0x190;
        Reject([&]{Decode(badHype);},"Arbitrary resource offset accepted.");
        badHype=hypeJson;badHype["effects"]["transformations"][1]["predicate"]["level"]=0;
        Reject([&]{Decode(badHype);},"Duplicate overlapping Hype levels accepted.");
        badHype=hypeJson;badHype["effects"]["transformations"][0]["previewMode"]=2;
        Reject([&]{Decode(badHype);},"Forced resource mode destroyed exclusivity.");
        badHype=hypeJson;badHype["effects"]["transformations"][0]["activationTag"]="ATTACK";
        Reject([&]{Decode(badHype);},"Mixed tag/resource predicate accepted.");
        badHype=hypeJson;badHype["effects"]["transformations"][0]["predicate"]["level"]=4;
        Reject([&]{Decode(badHype);},"Out-of-range Hype level accepted.");
        for(float amount:{0.f,0.999f,1.f,1.25f,1.999f,2.f,2.34f,2.999f,3.f}) {
            const int expected=static_cast<int>(amount);Require(BeowulfHypeLevel(amount)==expected,"Completed Hype level boundary failed.");
            int matches=0;
            for(int level=0;level<4;++level) {
                const auto comparisons=HypeComparisons(level);
                const bool active=amount>=comparisons[0].rhs && (comparisons[1].opcode==0x10?amount<comparisons[1].rhs:amount<=comparisons[1].rhs);
                if(active){++matches;Require(level==expected,"Native Hype plan selected the wrong level.");}
            }
            Require(matches==1,"Hype levels are not mutually exclusive and exhaustive.");
        }
        for(float invalid:{-1.f,3.001f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()})Require(BeowulfHypeLevel(invalid)==-1,"Invalid Hype sample reported an inactive/success state.");
        auto moves=hype;moves.transformations.clear();
        for(const auto& entry:CharacterMoves())if(entry.id=="beowulf/chair-toss" || entry.id=="beowulf/organic-arm") {
            Transformation rule{{{1,2}},255,0,"","","move-"+std::to_string(moves.transformations.size()),entry.label,true,-1,entry.id};
            moves.transformations.push_back(rule);
        }
        const auto movesJson=Encode(moves);
        Require(movesJson["version"]==5 && Encode(DecodePal(EncodePal(moves)))==movesJson,"Whole-move predicates did not round-trip.");
        auto badMove=movesJson;badMove["version"]=4;
        Reject([&]{Decode(badMove);},"Move predicate silently downgraded to V4.");
        badMove=movesJson;badMove["character"]="Annie";
        Reject([&]{Decode(badMove);},"Character move applied to another fighter.");
        badMove=movesJson;badMove["effects"]["transformations"][0]["predicate"]["move"]="not-a-move";
        Reject([&]{Decode(badMove);},"Unknown move accepted.");
        badMove=movesJson;badMove["effects"]["transformations"][0]["predicate"]["states"]={191,192};
        Reject([&]{Decode(badMove);},"Arbitrary move states accepted.");
        badMove=movesJson;badMove["effects"]["transformations"][1]["predicate"]=badMove["effects"]["transformations"][0]["predicate"];
        Reject([&]{Decode(badMove);},"Duplicate overlapping moves accepted.");
        badMove=movesJson;badMove["effects"]["transformations"][0]["activationTag"]="SPECIAL";
        Reject([&]{Decode(badMove);},"Mixed tag/move accepted.");
        badMove=movesJson;badMove["effects"]["transformations"][0]["previewMode"]=2;
        Reject([&]{Decode(badMove);},"Forced move mode broke exclusivity.");
        auto mixedMove=moves;mixedMove.transformations[0].hypeLevel=1;
        Reject([&]{Encode(mixedMove);},"Internal mixed Hype/move accepted.");
        auto movePlan=BuildRedirectPlan({}, {},255,moves.transformations);
        Require(movePlan.size()==2 && movePlan[0].conditionRule==0 && movePlan[1].conditionRule==1,"Move composition lost independent conditions.");
        auto moveAndHype=moves;moveAndHype.transformations.push_back(hype.transformations[0]);
        Reject([&]{Encode(moveAndHype);},"Move/Hype overlap was treated as exclusive.");
        Require(DecodePal(EncodePal(moves,true)).transformations.empty(),"Explicit colors-only move export retained predicates.");
        std::set<std::string> roster,moveIds;
        for(const auto& entry:CharacterMoves()) {
            roster.insert(entry.character);Require(moveIds.insert(entry.id).second,"Duplicate roster move ID.");
            Require(entry.animationCount>0 && entry.foitSha256.size()==64 && !entry.ranges.empty(),"Missing roster build bounds.");
            int previous=-1;for(const auto& range:entry.ranges){Require(range.first>=0 && range.second>=range.first && range.second<entry.animationCount && range.first>previous,"Roster move range exceeds character table or overlaps.");previous=range.second;}
            auto single=moves;single.character=entry.character;single.transformations.resize(1);single.transformations[0].moveId=entry.id;
            Require(DecodePal(EncodePal(single)).transformations[0].moveId==entry.id,"Roster move did not round-trip through .pal.");
        }
        Require(roster.size()==18,"Move catalogue does not cover all 18 characters.");
        auto folder=std::filesystem::current_path()/"document-test-files";
        std::filesystem::create_directories(folder);
        auto path=folder/L"palette-unicode-\u03A9.pal";
        AtomicWrite(path,extended); Require(Encode(ReadFile(path))==json,"Unicode-path palette read/write failed.");
        AtomicWrite(path,plain); Require(ReadFile(path).legacy,"Atomic replacement failed.");
        Reject([&]{AtomicWrite(folder,extended);},"Replacing a directory should fail.");
        Require(ReadFile(path).legacy,"Failed save damaged a previously saved palette.");
        std::cout<<"Passed "<<checks<<" palette format, legacy compatibility, validation and atomic I/O checks.\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<"\n"; return 1; }
}
