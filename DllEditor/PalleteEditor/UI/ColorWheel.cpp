#include "pch.h"
#include "ColorWheel.h"
#include "PaletteRamp.h"
#include "PlayableCharactersManager.h"
#include "Tools/Colors.hpp"
#include "AdvancedPalette.h"

namespace {
struct Studio {
    bool open=false,linked=true,dragging=false,hex=false;
    int slot=-1,palette=-1,selected=0;
    ColorGroup group;
    std::string character,status;
    std::vector<ImU32> original,source,undoBefore,undoAfter;
    float hue=0,saturation=0,brightness=0;
} studio;
std::vector<ImU32> Draft() {
    auto result=studio.source;
    for(auto& c:result)c=PaletteRamp::Adjust(c,studio.hue/360,studio.saturation,studio.brightness);
    return result;
}
void Bake() {studio.source=Draft();studio.hue=studio.saturation=studio.brightness=0;}
bool SameTarget() {
    const auto& c=PlayableCharactersManager::GetCurrentCharacter();
    return studio.slot==PlayableCharactersManager::GetCurrentCharacterIndex() && studio.palette==c.Current_Palette_Num && studio.character==c.Char_Name;
}
std::vector<ImU32> Live() {
    const auto& colors=PlayableCharactersManager::GetCurrentCharacter().Character_Colors;
    if(!SameTarget() || studio.group.startIndex<0 || studio.group.count<1 || size_t(studio.group.startIndex)+studio.group.count>colors.size())return {};
    return {colors.begin()+studio.group.startIndex,colors.begin()+studio.group.startIndex+studio.group.count};
}
void Strip(const char* label,const std::vector<ImU32>& colors,bool selectable) {
    ImGui::TextUnformatted(label);
    const float width=std::max(24.f,std::min(48.f,(ImGui::GetContentRegionAvail().x-8*colors.size())/std::max(size_t(1),colors.size())));
    for(int i=0;i<int(colors.size());++i) {
        ImGui::PushID(i);
        if(ImGui::ColorButton("##ramp",ColorsTools::ImU32BGRAtoImVec4RGBA(colors[i]),0,ImVec2(width,32)) && selectable)studio.selected=i;
        if(ImGui::IsItemHovered())ImGui::SetTooltip("Swatch %d | palette index %d",i+1,studio.group.startIndex+i);
        if(selectable && i==studio.selected) {
            const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddRect(lo,hi,IM_COL32(255,255,255,255),0,0,2);
        }
        ImGui::PopID();if(i+1<int(colors.size()) && (i+1)%10)ImGui::SameLine();
    }
}
void Wheel(float size) {
    const auto origin=ImGui::GetCursorScreenPos();
    const ImVec2 center(origin.x+size/2,origin.y+size/2);const float radius=size/2-15;
    ImGui::InvisibleButton("Hue saturation wheel",ImVec2(size,size));
    const bool active=ImGui::IsItemActive(),clicked=ImGui::IsItemActivated();
    auto* draw=ImGui::GetWindowDrawList();
    // Saturation interpolates from white at the center to full hue at the edge.
    for(int ring=0;ring<12;++ring)for(int seg=0;seg<96;++seg) {
        float a0=seg*2*IM_PI/96,a1=(seg+1)*2*IM_PI/96;
        float r0=radius*ring/12,r1=radius*(ring+1)/12;
        ImVec2 p0(center.x+cosf(a0)*r0,center.y+sinf(a0)*r0),p1(center.x+cosf(a1)*r0,center.y+sinf(a1)*r0);
        ImVec2 p2(center.x+cosf(a1)*r1,center.y+sinf(a1)*r1),p3(center.x+cosf(a0)*r1,center.y+sinf(a0)*r1);
        auto color=ColorsTools::ImU32BGRAtoImVec4RGBA(PaletteRamp::Encode({(seg+.5f)/96,(ring+.5f)/12,1},0xff000000));
        ImVec2 points[]={p0,p1,p2,p3};draw->AddConvexPolyFilled(points,4,ImGui::ColorConvertFloat4ToU32(color));
    }
    auto colors=Draft();
    if(clicked) {
        // Prefer the selected node when nodes overlap. Swatch strip selects hidden nodes.
        float nearest=18*18.f;int hit=-1;
        for(int n=int(colors.size())-1;n>=0;--n) {
            int i=(n+studio.selected+1)%int(colors.size());auto c=PaletteRamp::Decode(colors[i]);
            float x=center.x+cosf(c.h*2*IM_PI)*radius*c.s,y=center.y+sinf(c.h*2*IM_PI)*radius*c.s;
            float dx=ImGui::GetIO().MousePos.x-x,dy=ImGui::GetIO().MousePos.y-y,d=dx*dx+dy*dy;
            if(d<=nearest){hit=i;nearest=d;}
        }
        if(hit>=0)studio.selected=hit;
        Bake();studio.dragging=true;
    }
    if(active && studio.dragging) {
        float dx=ImGui::GetIO().MousePos.x-center.x,dy=ImGui::GetIO().MousePos.y-center.y;
        float hue=PaletteRamp::Wrap(atan2f(dy,dx)/(2*IM_PI)),sat=std::clamp(sqrtf(dx*dx+dy*dy)/radius,0.f,1.f);
        auto anchor=PaletteRamp::Decode(studio.source[studio.selected]);
        if(studio.linked) {
            studio.hue=PaletteRamp::HueDelta(anchor.h,hue)*360;
            studio.saturation=PaletteRamp::ShiftTo(anchor.s,sat);
        }else {
            anchor.h=hue;anchor.s=sat;
            studio.source[studio.selected]=PaletteRamp::Encode(anchor,studio.source[studio.selected]);
        }
        colors=Draft();
    }
    if(!active)studio.dragging=false;
    // Connecting nodes makes the linked ramp relationship visible.
    ImVec2 previous;bool first=true;
    for(int i=0;i<int(colors.size());++i) {
        auto c=PaletteRamp::Decode(colors[i]);
        ImVec2 pt(center.x+cosf(c.h*2*IM_PI)*radius*c.s,center.y+sinf(c.h*2*IM_PI)*radius*c.s);
        if(studio.linked && !first)draw->AddLine(previous,pt,IM_COL32(15,15,15,160),2);
        previous=pt;first=false;
        draw->AddCircleFilled(pt,i==studio.selected?10.f:7.f,ImGui::ColorConvertFloat4ToU32(ColorsTools::ImU32BGRAtoImVec4RGBA(colors[i])),24);
        draw->AddCircle(pt,i==studio.selected?11.f:8.f,i==studio.selected?IM_COL32(255,255,255,255):IM_COL32(15,15,15,255),24,2);
    }
}
}

void ColorWheel::Close() {studio=Studio{};}
void ColorWheel::Open(const ColorGroup& group) {
    const auto& c=PlayableCharactersManager::GetCurrentCharacter();
    if(studio.open && Draft()!=studio.original) {
        studio.status="Finish or cancel the current group before opening another.";return;
    }
    Close();studio.slot=PlayableCharactersManager::GetCurrentCharacterIndex();studio.palette=c.Current_Palette_Num;
    studio.character=c.Char_Name;studio.group=group;studio.original=Live();studio.source=studio.original;
    studio.open=!studio.source.empty();
}
void ColorWheel::Draw() {
    if(!studio.open)return;
    ImGui::SetNextWindowSize(ImVec2(880,780),ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(1.f);
    ImGui::SetNextWindowSizeConstraints(ImVec2(800,740),ImVec2(FLT_MAX,FLT_MAX));
    // Stable identity and title, independent of changing group and status text.
    if(!ImGui::Begin("Color Studio###ColorStudioRamp",nullptr,ImGuiWindowFlags_NoCollapse)){ImGui::End();return;}
    ImGui::Text("Team %d | %s | Palette %d | %s",studio.slot/3+1,studio.character.c_str(),studio.palette+1,studio.group.groupName.c_str());
    ImGui::TextWrapped("Staged colors. Apply changes the match; saving remains a separate action.");
    if(!SameTarget())ImGui::TextWrapped("Return to this target to apply, or Cancel to discard the staged colors.");
    const auto draft=Draft();const auto live=Live();
    const bool conflict=SameTarget() && live!=studio.original;
    if(conflict)ImGui::TextWrapped("Match colors changed outside this panel. Reload before applying.");
    ImGui::PushID("Before");Strip("Before",studio.original,false);ImGui::PopID();
    ImGui::PushID("After");Strip("After - select a swatch",draft,true);ImGui::PopID();
    ImGui::Separator();
    ImGui::BeginChild("Wheel",ImVec2(340,-70),false);
    ImGui::Checkbox("Link whole group",&studio.linked);
    ImGui::TextWrapped(studio.linked?"Drag any node to move the ramp together.":"Drag a node to change one swatch.");
    Wheel(310);
    ImGui::EndChild();ImGui::SameLine();
    ImGui::BeginChild("Adjustments",ImVec2(0,-70),false);
    ImGui::SeparatorText("Whole group adjustments");
    ImGui::SetNextItemWidth(-100);ImGui::SliderFloat("Hue shift",&studio.hue,-180,180,"%.0f deg");
    ImGui::SetNextItemWidth(-100);ImGui::SliderFloat("Saturation",&studio.saturation,-1,1,"%.2f");
    ImGui::SetNextItemWidth(-100);ImGui::SliderFloat("Brightness",&studio.brightness,-1,1,"%.2f");
    ImGui::TextDisabled("About linked adjustments (?)");
    if(ImGui::IsItemHovered())ImGui::SetTooltip("Hue retains shading values. Saturation and brightness move proportionally\ntoward their endpoints, preserving ramp order. Swatches stay in native order.");
    ImGui::SeparatorText("Selected swatch");
    ImGui::Text("Swatch %d | palette index %d",studio.selected+1,studio.group.startIndex+studio.selected);
    auto color=ColorsTools::ImU32BGRAtoImVec4RGBA(draft[studio.selected]);
    ImGui::Checkbox("Hex values",&studio.hex);
    ImGui::SetNextItemWidth(-1);
    if(ImGui::ColorEdit4("##Selected",&color.x,ImGuiColorEditFlags_NoPicker|(studio.hex?ImGuiColorEditFlags_DisplayHex:ImGuiColorEditFlags_DisplayRGB)|ImGuiColorEditFlags_Uint8)) {
        Bake();studio.source[studio.selected]=ColorsTools::ImVec4RGBAtoImU32BGRA(color);
    }
    ImGui::SetNextItemWidth(230);
    if(ImGui::ColorPicker4("##Picker",&color.x,ImGuiColorEditFlags_PickerHueBar|ImGuiColorEditFlags_NoSidePreview|ImGuiColorEditFlags_NoInputs|ImGuiColorEditFlags_NoAlpha)) {
        Bake();auto converted=ColorsTools::ImVec4RGBAtoImU32BGRA(color);
        studio.source[studio.selected]=(converted&0xffffff)|(studio.source[studio.selected]&0xff000000);
    }
    ImGui::EndChild();ImGui::Separator();
    ImGui::BeginDisabled(!SameTarget() || conflict || draft==studio.original || AdvancedPalette::Busy());
    if(ImGui::Button("Apply group",ImVec2(140,0))) {
        studio.undoBefore=studio.original;studio.undoAfter=Draft();bool ok=true;
        for(int i=0;i<int(studio.undoAfter.size());++i)ok=PlayableCharactersManager::ChangePaletteColor(studio.group.startIndex+i,studio.undoAfter[i]) && ok;
        studio.original=Live();studio.source=studio.original;studio.hue=studio.saturation=studio.brightness=0;
        studio.status=ok?"Applied group colors.":"Some colors could not be applied. Reload and check the match.";
        if(!ok){studio.undoBefore.clear();studio.undoAfter.clear();}
    }
    ImGui::EndDisabled();ImGui::SameLine();
    ImGui::BeginDisabled(studio.undoBefore.empty() || live!=studio.undoAfter || draft!=studio.original || AdvancedPalette::Busy());
    if(ImGui::Button("Undo apply",ImVec2(140,0))) {
        bool ok=true;
        for(int i=0;i<int(studio.undoBefore.size());++i)ok=PlayableCharactersManager::ChangePaletteColor(studio.group.startIndex+i,studio.undoBefore[i]) && ok;
        studio.original=Live();studio.source=studio.original;studio.hue=studio.saturation=studio.brightness=0;
        studio.undoBefore.clear();studio.undoAfter.clear();studio.status=ok?"Restored previous group colors.":"Undo incomplete. Check the match.";
    }
    ImGui::EndDisabled();ImGui::SameLine();
    if(ImGui::Button("Reset draft",ImVec2(140,0))) {studio.source=studio.original;studio.hue=studio.saturation=studio.brightness=0;studio.status="Draft reset.";}
    ImGui::SameLine();ImGui::BeginDisabled(!SameTarget());
    if(ImGui::Button("Reload match",ImVec2(140,0))) {studio.original=Live();studio.source=studio.original;studio.hue=studio.saturation=studio.brightness=0;studio.undoBefore.clear();studio.undoAfter.clear();studio.status="Loaded current match colors.";}
    ImGui::EndDisabled();ImGui::SameLine();
    if(ImGui::Button("Cancel / close",ImVec2(140,0)))Close();
    ImGui::TextWrapped("%s",studio.status.c_str());
    ImGui::End();
}
