// Opt-in research probe, compiled only into validation DLLs. It deliberately
// does not alter saved palette documents, assignments, or user settings.
namespace {
struct OverlayProbe {
    OverlayEditor::Original owner;
    ULONGLONG deadline;
    nlohmann::json report;
};
std::optional<OverlayProbe> overlayProbe;
void ReportOverlayProbe(nlohmann::json report) {
    PaletteData::AtomicWriteText("dll-overlay-probe-result.json",report.dump(2));
}
}
bool TickOverlayProbe() {
    using namespace OverlayEditor;
    if(overlayProbe) {
        auto& probe=*overlayProbe;
        if(!Valid(probe.owner)) {
            auto report=probe.report;report["state"]="Match changed; discarded stale pointers";report["restored"]=false;
            overlayProbe.reset();ReportOverlayProbe(report);return false;
        }
        if(GetTickCount64()<probe.deadline)return true;
        auto report=probe.report;
        try {
            PausedWrite(probe.owner.address,probe.owner.header);Refresh();
            report["restored"]=Read(probe.owner.address,12)==probe.owner.header;
            report["state"]="Finished; visual result requires user confirmation";
        }catch(const std::exception& e){report["state"]="Restoration failed";report["error"]=e.what();report["restored"]=false;}
        overlayProbe.reset();ReportOverlayProbe(report);return false;
    }
    if(!std::filesystem::exists("dll-overlay-probe.request.json"))return false;
    std::optional<Original> rollback;
    try {
        auto path=std::filesystem::path("dll-overlay-probe.request.json");
        if(std::filesystem::file_size(path)>4096)throw std::runtime_error("Probe request too large.");
        nlohmann::json request;std::ifstream(path)>>request;std::filesystem::remove(path);
        int slot=PaletteData::Integer(request.at("slot"),0,5,"slot");
        int first=PaletteData::Integer(request.at("firstBand"),1,255,"first band");
        int second=PaletteData::Integer(request.at("secondBand"),1,255,"second band");
        int seconds=PaletteData::Integer(request.value("seconds",20),5,45,"probe duration");
        auto firstTexture=request.value("firstTexture",std::string("Fire"));
        auto secondTexture=request.value("secondTexture",std::string("Water"));
        if(first==second)throw std::runtime_error("Probe groups must be distinct.");
        auto c=Model(slot);auto data=AppearanceData(c);
        CheckAppearanceBand(data,first);CheckAppearanceBand(data,second);Catalog();
        if(!templates.count(firstTexture) || !templates.count(secondTexture))throw std::runtime_error("Requested texture templates are unavailable.");
        DWORD address=Word(data.palette+0x14)+12*c.Current_Pallete_Num;
        Original owner{data.actor,data.palette,address,slot,Read(address,12)};
        Bytes records;
        for(auto entry:{std::pair{firstTexture,first},std::pair{secondTexture,second}}) {
            auto record=templates.at(entry.first);DWORD bands=Allocate(1);Write(bands,Bytes{(unsigned char)entry.second});
            record[0]=record[1]=0;Put(record,32,bands);Put(record,36,bands+1);Put(record,40,bands+1);
            record[44]=1;record[46]=255;Put(record,48,0);Put(record,88,0);record[92]=0;
            std::fill(record.begin()+96,record.end(),0);Put(record,116,15);
            records.insert(records.end(),record.begin(),record.end());
        }
        DWORD memory=Allocate(records.size());Write(memory,records);
        Bytes header(12);Put(header,0,memory);Put(header,4,memory+(DWORD)records.size());Put(header,8,memory+(DWORD)records.size());
        rollback=owner;
        AppearanceTransaction(owner,[&]{Write(address,header);});Refresh();
        nlohmann::json report={{"state","Active"},{"character",c.Char_Name},{"palette",c.Current_Pallete_Num+1},
            {"firstTexture",firstTexture},{"firstGroup",data.groups[first].label},
            {"secondTexture",secondTexture},{"secondGroup",data.groups[second].label},
            {"firstBand",first},{"secondBand",second},{"seconds",seconds},{"nativeRecordCount",2},{"restored",false}};
        overlayProbe=OverlayProbe{owner,GetTickCount64()+seconds*1000,report};ReportOverlayProbe(report);
        return true;
    }catch(const std::exception& e) {
        bool restored=!rollback;
        if(rollback)try{if(Valid(*rollback)){PausedWrite(rollback->address,rollback->header);Refresh();restored=Read(rollback->address,12)==rollback->header;}}catch(...){}
        overlayProbe.reset();ReportOverlayProbe({{"state","Probe failed"},{"error",e.what()},{"restored",restored}});return false;
    }
}
