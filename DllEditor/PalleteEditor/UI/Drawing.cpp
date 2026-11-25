#include "pch.h"
#include "Drawing.h"
#include "../../../ReleaseVersion.h"
#include "AdvancedPalette.h"
#include "MainThread.h"

#include "PlayableCharactersManager.h"
#include "AutoLoadPalette.h"

#include "Tools/Colors.hpp"

#include "Files/PaletteFiles.h"
#include "Files/GroupJSONFiles.h"
#include "Files/Config.h"

#include "ImGuiCustom.h"
#include "UI/Eyedropper/Eyedropper.h"
#include "UI/ColorWheel.h"
#include "SpriteViewer.h"
#include "PalettePasteHistory.h"
#include "UI/LoggerWidget.hpp"

#include <filesystem>


auto DrawingLogger = LOGGER::createLocal("Drawing", LogLevel::GENERAL_LOG);
static bool saveAndAssign=false,saveLegacy=false;
static std::string copiedGroup;
static int selectedColorGroup=0;
static bool& PreviewWindowOpen() {
    static bool open=config::get_bool("dll_preview_window",false);
    return open;
}
static int requestedSlot=-1, requestedPalette=-1;
static bool openAdvanced=false, switchAfterApply=false;
static std::string spritePreparationOutput;
using PasteUndo=PalettePasteHistory::Entry;
static std::vector<PasteUndo> pasteUndo;

static bool FindPreparationScript(std::filesystem::path& script) {
    HMODULE module=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&FindPreparationScript),&module))return false;
    std::wstring modulePath(32768,L'\0');
    DWORD length=GetModuleFileNameW(module,&modulePath[0],static_cast<DWORD>(modulePath.size()));
    if(!length || length>=modulePath.size())return false;
    modulePath.resize(length);
    script=std::filesystem::path(modulePath).parent_path()/L"prepare-dll-preview.py";
    std::error_code ec;
    return std::filesystem::is_regular_file(script,ec) && !ec;
}

static void AppendPreparationOutput(const char* data,size_t length) {
    spritePreparationOutput.append(data,length);
    if(spritePreparationOutput.size()>8192)spritePreparationOutput.erase(0,spritePreparationOutput.size()-8192);
}

static void DrainPreparationOutput() {
    if(!SpriteViewer::preparationPipe)return;
    for(;;) {
        DWORD available=0;
        if(!PeekNamedPipe(SpriteViewer::preparationPipe,nullptr,0,nullptr,&available,nullptr)) {
            if(GetLastError()!=ERROR_BROKEN_PIPE)SpriteViewer::preparationStatus="Could not read preview preparation output.";
            break;
        }
        if(!available)break;
        char buffer[2048];DWORD read=0;
        if(!ReadFile(SpriteViewer::preparationPipe,buffer,(std::min)(available,DWORD(sizeof(buffer))),&read,nullptr) || !read)break;
        AppendPreparationOutput(buffer,read);
    }
}

static void StartSpritePreparation(const std::filesystem::path& archivePath) {
    const auto dataFolder=archivePath.parent_path();
    const auto fileName=archivePath.filename().wstring();
    if(_wcsicmp(dataFolder.filename().c_str(),L"data01")!=0 ||
        (_wcsicmp(fileName.c_str(),L"characters-foits.gfs")!=0 && _wcsicmp(fileName.c_str(),L"characters-foits-retail.gfs")!=0)) {
        SpriteViewer::preparationStatus="Select characters-foits.gfs from the local game's data01 folder.";
        return;
    }
    std::filesystem::path script;
    if(!FindPreparationScript(script)) {
        SpriteViewer::preparationStatus="The local prepare-dll-preview.py helper was not found beside the DLL.";
        return;
    }
    std::wstring python=SpriteViewer::preparationPython.wstring();
    if(python.empty()) {
        python.resize(32768,L'\0');
        DWORD pythonLength=SearchPathW(nullptr,L"python.exe",nullptr,static_cast<DWORD>(python.size()),&python[0],nullptr);
        if(!pythonLength || pythonLength>=python.size()) {
            SpriteViewer::preparationStatus="Python 3 was not found on PATH. Use Choose Python executable.";
            return;
        }
        python.resize(pythonLength);
    }
    const auto gameDirectory=dataFolder.parent_path().wstring();
    const auto outputDirectory=std::filesystem::current_path().wstring();
    std::wstring command=L"\""+python+L"\" \""+script.wstring()+L"\" \""+gameDirectory+L"\" \""+outputDirectory+L"\"";
    SECURITY_ATTRIBUTES security={sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};
    HANDLE readPipe=nullptr,writePipe=nullptr;
    if(!CreatePipe(&readPipe,&writePipe,&security,0)) {
        SpriteViewer::preparationStatus="Could not create the preview preparation output pipe.";
        return;
    }
    SetHandleInformation(readPipe,HANDLE_FLAG_INHERIT,0);
    STARTUPINFOW startup={};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES;
    startup.hStdOutput=writePipe;startup.hStdError=writePipe;startup.hStdInput=GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process={};
    BOOL started=CreateProcessW(python.c_str(),&command[0],nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,
        gameDirectory.c_str(),&startup,&process);
    DWORD createError=started?ERROR_SUCCESS:GetLastError();
    CloseHandle(writePipe);
    if(!started) {
        CloseHandle(readPipe);
        SpriteViewer::preparationStatus="Could not start Python preview preparation (Windows error "+std::to_string(createError)+").";
        return;
    }
    CloseHandle(process.hThread);
    SpriteViewer::preparationProcess=process.hProcess;
    SpriteViewer::preparationPipe=readPipe;
    spritePreparationOutput.clear();
    SpriteViewer::preparationStatus="Generating the combined local preview asset. This can take several minutes.";
}

static void PollSpritePreparation() {
    if(!SpriteViewer::preparationProcess)return;
    DrainPreparationOutput();
    DWORD wait=WaitForSingleObject(SpriteViewer::preparationProcess,0);
    if(wait==WAIT_TIMEOUT)return;
    if(wait!=WAIT_OBJECT_0) {
        SpriteViewer::preparationStatus="Could not check the Python preview preparation process.";
        CloseHandle(SpriteViewer::preparationProcess);SpriteViewer::preparationProcess=nullptr;
        if(SpriteViewer::preparationPipe){CloseHandle(SpriteViewer::preparationPipe);SpriteViewer::preparationPipe=nullptr;}
        return;
    }
    DrainPreparationOutput();
    DWORD exitCode=1;
    if(!GetExitCodeProcess(SpriteViewer::preparationProcess,&exitCode)) {
        SpriteViewer::preparationStatus="Could not read the Python preview preparation exit code.";
        CloseHandle(SpriteViewer::preparationProcess);SpriteViewer::preparationProcess=nullptr;
        if(SpriteViewer::preparationPipe){CloseHandle(SpriteViewer::preparationPipe);SpriteViewer::preparationPipe=nullptr;}
        return;
    }
    CloseHandle(SpriteViewer::preparationProcess);SpriteViewer::preparationProcess=nullptr;
    CloseHandle(SpriteViewer::preparationPipe);SpriteViewer::preparationPipe=nullptr;
    if(exitCode==0) {
        SpriteViewer::ResetAsset();
        SpriteViewer::preparationStatus="Combined local sprite asset prepared. "+spritePreparationOutput;
    } else {
        SpriteViewer::preparationStatus="Preview preparation failed (exit "+std::to_string(exitCode)+"). "+spritePreparationOutput;
    }
}

static std::filesystem::path PathFromUtf8(const std::string& value) {
    std::u8string encoded;
    encoded.reserve(value.size());
    for(unsigned char byte:value)encoded.push_back(static_cast<char8_t>(byte));
    return std::filesystem::path(encoded);
}

static void RequestTarget(int slot,int palette) {
    requestedSlot=slot; requestedPalette=palette;
}
static void CommitTarget() {
    if(requestedSlot>=0) {
        PlayableCharactersManager::instance().SetCurrentCharacterIndex(requestedSlot);
        PlayableCharactersManager::LoadCharacter();
    } else if(requestedPalette>=0) PlayableCharactersManager::ChangePaletteNumber(requestedPalette);
    AdvancedPalette::PaletteChanged();
    requestedSlot=requestedPalette=-1;
    pasteUndo.clear();
}



void Drawing::Draw()
{
    if(!MainThread::Ready)return;
    MainLoop();
    AdvancedPalette::Tick();
    static unsigned long long editGeneration=0;
    if(editGeneration!=AdvancedPalette::MatchGeneration) {
        pasteUndo.clear();ColorWheel::Close();
        requestedSlot=requestedPalette=-1;switchAfterApply=false;
        editGeneration=AdvancedPalette::MatchGeneration;
    }
    if(!MainThread::Match_Readed)ColorWheel::Close();
	if ((GetAsyncKeyState(VK_INSERT) & 1) || (GetAsyncKeyState(VK_F1) & 1)) {
		bDrawAll = !bDrawAll;
	}

	if (bDrawAll)
	{
		ImGui::SetNextWindowSize(vWindowSize, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(420,500),ImVec2(FLT_MAX,FLT_MAX));
		ImGui::SetNextWindowBgAlpha(1.0f);

		ImGui::Begin(lpWindowName, &bDrawAll, WindowFlags);
		{

			Developer::DrawDev();		
			DrawColorPickerWindow();
			DrawMenuBar();
			DrawFileDialog();
			DrawAboutWindow();

			EyeDropper::getInstance().EyeDropperToolTip();

			if(MainThread::Match_Readed) {
                DrawPlayableCharactersComboBox();
                if(PlayableCharactersManager::GetCurrentCharacterIndex()>=0) {
                    DrawCharacterPaletteNumSlider();
                    const auto& target=PlayableCharactersManager::GetCurrentCharacter();
                    ImGui::TextWrapped("Team %d | %s | Palette %d",PlayableCharactersManager::GetCurrentCharacterIndex()/3+1,target.Char_Name,target.Current_Palette_Num+1);
                    bool assigned=false;
                    for(const auto& entry:AutoPalette::Auto_Pals)if(entry.Enabled && entry.CharName==target.Char_Name && entry.PalNum==target.Current_Palette_Num) {
                        ImGui::TextWrapped("Autoload: %s (%s)",entry.PalPath.c_str(),entry.Status.c_str());assigned=true;break;
                    }
                    if(!assigned)ImGui::TextDisabled("Autoload: None");
                }
            } else pasteUndo.clear();
            if(switchAfterApply && !AdvancedPalette::Busy()) {
                switchAfterApply=false;
                if(!AdvancedPalette::HasPending() && AdvancedPalette::Status=="Applied")CommitTarget();
                else requestedSlot=requestedPalette=-1;
            }
            if((requestedSlot>=0 || requestedPalette>=0) && !switchAfterApply) {
                if(AdvancedPalette::Busy()) { requestedSlot=requestedPalette=-1; }
                else if(AdvancedPalette::HasPending())ImGui::OpenPopup("Pending edits");
                else CommitTarget();
            }
            if(ImGui::BeginPopupModal("Pending edits",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::TextUnformatted("Apply or discard pending overlay assignments before changing target.");
                if(ImGui::Button("Apply")) { AdvancedPalette::ApplyPending(); switchAfterApply=true; ImGui::CloseCurrentPopup(); }
                ImGui::SameLine();
                if(ImGui::Button("Discard")) { AdvancedPalette::DiscardPending(); CommitTarget(); ImGui::CloseCurrentPopup(); }
                ImGui::SameLine();
                if(ImGui::Button("Cancel")) { requestedSlot=requestedPalette=-1; ImGui::CloseCurrentPopup(); }
                ImGui::EndPopup();
            }
            if (ImGui::BeginTabBar("##TabBar")) {

				if (ImGui::BeginTabItem("Palette")) {
					DrawPaletteTabItem();
					ImGui::EndTabItem();
				}
                if (ImGui::BeginTabItem("Advanced",nullptr,openAdvanced?ImGuiTabItemFlags_SetSelected:0)) {
                    openAdvanced=false;
                    if (!MainThread::Match_Readed) {
                        ImGui::TextUnformatted("Start a match to use advanced settings.");
                    } else {
                        if (PlayableCharactersManager::GetCurrentCharacterIndex() != -1) {
                            ImGui::Separator();
                            DrawCharacterOptions();
                            AdvancedPalette::Draw();
                        }
                    }
                    ImGui::EndTabItem();
                }
				if (ImGui::BeginTabItem("Auto-load")) {
					DrawAutoLoadPaletteTabItem();				
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Options")) {
					DrawOptionsTabItem();
					ImGui::EndTabItem();
				}
				ImGui::EndTabBar();
			}
			
		}
		ImGui::End();
        ColorWheel::Draw();
        DrawSpritePreviewWindow();

	}
}



void Drawing::DrawMenuBar() {
	int& Curent_Index = PlayableCharactersManager::GetCurrentCharacterIndex();
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (Curent_Index == -1) { //If we haven't Loaded Character;
				ImGui::TextDisabled("Save Palette");
				if (ImGui::IsItemHovered()) {
					if (ImGui::BeginItemTooltip())
					{
						ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
						ImGui::TextUnformatted("You need to load/select a character first!");
						ImGui::PopTextWrapPos();
						ImGui::EndTooltip();
					}
				}

				ImGui::TextDisabled("Load Palette");
				if (ImGui::IsItemHovered()) {
					if (ImGui::BeginItemTooltip())
					{
						ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
						ImGui::TextUnformatted("You need to load/select a character first!");
						ImGui::PopTextWrapPos();
						ImGui::EndTooltip();
					}
				}

			}
			else {
				if (ImGui::MenuItem("Save Palette"))
				{
                    saveAndAssign=false;saveLegacy=false;
					IGFD::FileDialogConfig config;
					config.path = ".";
					config.flags = ImGuiFileDialogFlags_ConfirmOverwrite;
					ImGuiFileDialog::Instance()->OpenDialog("SavePaletteFile", "Save Palette File", ".pal", config);
				}

                if(ImGui::MenuItem("Save palette and auto-load")) {
                    saveAndAssign=true;saveLegacy=false;
                    IGFD::FileDialogConfig cfg;cfg.path=".";cfg.flags=ImGuiFileDialogFlags_ConfirmOverwrite;
                    ImGuiFileDialog::Instance()->OpenDialog("SavePaletteFile","Save palette and auto-load",".pal",cfg);
                }
                if(ImGui::MenuItem("Export legacy colors only")) {
                    saveAndAssign=false;saveLegacy=true;
                    IGFD::FileDialogConfig cfg;cfg.path=".";cfg.flags=ImGuiFileDialogFlags_ConfirmOverwrite;
                    ImGuiFileDialog::Instance()->OpenDialog("SavePaletteFile","Export legacy colors only",".pal",cfg);
                }
				if (ImGui::MenuItem("Load Palette"))
				{
					IGFD::FileDialogConfig config;
					config.path = ".";
					ImGuiFileDialog::Instance()->OpenDialog("LoadPaletteFile", "Load Palette File", ".pal", config);

				}


			}
			ImGui::Separator();
			if (ImGui::MenuItem("Load JSON (Characters parts)"))
			{
				IGFD::FileDialogConfig config;
				config.path = ".";
				ImGuiFileDialog::Instance()->OpenDialog("LoadJSON", "Load JSON (Characters parts)", ".json", config);
			}
			ImGui::EndMenu();
		}

		// Кнопка About прямо в меню баре
		if (ImGui::MenuItem("About"))
		{
			// Открываем окно About при нажатии
			bDrawAboutWindow = true;

		}
		ImGui::EndMenuBar();

	}
}

void Drawing::DrawAboutWindow() {
	if (!bDrawAboutWindow) return;
	ImGui::Begin("About", &bDrawAboutWindow);
	{
		ImGui::Text("Skullgirls Palette Editor (DLL Version)");
		ImGui::TextDisabled("Version: " SGPE_VERSION);
		ImGui::TextDisabled("Author: ImpDi");
		ImGui::Text("Also, check our Discord: ");
		ImGui::TextLinkOpenURL("Discord", "https://discord.gg/4ufGJQjkpc");
		ImGui::Separator();
		if (ImGui::Button("OK"))
		{
			bDrawAboutWindow = false;
		}
		ImGui::SameLine();
		ImGui::Checkbox("Super secret Dev Window", &bDrawDevWindow);
	}
	ImGui::End();
}

void Drawing::DrawPaletteTabItem() {
	PlayableCharactersManager& charMgr = PlayableCharactersManager::instance();
	if (MainThread::Match_Readed != true) {
		ImGui::Text("Start a match to use the editor.");
		return;
	}
	if (charMgr.GetCurrentCharacterIndex() == -1) return;

	ImGui::Spacing();

    if (!AdvancedPalette::Status.empty())
        ImGui::TextWrapped("%s", AdvancedPalette::Status.c_str());

	ImGui::Separator();


	ImGui::Separator();

    DrawPaletteWideColors();
	DrawCharacterColors();

}

void Drawing::DrawPlayableCharactersComboBox() {
	PlayableCharactersManager& charMgr = PlayableCharactersManager::instance();
	auto& Curent_Char = PlayableCharactersManager::GetCurrentCharacter();
	int& Curent_Index = PlayableCharactersManager::GetCurrentCharacterIndex();
	const auto& allCharsNames = charMgr.GetCharacterNames();

	std::string preview_text = "Select";
	if (Curent_Index != -1) {
		preview_text = allCharsNames[Curent_Index].value();
	}
	ImGui::Text("Target");
    ImGui::BeginDisabled(AdvancedPalette::Busy());

	if (ImGui::BeginCombo("##CharSelect", preview_text.c_str())) {

		for (int i = 0; i < MAX_PLAYABLE_CHARACTERS; i++) {
			if (!allCharsNames[i].has_value()) continue; //If we haven't Playable Character in Array

			bool is_selected = (Curent_Index == i);

			std::string display_name = allCharsNames[i].value();

			if (i < 3) {
				display_name += " (Player 1)";
			}
			else {
				display_name += " (Player 2)";
			}

			if (ImGui::Selectable(display_name.c_str(), is_selected)) {
				if (Curent_Index == i) {
					LOG_LOCAL_WARN(DrawingLogger, "We are trying to select an already selected character.");
					continue;
				}
				// Сохраняем ID выбранного персонажа
				LOG_LOCAL_DEBUG(DrawingLogger, "Choose new Playable Character ", allCharsNames[i].value(), " at ", i);
				m_ActiveColorIndex = 1;
				RequestTarget(i,-1);
				LOG_DEBUG("Loading character from slot: ", i);
				LOG_DEBUG("Character name: ", Curent_Char.Char_Name);
				LOG_DEBUG("Character count of palette: ", Curent_Char.Max_Palette_Num);
				LOG_DEBUG("Character current num of Palette: ", Curent_Char.Current_Palette_Num);
				LOG_DEBUG("Character count of colors: ", Curent_Char.Num_Of_Color);

			}

			if (is_selected) {
				ImGui::SetItemDefaultFocus();
			}

		}
		ImGui::EndCombo();
	}

    ImGui::EndDisabled();
}

void Drawing::DrawCharacterOptions() {
	ImGui::Text("Options: ");
	ImGui::Checkbox("Single Color Picker", &bUseColorPickerMode);

	if (bUseColorPickerMode) {
		ImGui::SameLine();
		ImGui::Text("Current Color ID %d", m_ActiveColorIndex);
	}
}

void Drawing::DrawCharacterPaletteNumSlider() {
	auto& Curent_Char = PlayableCharactersManager::GetCurrentCharacter();
	int displayValue = Curent_Char.Current_Palette_Num + 1;

    ImGui::BeginDisabled(AdvancedPalette::Busy());
    bool changed=false;
    if(ImGui::Button("Previous")){--displayValue;changed=true;}
    ImGui::SameLine();ImGui::SetNextItemWidth(100);
    changed=ImGui::InputInt("##Palette number",&displayValue,0,0,ImGuiInputTextFlags_EnterReturnsTrue)||changed;
    ImGui::SameLine();if(ImGui::Button("Next")){++displayValue;changed=true;}
    if(changed && displayValue>=1 && displayValue<=Curent_Char.Max_Palette_Num && displayValue-1!=Curent_Char.Current_Palette_Num)RequestTarget(-1,displayValue-1);
    ImGui::SameLine();ImGui::Text("of %d",Curent_Char.Max_Palette_Num);
    int sliderValue=Curent_Char.Current_Palette_Num+1;
    ImGui::TextUnformatted("Palette slider");
    ImGui::SetNextItemWidth(-1);
    if(ImGui::SliderInt("##Palette slider",&sliderValue,1,Curent_Char.Max_Palette_Num) && sliderValue-1!=Curent_Char.Current_Palette_Num)RequestTarget(-1,sliderValue-1);
    ImGui::EndDisabled();
}

void Drawing::DrawCharacterColors() {

    auto& Curent_Char = PlayableCharactersManager::GetCurrentCharacter();
	ImGui::Text("Colors Palette: %d", Curent_Char.Num_Of_Color);
	auto& colors = Curent_Char.Character_Colors;
    const auto* groupsPtr=GroupColorManager::GetInstance().GetGroupsForCharacter(Curent_Char.Char_Name);
    if(groupsPtr && !groupsPtr->empty()) {
		const auto& groups = *groupsPtr;
		static char searchBuffer[256] = "";

		ImGui::Text("Search in groups:");
		ImGui::InputText("##GroupSearch", searchBuffer, sizeof(searchBuffer));

		auto groupMatchesSearch = [&](const ColorGroup& group) -> bool {
			if (searchBuffer[0] == '\0') return true;

			std::string groupNameLower = group.groupName;
			std::string searchLower = searchBuffer;

			// Приводим к нижнему регистру для регистронезависимого поиска
			std::transform(groupNameLower.begin(), groupNameLower.end(), groupNameLower.begin(), ::tolower);
			std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

			//return groupNameLower.find(searchLower) != std::string::npos; //Mathes
			return groupNameLower.find(searchLower) != std::string::npos; //First latter
			};

        int& selected=selectedColorGroup;
        static std::string character;
        if(character!=Curent_Char.Char_Name){selected=0;character=Curent_Char.Char_Name;}
        if(selected>=groups.size())selected=0;
        if(ImGui::Checkbox("Sprite preview window",&PreviewWindowOpen()))config::set_bool("dll_preview_window",PreviewWindowOpen());
        ImGui::Text("Copied: %s (%u swatches)",copiedGroup.empty()?"Nothing yet":copiedGroup.c_str(),unsigned(s_ClipboardColors.size()));
            ImGui::BeginDisabled(pasteUndo.empty());
            if(ImGui::Button("Undo paste") && !pasteUndo.empty()) {
                auto undo=pasteUndo.back();pasteUndo.pop_back();
                const bool same=undo.CanUndo(PlayableCharactersManager::GetCurrentCharacterIndex(),Curent_Char.Current_Palette_Num,AdvancedPalette::MatchGeneration,colors);
                if(same)for(int i=0;i<undo.before.size();++i)PlayableCharactersManager::ChangePaletteColor(undo.start+i,undo.before[i]);
                else AdvancedPalette::Status="Undo skipped: palette changed since paste.";
            }
            ImGui::EndDisabled();
        ImGui::BeginChild("Color groups",ImVec2(0,0),true);
        const auto& style=ImGui::GetStyle();
        auto buttonWidth=[&](const char* label){return std::ceil(ImGui::CalcTextSize(label).x+style.FramePadding.x*2+4);};
        const float studioWidth=buttonWidth("Color Studio"),effectsWidth=buttonWidth("Effects");
        const float copyWidth=buttonWidth("Copy"),pasteWidth=buttonWidth("Paste matching");
        const float minimumWidth=studioWidth+effectsWidth+copyWidth+pasteWidth+style.CellPadding.x*10+ImGui::GetFrameHeight()*2;
        const float tableWidth=std::max(minimumWidth,ImGui::GetContentRegionAvail().x-style.ScrollbarSize);
        if(ImGui::BeginTable("Grouped palette rows",5,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_ScrollX|ImGuiTableFlags_ScrollY,ImVec2(0,0),tableWidth)) {
            ImGui::TableSetupColumn("Swatches",ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Color Studio",ImGuiTableColumnFlags_WidthFixed,studioWidth);
            ImGui::TableSetupColumn("Effects",ImGuiTableColumnFlags_WidthFixed,effectsWidth);
            ImGui::TableSetupColumn("Copy",ImGuiTableColumnFlags_WidthFixed,copyWidth);
            ImGui::TableSetupColumn("Paste",ImGuiTableColumnFlags_WidthFixed,pasteWidth);
            for(int n=0;n<int(groups.size());++n) {
                const auto& group=groups[n];if(!groupMatchesSearch(group))continue;
                ImGui::PushID(n);ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);
                if(ImGui::Selectable(group.groupName.c_str(),selected==n,ImGuiSelectableFlags_SpanAllColumns))selected=n;
                if(ImGui::IsItemHovered())ImGui::SetTooltip("%s (%d swatches)",group.groupName.c_str(),group.count);
                ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);
                const int perRow=std::max(1,int(ImGui::GetContentRegionAvail().x/(ImGui::GetFrameHeight()+style.ItemSpacing.x)));
                for(int i=group.startIndex;i<group.startIndex+group.count && i<int(colors.size());++i) {
                    DrawColorButton(i,colors[i]);if(ImGui::IsItemActivated())selected=n;
                    if(i+1<group.startIndex+group.count && i+1<int(colors.size()) && (i-group.startIndex+1)%perRow)ImGui::SameLine();
                }
                ImGui::TableSetColumnIndex(1);
                if(ImGui::Button("Color Studio",ImVec2(studioWidth,0))){selected=n;ColorWheel::Open(group);}
                ImGui::TableSetColumnIndex(2);
                if(ImGui::Button("Effects",ImVec2(effectsWidth,0))){selected=n;ImGui::OpenPopup("Group tools");}
                if(ImGui::BeginPopup("Group tools")) {
                    ImGui::TextUnformatted(group.groupName.c_str());AdvancedPalette::DrawGroup(group);
                    if(AdvancedPalette::OpenAdvanced){openAdvanced=true;AdvancedPalette::OpenAdvanced=false;}
                    ImGui::EndPopup();
                }
                ImGui::TableSetColumnIndex(3);
                if(ImGui::Button("Copy",ImVec2(copyWidth,0)))CopyGroupColors(group);
                ImGui::TableSetColumnIndex(4);ImGui::BeginDisabled(s_ClipboardColors.empty());
                const bool matches=s_ClipboardColors.size()==group.count;
                if(ImGui::Button(matches?"Paste###PasteGroup":"Paste matching###PasteGroup",ImVec2(pasteWidth,0)))PasteGroupColors(group);
                if(!matches && ImGui::IsItemHovered())ImGui::SetTooltip("Paste matching swatches: %u of %d",unsigned(std::min(s_ClipboardColors.size(),size_t(group.count))),group.count);
                ImGui::EndDisabled();
                const auto effects=AdvancedPalette::GroupEffects(group);
                if(effects!="No applied effects") {
                    ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ImGui::TextWrapped("%s",effects.c_str());
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();

	}
	else {
        ImGui::TextWrapped("Group labels are unavailable for this fighter. Bundled groups.json is loaded automatically; you can choose a custom file below.");
        if(ImGui::Button("Choose group labels")) {
            IGFD::FileDialogConfig cfg;cfg.path=".";
            ImGuiFileDialog::Instance()->OpenDialog("LoadJSON","Choose character group labels",".json",cfg);
        }
		// Автоматический расчет столбцов для негруппированного режима
		float availableWidth = ImGui::GetContentRegionAvail().x;
		float colorButtonSize = ImGui::GetFrameHeight();
		float itemSpacing = ImGui::GetStyle().ItemSpacing.x;
		int colorsPerRow = (int)(availableWidth / (colorButtonSize + itemSpacing));
		if (colorsPerRow < 1) colorsPerRow = 1;
		// Негруппированный режим
		int currentColumn = 0;

		for (int i = 1; i < colors.size(); i++) { // Начинаем с второго цвета
			DrawColorButton(i, colors[i]);

			// Решаем, добавлять SameLine или переходить на новую строку
			currentColumn++;
			if (currentColumn < colorsPerRow && i < colors.size() - 1) {
				ImGui::SameLine(0, itemSpacing);
			}
			else {
				currentColumn = 0;
			}
		}
	}
	
}

void Drawing::DrawSpritePreviewWindow() {
    if(!PreviewWindowOpen())return;
    ImGui::SetNextWindowSize(ImVec2(650,850),ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(460,540),ImVec2(FLT_MAX,FLT_MAX));
    ImGui::SetNextWindowBgAlpha(1.f);
    if(ImGui::Begin("Sprite preview",&PreviewWindowOpen())) {
        if(!MainThread::Match_Readed || PlayableCharactersManager::GetCurrentCharacterIndex()<0)ImGui::TextWrapped("Select a fighter in the palette editor.");
        else {
            auto& target=PlayableCharactersManager::GetCurrentCharacter();
            const auto* groups=GroupColorManager::GetInstance().GetGroupsForCharacter(target.Char_Name);
            if(groups && !groups->empty()) {
                selectedColorGroup=std::clamp(selectedColorGroup,0,int(groups->size())-1);
                const auto& group=(*groups)[selectedColorGroup];
                ImGui::TextWrapped("%s | Palette %d | %s",target.Char_Name,target.Current_Palette_Num+1,group.groupName.c_str());
                try {
                    auto layout=AdvancedPalette::PreviewLayout();std::set<int> bands;
                    int first=0;for(int band=0;band<layout.size();++band){if(first<group.startIndex+group.count && first+layout[band]>group.startIndex)bands.insert(band);first+=layout[band];}
                    SpriteViewer::Draw(PlayableCharactersManager::GetCurrentCharacterIndex(),target.Current_Palette_Num,target.Char_Name,target.Character_Colors,layout,group,bands,AdvancedPalette::PreviewRules());
                }catch(const std::exception& e){ImGui::TextWrapped("Preview unavailable: %s",e.what());}
            }else ImGui::TextWrapped("No color groups available for this fighter.");
        }
    }
    ImGui::End();
    if(!PreviewWindowOpen())config::set_bool("dll_preview_window",false);
}

void Drawing::DrawColorButton(int index, ImU32 colorU32) {
	ImGui::PushID(index);



	// Выбираем флаги в зависимости от режима
	ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoInputs |
		ImGuiColorEditFlags_NoLabel |
		ImGuiColorEditFlags_AlphaPreview |
		ImGuiColorEditFlags_AlphaBar;

	if (bUseColorPickerMode) {
		flags |= ImGuiColorEditFlags_NoPicker;

		// Делаем активную кнопку более заметной
		//if (m_ActiveColorIndex == index) {
		//	flags |= ImGuiColorEditFlags_Border;
		//}
	}

	// Конвертируем цвет
	ImVec4 colorVec = ColorsTools::ImU32BGRAtoImVec4RGBA(colorU32);
	if (bUseColorPickerMode) {
		ImGui::ColorButton(
			("##color_" + std::to_string(index)).c_str(),
			colorVec
		);

		if (m_ActiveColorIndex == index) {
			ImVec2 p_min = ImGui::GetItemRectMin();
			ImVec2 p_max = ImGui::GetItemRectMax();
			ImGui::GetWindowDrawList()->AddRect(p_min, p_max, ImGui::GetColorU32(ImVec4(1.0f, 0.0f, 0.0f, 1.0f)), 0.0f, 0, 2.0f);
		}

		if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
			m_ActiveColorIndex = index;
		}
	}
	else {
		if (bDrawSimpleColorEdit) {
			if (ImGui::ColorEdit4(
				("##color_" + std::to_string(index)).c_str(),
				&colorVec.x,
				flags
			)
				) {
				ProcessColorChange(index, colorVec);
			}
		}
		else {
			// Кнопка цвета
			if (ImGuiCustom::ColorEdit4(
				("##color_" + std::to_string(index)).c_str(),
				&colorVec.x,
				flags
			)
				) {
				ProcessColorChange(index, colorVec);
			}
		}
	}
	// В режиме Color Picker обрабатываем клик для выбора активного цвета

	ImGui::PopID();
}

void Drawing::ProcessColorChange(int index, const ImVec4& colorVec) {
	LOG_LOCAL_DEBUG(DrawingLogger, "Change color at index: ");
	LOG_LOCAL_VARIABLE(DrawingLogger, index);
	LOG_LOCAL_DEBUG(DrawingLogger, "Change color to: ");
	LOG_LOCAL_VARIABLE(DrawingLogger, colorVec.x);
	LOG_LOCAL_VARIABLE(DrawingLogger, colorVec.y);
	LOG_LOCAL_VARIABLE(DrawingLogger, colorVec.z);
	LOG_LOCAL_VARIABLE(DrawingLogger, colorVec.w);
	LOG_LOCAL_DEBUG(DrawingLogger, "Red, Green, Blue, Alpha");

	// Конвертируем обратно
	ImU32 ColorToWrite = ColorsTools::ImVec4RGBAtoImU32BGRA(colorVec);

	PlayableCharactersManager::ChangePaletteColor(index, ColorToWrite);
}

void Drawing::DrawAutoLoadPaletteTabItem() {
    if(!config::Status.empty())ImGui::TextWrapped("%s",config::Status.c_str());
    if(ImGuiFileDialog::Instance()->IsOpened()) {ImGui::TextUnformatted("Finish the file dialog to edit assignments.");return;}
    ImGui::TextWrapped("Load colors and applied shader settings once per palette, and again in each new match.");
    if(ImGui::Button("Add assignment")){AutoPalette::Auto_Pals.push_back({"Filia",0,""});AutoPalette::save();}
    ImGui::SameLine();if(ImGui::Button("Retry assignments"))AutoPalette::init();
    int remove=-1;
    for(int i=0;i<(int)AutoPalette::Auto_Pals.size();i++) {
        auto& pal=AutoPalette::Auto_Pals[i];ImGui::PushID(i);ImGui::Separator();
        if(ImGui::Checkbox("Enabled",&pal.Enabled))AutoPalette::save();
        ImGui::SetNextItemWidth(220);
        if(ImGui::BeginCombo("Character",pal.CharName.c_str())){
            for(auto name:PlayableCharacterNames)if(ImGui::Selectable(name,pal.CharName==name)){pal.CharName=name;AutoPalette::save();}
            ImGui::EndCombo();
        }
        int number=pal.PalNum+1;ImGui::SetNextItemWidth(180);
        if(ImGui::InputInt("##Palette number",&number)){pal.PalNum=std::clamp(number,1,256)-1;AutoPalette::save();}
        char path[32769]{};strncpy_s(path,pal.PalPath.c_str(),_TRUNCATE);
        ImGui::SetNextItemWidth(std::max(150.0f,ImGui::GetContentRegionAvail().x-95));
        if(ImGui::InputText("##path",path,sizeof(path)))pal.PalPath=path;
        if(ImGui::IsItemDeactivatedAfterEdit())AutoPalette::save();
        ImGui::SameLine();if(ImGui::Button("Browse")){
            IGFD::FileDialogConfig cfg;cfg.path=".";cfg.userDatas=reinterpret_cast<void*>(&pal);
            ImGuiFileDialog::Instance()->OpenDialog("LoadPaletteFile_AutoLoad","Choose auto-load palette",".pal",cfg);
        }
        ImGui::TextWrapped("%s",pal.Status.c_str());
        if(ImGui::Button("Remove assignment"))remove=i;
        ImGui::PopID();
    }
    if(remove>=0){AutoPalette::Auto_Pals.erase(AutoPalette::Auto_Pals.begin()+remove);AutoPalette::save();}
}

void Drawing::DrawOptionsTabItem()
{
	ImGui::Text("Options: ");
	ImGui::SeparatorText("Drawing: ");
	//We need to create "intermediate" bool
	bool Drawing_NODisplayChar = MainThread::b_NODisplayChar.load();
	if (ImGui::Checkbox("Don't display characters", &Drawing_NODisplayChar)) {
		MainThread::b_NODisplayChar.store(Drawing_NODisplayChar); //And now, we save this in our atomic bool
	}
	bool Drawing_NODisplayShadows = MainThread::b_NODisplayShadows.load();
	if (ImGui::Checkbox("Don't display shadows", &Drawing_NODisplayShadows)) {
		MainThread::b_NODisplayShadows.store(Drawing_NODisplayShadows); //And now, we save this in our atomic bool
	}
	bool Drawing_DisplaySuperShadows = MainThread::b_DisplaySuperShadows.load();
	if (ImGui::Checkbox("Display super shadow", &Drawing_DisplaySuperShadows)) {
		MainThread::b_DisplaySuperShadows.store(Drawing_DisplaySuperShadows); //And now, we save this in our atomic bool
	}
	ImGui::Checkbox("Simple Color Edit", &bDrawSimpleColorEdit);
    ImGui::SeparatorText("Online palette sharing");
    ImGui::TextWrapped("Disabled in this build. Palettes and shader settings stay local.");

}


void Drawing::DrawFileDialog() {

	ImGui::SetNextWindowSize(vFileDialogSize, ImGuiCond_Once);
    PollSpritePreparation();
    SpriteViewer::PollAnimationExport();

    if(ImGuiFileDialog::Instance()->Display("ExportSpriteAnimation")) {
        if(ImGuiFileDialog::Instance()->IsOk()) {
            try {SpriteViewer::StartAnimationExport(PathFromUtf8(ImGuiFileDialog::Instance()->GetFilePathName()));}
            catch(const std::exception& e){SpriteViewer::animationStatus=std::string("Animation export failed: ")+e.what();}
        }else {SpriteViewer::pendingAnimationExport.reset();SpriteViewer::animationStatus="Animation export cancelled.";}
        ImGuiFileDialog::Instance()->Close();
    }

    if(ImGuiFileDialog::Instance()->Display("ExportSpritePNG")) {
        if(ImGuiFileDialog::Instance()->IsOk())SpriteViewer::SavePngExport(PathFromUtf8(ImGuiFileDialog::Instance()->GetFilePathName()));
        else {SpriteViewer::pendingPngExport.reset();SpriteViewer::exportStatus="PNG export cancelled.";}
        ImGuiFileDialog::Instance()->Close();
    }

    if(ImGuiFileDialog::Instance()->Display("ChoosePreviewPython")) {
        if(ImGuiFileDialog::Instance()->IsOk())SpriteViewer::preparationPython=PathFromUtf8(ImGuiFileDialog::Instance()->GetFilePathName());
        ImGuiFileDialog::Instance()->Close();
    }

    if(ImGuiFileDialog::Instance()->Display("PrepareSpritePreview")) {
        if(ImGuiFileDialog::Instance()->IsOk()) {
            const auto selected=PathFromUtf8(ImGuiFileDialog::Instance()->GetFilePathName());
            StartSpritePreparation(selected);
        }
        ImGuiFileDialog::Instance()->Close();
    }

	if (ImGuiFileDialog::Instance()->Display("SavePaletteFile")) {

		if (ImGuiFileDialog::Instance()->IsOk()) { // action if OK

			std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();

			if(AdvancedPalette::Save(filePathName,saveLegacy) && saveAndAssign) {
                auto& c=PlayableCharactersManager::GetCurrentCharacter();bool found=false;
                for(auto& entry:AutoPalette::Auto_Pals)if(entry.CharName==c.Char_Name && entry.PalNum==c.Current_Palette_Num){entry.PalPath=filePathName;entry.Enabled=true;found=true;break;}
                if(!found)AutoPalette::Auto_Pals.push_back({c.Char_Name,c.Current_Palette_Num,filePathName});
                AutoPalette::save();
            }
		}

		// close
		ImGuiFileDialog::Instance()->Close();
	}

	if (ImGuiFileDialog::Instance()->Display("LoadPaletteFile")) {

		if (ImGuiFileDialog::Instance()->IsOk()) { // action if OK

			std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();

			PalleteFile::LoadFromFile(filePathName);
		}

		// close
		ImGuiFileDialog::Instance()->Close();
	}

	if (ImGuiFileDialog::Instance()->Display("LoadJSON")) {

		if (ImGuiFileDialog::Instance()->IsOk()) { // action if OK

			std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();

			GroupColorManager::GetInstance().LoadFromFile(filePathName);
			config::set_string("CharPart", filePathName);
		}

		// close
		ImGuiFileDialog::Instance()->Close();
	}

	if (ImGuiFileDialog::Instance()->Display("LoadPaletteFile_AutoLoad")) {

		if (ImGuiFileDialog::Instance()->IsOk()) { // action if OK

			std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
			Auto_Pal* palPtr = reinterpret_cast<Auto_Pal*>(ImGuiFileDialog::Instance()->GetUserDatas());
			palPtr->PalPath = filePathName;

			AutoPalette::save();
		}

		// close
		ImGuiFileDialog::Instance()->Close();
	}
}

void Drawing::DrawColorPickerWindow() {
	if (!bUseColorPickerMode) return;
	auto& Curent_Char = PlayableCharactersManager::GetCurrentCharacter();
	auto& colors = Curent_Char.Character_Colors;
	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;

	ImGui::SetNextWindowSize(ImVec2(750, 550), ImGuiCond_Once);
	if (ImGui::Begin("Color Picker", &bUseColorPickerMode, ImGuiWindowFlags_NoCollapse)) {

		// Отображаем информацию о текущем цвете и группе
		ImGui::Text("Color Index: #%d", m_ActiveColorIndex);

		// Определяем, в какой группе находится активный цвет
		std::string currentGroup = "Ungrouped";
		if (auto groupsPtr = GroupColorManager::GetInstance().GetGroupsForCharacter(Curent_Char.Char_Name)) {
			const auto& groups = *groupsPtr;
			for (const auto& group : groups) {
				if (m_ActiveColorIndex >= group.startIndex &&
					m_ActiveColorIndex < group.startIndex + group.count) {
					currentGroup = group.groupName;
					break;
				}
			}
		}
		ImGui::Text("Group: %s", currentGroup.c_str());

		ImGui::Separator();

		// Получаем текущий цвет
		ImU32 colorRGBA_U32 = ColorsTools::SwapRBChannels(colors[m_ActiveColorIndex]);
		ImVec4 colorVec = ImGui::ColorConvertU32ToFloat4(colorRGBA_U32);

		// Используем временный буфер для ref цвета
		static int lastActiveIndex = -1;
		static ImVec4 refColorVec;

		// Если индекс изменился или окно только открылось, обновляем ref цвет
		if (lastActiveIndex != m_ActiveColorIndex) {
			refColorVec = colorVec;
			lastActiveIndex = m_ActiveColorIndex;
		}

		// Color Picker с отображением оригинального цвета
		ImGui::BeginGroup();
		bool colorChanged = ImGuiCustom::ColorPicker4("##color_picker", &colorVec.x,
			ImGuiColorEditFlags_NoLabel |
			ImGuiColorEditFlags_AlphaPreview |
			ImGuiColorEditFlags_AlphaBar,
			&refColorVec.x);
		ImGui::EndGroup();
		//if (ImGui::Button("Eyedropper")) {
		//	EyeDropper::getInstance().StartEyedropper(&colorVec.x);
		//}
		ImGui::Separator();
		ImGui::BeginGroup();
		ImGui::Text("Palette");
		for (int n = 0; n < IM_ARRAYSIZE(ImGuiCustom::saved_palette); n++)
		{
			ImGui::PushID(n);
			if ((n % 8) != 0)
				ImGui::SameLine(0.0f, style.ItemSpacing.y);

			ImGuiColorEditFlags palette_button_flags = ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoTooltip;
			if (ImGui::ColorButton("##palette", ImGuiCustom::saved_palette[n], palette_button_flags, ImVec2(20, 20)))
			{
				colorVec.x = ImGuiCustom::saved_palette[n].x;
				colorVec.y = ImGuiCustom::saved_palette[n].y;
				colorVec.z = ImGuiCustom::saved_palette[n].z;
				// Preserve alpha
				colorChanged = true;
			}

			// Allow user to drop colors into each palette entry
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_3F))
					memcpy((float*)&ImGuiCustom::saved_palette[n], payload->Data, sizeof(float) * 3);
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_4F))
					memcpy((float*)&ImGuiCustom::saved_palette[n], payload->Data, sizeof(float) * 4);
				ImGui::EndDragDropTarget();
			}
			ImGui::PopID();
		}
		ImGui::EndGroup();

		// Проверяем, завершилась ли пипетка (и не была отменена)
		if (EyeDropper::getInstance().IsThreadFinished()) {
			if (EyeDropper::getInstance().WasCancelled()) {
				// Пипетка была отменена - возвращаем предыдущий цвет
				colorVec.x = refColorVec.x;
				colorVec.y = refColorVec.y;
				colorVec.z = refColorVec.z;
				colorVec.w = refColorVec.w;
				colorChanged = true;
			}
		}

		// Во время работы пипетки показываем предпросмотр
		if (!EyeDropper::getInstance().IsThreadFinished()) {
			ImVec4 EyeDropperColor = EyeDropper::getInstance().GetColorUnderCursorImVec4();
			colorVec.x = EyeDropperColor.x;
			colorVec.y = EyeDropperColor.y;
			colorVec.z = EyeDropperColor.z;
			colorChanged = true;
		}
		if (colorChanged) {
			ProcessColorChange(m_ActiveColorIndex, colorVec);
		}

		// Отображаем информацию о цветах
		ImGui::Separator();
		ImGui::Text("Navigation:");

		if (ImGui::ArrowButton("##prev", ImGuiDir_Left) && m_ActiveColorIndex > 1) {
			m_ActiveColorIndex--;
		}
		ImGui::SameLine();
		ImGui::Text("Index: #%d", m_ActiveColorIndex);
		ImGui::SameLine();
		if (ImGui::ArrowButton("##next", ImGuiDir_Right) && m_ActiveColorIndex < colors.size() - 1) {
			m_ActiveColorIndex++;
		}	
	}
	ImGui::End();
}

//===== Utils ======
std::string ColorsToText(const std::vector<ImU32>& colors) {
	std::string result;

	for (size_t i = 0; i < colors.size(); i++) {
		// Преобразуем ImU32 в RGBA компоненты
		ImU32 colorRGBA = ColorsTools::SwapRBChannels(colors[i]);
		ImVec4 colorVec = ImGui::ColorConvertU32ToFloat4(colorRGBA);

		// Формируем строку в формате: R G B A
		char buffer[64];
		snprintf(buffer, sizeof(buffer), "%d %d %d %d",
			(int)((colorRGBA >> IM_COL32_R_SHIFT) & 0xFF),  // Красный
			(int)((colorRGBA >> IM_COL32_G_SHIFT) & 0xFF),  // Зеленый
			(int)((colorRGBA >> IM_COL32_B_SHIFT) & 0xFF),  // Синий
			(int)((colorRGBA >> IM_COL32_A_SHIFT) & 0xFF)); // Альфа

		result += buffer;
		if (i < colors.size() - 1) {
			result += "\n";
		}
	}

	return result;
}

void Drawing::CopyGroupColors(const ColorGroup& group) {
	auto& colors = PlayableCharactersManager::GetCurrentCharacter().Character_Colors;

	// Очищаем буфер обмена
	s_ClipboardColors.clear();
    copiedGroup=group.groupName;

	// Копируем цвета группы в буфер обмена
	for (int i = group.startIndex;
		i < group.startIndex + group.count && i < colors.size();
		i++) {
		s_ClipboardColors.push_back(colors[i]);
	}

	// Также копируем в системный буфер обмена в текстовом формате
	std::string clipboardText = ColorsToText(s_ClipboardColors);
	ImGui::SetClipboardText(clipboardText.c_str());

	LOG_LOCAL_DEBUG(DrawingLogger, "Copied group '");
	LOG_LOCAL_VARIABLE(DrawingLogger, group.groupName);
	LOG_LOCAL_DEBUG(DrawingLogger, "' to clipboard. Colors count: ");
	LOG_LOCAL_VARIABLE(DrawingLogger, s_ClipboardColors.size());
}

void Drawing::PasteGroupColors(const ColorGroup& group) {
	auto& colors = PlayableCharactersManager::GetCurrentCharacter().Character_Colors;

	// Проверяем, есть ли что-то в буфере обмена
	if (s_ClipboardColors.empty()) {
		LOG_LOCAL_WARN(DrawingLogger, "Clipboard is empty. Cannot paste.");
		return;
	}

	// Проверяем, что количество цветов совпадает
	if (s_ClipboardColors.size() != group.count) {
		LOG_LOCAL_WARN(DrawingLogger, "Clipboard colors count (");
		LOG_LOCAL_VARIABLE(DrawingLogger, s_ClipboardColors.size());
		LOG_LOCAL_DEBUG(DrawingLogger, ") doesn't match group count (");
		LOG_LOCAL_VARIABLE(DrawingLogger, group.count);
		LOG_LOCAL_DEBUG(DrawingLogger, "). Will paste as many as possible.");
	}

	// Вставляем цвета из буфера обмена
	int pasteCount = std::min((int)s_ClipboardColors.size(), group.count);

    PasteUndo undo{PlayableCharactersManager::GetCurrentCharacterIndex(),PlayableCharactersManager::GetCurrentCharacter().Current_Palette_Num,group.startIndex,{},{}};
    for(int i=0;i<pasteCount && group.startIndex+i<colors.size();++i){undo.before.push_back(colors[group.startIndex+i]);undo.after.push_back(s_ClipboardColors[i]);}
    undo.generation=AdvancedPalette::MatchGeneration;
    if(!undo.before.empty())pasteUndo.push_back(undo);
	for (int i = 0; i < pasteCount; i++) {
		int colorIndex = group.startIndex + i;
		if (colorIndex >= colors.size()) break;

		PlayableCharactersManager::ChangePaletteColor(colorIndex, s_ClipboardColors[i]);
	}

	LOG_LOCAL_DEBUG(DrawingLogger, "Pasted ");
	LOG_LOCAL_VARIABLE(DrawingLogger, pasteCount);
	LOG_LOCAL_DEBUG(DrawingLogger, " colors to group '");
	LOG_LOCAL_VARIABLE(DrawingLogger, group.groupName);
	LOG_LOCAL_DEBUG(DrawingLogger, "'");
}



//===== DEV WINDOW =======

void Drawing::Developer::DrawDevWindow() {
	if (!bDrawDevWindow) return;
	ImGui::Begin("Developer Window", &bDrawDevWindow);
	ImGui::Text("You want find something here?");
	static ImVec4 Test_Color;
	ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoInputs |
		ImGuiColorEditFlags_NoLabel |
		ImGuiColorEditFlags_AlphaPreview |
		ImGuiColorEditFlags_AlphaBar 
		// | ImGuiColorEditFlags_Float
		;
	ImGuiCustom::ColorEdit4("Test!", &Test_Color.x, flags);
	if(ImGui::Button("Eyedropper!")) {
		EyeDropper::getInstance().StartEyedropper(&Test_Color.x);
	}
	if(ImGui::Checkbox("Show console", &bDrawConsole)) {
		SetConsoleMode(bDrawConsole);
	}

	ImGui::Checkbox("Show Demo Window", &ShowDemoWindow);

	ImGui::Checkbox("Show Logs", &ShowLogs);





	ImGui::End();
}

void Drawing::DrawPaletteWideColors() {
    auto& target=PlayableCharactersManager::GetCurrentCharacter();
    if(ImGui::BeginTable("Line and shadow colors",3,ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow();
        ImU32* values[]={&target.LineColor,&target.SuperShadowColor1,&target.SuperShadowColor2};
        const char* labels[]={"Line","Shadow 1","Shadow 2"};
        const ColorOptionFlag flags[]={FLAG_LINE_COLOR,FLAG_SUPER_SHADOW_1,FLAG_SUPER_SHADOW_2};
        for(int i=0;i<3;++i) {
            ImGui::TableSetColumnIndex(i);auto color=ColorsTools::ImU32BGRAtoImVec4RGBA(*values[i]);
            if(ImGui::ColorEdit3(labels[i],&color.x,ImGuiColorEditFlags_NoInputs)) {
                auto value=ColorsTools::ImVec4RGBAtoImU32BGRA(color);
                PlayableCharactersManager::ChangeOptionPaletteColor(value,flags[i]);
            }
        }
        ImGui::EndTable();
    }
}
