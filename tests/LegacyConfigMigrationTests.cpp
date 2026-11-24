#include "../DllEditor/PalleteEditor/Files/LegacyConfigMigration.h"
#include <iostream>
#include <fstream>
#include <stdexcept>
using LegacyConfigMigration::json;
void Check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
json Entry(const char* name,int number,const char* path){return {{"CharName",name},{"PalNumber",number},{"PathToPal",path}};}
int main(int argc,char** argv){
 // Exact original DLL schema: absent Enabled, zero-based PalNumber, absolute path.
 json legacy={{"CharPart","C:/labels/groups.json"},{"Table","C:/tables/game.json"},{"AutoLoadPals",json::array({Entry("Cerebella",0,"C:/palettes/bella.pal"),Entry("Filia",1,"C:/palettes/filia.pal")})}};
 const auto untouched=legacy;
 json current={{"AutoLoadPals",json::array()},{"unrelatedPreference",42}};
 auto result=LegacyConfigMigration::Merge(current,legacy,"original-documents");
 Check(result.imported==2 && result.invalid==0,"Original entries not imported");
 Check(current["AutoLoadPals"][0]["PalNumber"]==0 && current["AutoLoadPals"][1]["PalNumber"]==1,"Legacy index changed");
 Check(current["AutoLoadPals"][0].value("Enabled",true),"Absent Enabled did not default on");
 Check(current["unrelatedPreference"]==42 && legacy==untouched,"Source or preferences changed");
 Check(current["CharPart"]==legacy["CharPart"] && current["Table"]==legacy["Table"],"Metadata lost");
 Check(!LegacyConfigMigration::Merge(current,legacy,"original-documents").changed,"Repeated import changed settings");
 current["AutoLoadPals"].erase(0);
 Check(LegacyConfigMigration::Merge(current,legacy,"original-documents").imported==0,"Deleted assignment resurrected");
 auto existing=Entry("Cerebella",0,"C:/new/current.pal");existing["Enabled"]=false;
 json combined={{"CharPart","custom-current.json"},{"AutoLoadPals",json::array({existing})}};
 result=LegacyConfigMigration::Merge(combined,legacy,"legacy");
 Check(result.imported==1 && combined["AutoLoadPals"][0]==existing,"Current disabled entry overwritten");
 Check(combined["CharPart"]=="custom-current.json","Current labels overwritten");
 json duplicates={{"AutoLoadPals",json::array({Entry("Cerebella",0,"first.pal"),Entry("Cerebella",0,"second.pal")})}};
 json target=json::object();result=LegacyConfigMigration::Merge(target,duplicates,"duplicates");
 Check(result.imported==1 && target["AutoLoadPals"][0]["PathToPal"]=="first.pal","Duplicate precedence changed");
 json bad={{"AutoLoadPals",json::array({Entry("Filia",-1,"bad.pal"),json{{"CharName","Cerebella"},{"PalNumber","0"},{"PathToPal","bad.pal"}},Entry("Cerebella",0,"valid.pal")})}};
 target=json::object();result=LegacyConfigMigration::Merge(target,bad,"repairable");
 Check(result.invalid==2 && result.imported==1 && !target.contains("ImportedLegacyConfigSources"),"Malformed legacy handling");
 bad["AutoLoadPals"]=json::array({Entry("Cerebella",0,"valid.pal"),Entry("Filia",1,"fixed.pal")});
 Check(LegacyConfigMigration::Merge(target,bad,"repairable").imported==1,"Repaired source not retried");
 target={{"AutoLoadPals","leave-current-data-alone"}};const auto copy=target;
 Check(LegacyConfigMigration::Merge(target,legacy,"legacy").invalid==1 && target==copy,"Invalid current data replaced");
 auto invalidEnabled=Entry("Filia",0,"x.pal");invalidEnabled["Enabled"]="yes";
 Check(!LegacyConfigMigration::ValidAssignment(invalidEnabled),"Bad Enabled accepted");
 Check(!LegacyConfigMigration::ValidAssignment(Entry("Filia",256,"x.pal")),"Out-of-range index accepted");
 if(argc==3) {
  std::ifstream currentInput(argv[1]),legacyInput(argv[2]);json actualCurrent,actualLegacy;currentInput>>actualCurrent;legacyInput>>actualLegacy;
  const auto oldCurrent=actualCurrent,oldLegacy=actualLegacy;
  auto migration=LegacyConfigMigration::Merge(actualCurrent,actualLegacy,"read-only-dry-run");
  Check(actualLegacy==oldLegacy,"Dry run changed source");
  for(const auto& entry:oldCurrent["AutoLoadPals"]) {
   bool retained=false;for(const auto& merged:actualCurrent["AutoLoadPals"])if(entry==merged)retained=true;
   Check(retained,"Dry run lost current assignment");
  }
  std::cout<<"Actual legacy config dry run: imported="<<migration.imported<<", invalid="<<migration.invalid<<", original assignments retained; no files written.\n";
 }
 std::cout<<"Legacy settings migration tests passed: original schema, precedence, disabled entries, one-time import, deletion, malformed rows, repair retry and untouched source.\n";
}
