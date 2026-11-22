#include "PalleteEditor.h"
#include "Memory.h"
#include "FileLoad.h"
#include "Auto-Load-Pallete.h"
#include "OverlayEditor.h"
#include "OverlayControls.h"

void PalEdit::DrawAdvanced() {
    int index=FindVectorIndexByID(current_character_idx);
    if(index<0)return;
    auto& character=Character_Vector[index];
    OverlayEditor::Draw(s_SG_Process,s_BaseAddress,character);
    try {
        OverlayControls::Draw(character,OverlayEditor::status,false,false,[&](PaletteData::Document document) {
            OverlayEditor::ApplyPalette(character,document,true);
            OverlayEditor::status=document.secondOverlay?"Previewing the primary texture only. Save Palette keeps both slots for the in-game DLL.":"Overlay applied.";
        });
    }catch(const std::exception& error) {
        ImGui::TextWrapped("%s",error.what());
    }
}
void PalEdit::CloseOverlay() { OverlayEditor::Shutdown(); OverlayControls::overlayDrafts.clear(); }

#define GAME_STATUS_MATCH_STARTED 0x4

namespace PatchStuff {
    std::vector<unsigned char> CodeCave = { //"Skullgirls.exe" + 338A50
    0x60, 0x8B, 0x81, 0x30, 0x03, 0x00, 0x00, 0x8B, 0x88, 0x28, 0x03, 0x00, 0x00, 0x05, 0x30, 0x03,
    0x00, 0x00, 0x89, 0xC2, 0x8B, 0x45, 0xA4, 0x6A, 0x00, 0x8B, 0x7D, 0x80, 0x6B, 0xFF, 0x56, 0x01,
    0xF8, 0x05, 0xEC, 0x06, 0x00, 0x00, 0x50, 0x8B, 0x02, 0x50, 0xE8, 0x71, 0xE9, 0xD4, 0xFF, 0x61,
    0x60, 0x8B, 0x81, 0x30, 0x03, 0x00, 0x00, 0x8B, 0x88, 0x28, 0x03, 0x00, 0x00, 0x05, 0x30, 0x03,
    0x00, 0x00, 0x89, 0xC2, 0x8B, 0x45, 0xA4, 0x6A, 0x01, 0x8B, 0x7D, 0x80, 0x6B, 0xFF, 0x56, 0x01,
    0xF8, 0x05, 0x54, 0x10, 0x00, 0x00, 0x50, 0x8B, 0x02, 0x50, 0xE8, 0x41, 0xE9, 0xD4, 0xFF, 0x61,
    0xE9, 0x69, 0x88, 0xE5, 0xFF
    };
    std::vector<unsigned char> JmpToCodeCave = { //"Skullgirls.exe" + 191313
        0xE9, 0x38, 0x77, 0x1A, 0x00 };
};

int PalEdit::FindVectorIndexByID(int id) {
    for (int i = 0; i < PalEdit::Character_Vector.size(); i++) {
        if (PalEdit::Character_Vector[i].ID == id) {
            return i;
        }
    }
    return -1;
}

void PalEdit::Init() {
    Sleep(10);
    s_ProcessId = Memory::FindProcessId(L"Skullgirls.exe");
    if (s_ProcessId == NULL){
        CloseOverlay();
        if(s_SG_Process) CloseHandle(s_SG_Process);
        s_SG_Process=nullptr; s_matchIdentity.clear(); Character_Vector.clear(); current_character_idx=-1;
        AutoPallete::ResetSession(); bMatchStarted=false; bGameOpenned=false;
        return;
    }
    if (s_ProcessId != NULL && (!s_SG_Process || GetProcessId(s_SG_Process)!=s_ProcessId)) {
        CloseOverlay();
        if(s_SG_Process) CloseHandle(s_SG_Process);
        s_matchIdentity.clear(); Character_Vector.clear(); current_character_idx=-1; AutoPallete::ResetSession();
        s_BaseAddress = Memory::GetModuleBaseAddress(s_ProcessId, L"Skullgirls.exe");
        s_SG_Process = OpenProcess(PROCESS_ALL_ACCESS, TRUE, s_ProcessId);
        if(!s_SG_Process) { bGameOpenned=false; return; }
        try { OverlayEditor::Bind(s_SG_Process,s_BaseAddress); }
        catch(const std::exception& error) { PalleteFile::Status=error.what(); CloseHandle(s_SG_Process); s_SG_Process=nullptr; bGameOpenned=false; return; }
        //Patch game;
        WriteProcessMemory(s_SG_Process, (LPVOID)(s_BaseAddress + 0x338A50), PatchStuff::CodeCave.data(), PatchStuff::CodeCave.size(), nullptr);
        WriteProcessMemory(s_SG_Process, (LPVOID)(s_BaseAddress + 0x191313), PatchStuff::JmpToCodeCave.data(), PatchStuff::JmpToCodeCave.size(), nullptr);
        bGameOpenned = true;
    }
    try { OverlayEditor::Tick(); } catch(...) {}
    Memory::ReadProcessMemoryWithOffsets(
        s_SG_Process,
        s_BaseAddress, {
        static_cast<uintptr_t>(AddressTable::Base_Adress()),
        static_cast<uintptr_t>(AddressTable::Offset_GameStatus())  // Приведение к нужному типу
        },
        &s_GameStatus);

    if (s_GameStatus != GAME_STATUS_MATCH_STARTED) {
        OverlayEditor::ForgetMatch(); OverlayControls::overlayDrafts.clear(); s_matchIdentity.clear(); AutoPallete::ResetSession();
        current_character_idx = -1;
        Character_Vector.clear();
        bMatchStarted = false;
        return;
    }
    bMatchStarted = true;

    try {
        DWORD root=OverlayEditor::Word(s_BaseAddress+0x85ea4c);
        std::vector<DWORD> identity{root};
        for(int slot=0;slot<6;slot++) {
            DWORD actor=OverlayEditor::Word(root+0x5e8+4*slot);
            identity.push_back(actor); identity.push_back(actor?OverlayEditor::Word(actor+0x328):0);
        }
        if(identity!=s_matchIdentity) {
            OverlayEditor::ForgetMatch(); OverlayControls::overlayDrafts.clear(); Character_Vector.clear(); current_character_idx=-1;
            AutoPallete::ResetSession(); s_matchIdentity=identity;
        }
        if(!Character_Vector.empty()) {
            for(auto& character:Character_Vector) {
                int palette=OverlayEditor::Read(OverlayEditor::Word(root+0x5e8+4*character.ID)+0x330,1)[0];
                if(palette!=character.Current_Pallete_Num) {
                    character.Current_Pallete_Num=palette;
                    if(character.ID==current_character_idx) Read_Character();
                }
            }
            AutoPallete::Tick(); return;
        }
    } catch(const std::exception& error) {
        PalleteFile::Status=error.what(); bMatchStarted=false; Character_Vector.clear(); current_character_idx=-1;
        AutoPallete::ResetSession(); return;
    }

    bNODisplayChar = false;
    bNODisplayShadows = false;
    bDisplaySuperShadows = false;

    Character_Vector.clear();
    for (int n{ 0 }; n < 6; n++) {
        std::string Name;
        Memory::ReadProcessMemoryWithOffsets(
            s_SG_Process,
            s_BaseAddress, {
            static_cast<uintptr_t>(AddressTable::Base_Adress()),
            static_cast<uintptr_t>(AddressTable::Offset_Character() + n * 4),  // Приведение к нужному типу
            static_cast<uintptr_t>(AddressTable::Offset_Name()) },
            &Name
            );
        if (Name != "") {
            Character Ch;
            Ch.Char_Name = Name;
            Ch.ID = n;
            Memory::ReadProcessMemoryWithOffsets(
                s_SG_Process,
                s_BaseAddress, {
                static_cast<uintptr_t>(AddressTable::Base_Adress()),
                static_cast<uintptr_t>(AddressTable::Offset_Character() + n * 4),  // Приведение к нужному типу
                static_cast<uintptr_t>(AddressTable::Offset_PaletteData()),
                static_cast<uintptr_t>(AddressTable::Offset_PaletteTotalOffset()),
                },
                &Ch.Max_Pallete_Num
                );
            Memory::ReadProcessMemoryWithOffsets(
                s_SG_Process,
                s_BaseAddress, {
                static_cast<uintptr_t>(AddressTable::Base_Adress()),
                static_cast<uintptr_t>(AddressTable::Offset_Character() + n * 4),  // Приведение к нужному типу
                static_cast<uintptr_t>(AddressTable::Offset_CurrentPalette()),
                },
                &Ch.Current_Pallete_Num
                );
            Memory::ReadProcessMemoryWithOffsets(
                s_SG_Process,
                s_BaseAddress, {
                static_cast<uintptr_t>(AddressTable::Base_Adress()),
                static_cast<uintptr_t>(AddressTable::Offset_Character() + n * 4),  // Приведение к нужному типу
                static_cast<uintptr_t>(AddressTable::Offset_PaletteData()),
                static_cast<uintptr_t>(AddressTable::Offset_NumberOfColor()),
                },
                &Ch.Num_Of_Color
                );
            Memory::ReadProcessMemoryWithOffsets(
                s_SG_Process,
                s_BaseAddress, {
                static_cast<uintptr_t>(AddressTable::Base_Adress()),
                static_cast<uintptr_t>(AddressTable::Offset_Character() + n * 4),  // Приведение к нужному типу
                static_cast<uintptr_t>(AddressTable::Offset_PaletteData()),
                static_cast<uintptr_t>(AddressTable::NEW_Offset_LineColor()),
                static_cast<uintptr_t>(4 * Ch.Current_Pallete_Num),
                },
                &Ch.LineColor
                );
            //SuperShadows1
            Memory::ReadProcessMemoryWithOffsets(
                s_SG_Process,
                s_BaseAddress, {
                static_cast<uintptr_t>(AddressTable::Base_Adress()),
                static_cast<uintptr_t>(AddressTable::Offset_Character() + n * 4),  // Приведение к нужному типу
                static_cast<uintptr_t>(AddressTable::Offset_PaletteData()),
                static_cast<uintptr_t>(AddressTable::NEW_Offset_SuperShadow()),
                static_cast<uintptr_t>(4 * Ch.Current_Pallete_Num),
                0
                },
                &Ch.SuperShadowColor1
            );
            Memory::ReadProcessMemoryWithOffsets(
                s_SG_Process,
                s_BaseAddress, {
                static_cast<uintptr_t>(AddressTable::Base_Adress()),
                static_cast<uintptr_t>(AddressTable::Offset_Character() + n * 4),  // Приведение к нужному типу
                static_cast<uintptr_t>(AddressTable::Offset_PaletteData()),
                static_cast<uintptr_t>(AddressTable::NEW_Offset_SuperShadow()),
                static_cast<uintptr_t>(4 * Ch.Current_Pallete_Num),
                4
                },
                &Ch.SuperShadowColor2
            );
            Character_Vector.push_back(Ch);
            std::cout << Name;
        }
    }

    AutoPallete::init();
}

void PalEdit::Read_Character() {
    int index=FindVectorIndexByID(current_character_idx);
    if(index<0) return;
    try { OverlayEditor::SyncPaletteModel(Character_Vector[index]); }
    catch(const std::exception& error) { PalleteFile::Status=error.what(); }
}

void PalEdit::ChangePallete() {
    int VectorID = FindVectorIndexByID(current_character_idx);
    unsigned __int8 New_Pal = static_cast<unsigned __int8>(Character_Vector[VectorID].Current_Pallete_Num);
    Memory::WriteProcessMemoryWithOffsets(
        s_SG_Process,
        s_BaseAddress, {
        static_cast<uintptr_t>(AddressTable::Base_Adress()),
        static_cast<uintptr_t>(AddressTable::Offset_Character() + current_character_idx * 4),  // Приведение к нужному типу
        static_cast<uintptr_t>(AddressTable::Offset_CurrentPalette()),
        },
        New_Pal
        );
    // Palette colors and shader-feature masks have separate GPU caches.
    try { OverlayEditor::Refresh(); } catch(...) {}
}

void PalEdit::ChangeColor(int Color_ID, __int32 colorValue) {
    int VectorID = FindVectorIndexByID(current_character_idx);
    Memory::WriteProcessMemoryWithOffsets(
        s_SG_Process,
        s_BaseAddress, {
        static_cast<uintptr_t>(AddressTable::Base_Adress()),
        static_cast<uintptr_t>(AddressTable::Offset_Character() + current_character_idx * 4),  // Приведение к нужному типу
        static_cast<uintptr_t>(AddressTable::Offset_PaletteData()),
        static_cast<uintptr_t>(AddressTable::Offset_ColorCodeOffset()),
        static_cast<uintptr_t>(4 * Character_Vector[VectorID].Current_Pallete_Num),
        static_cast<uintptr_t>(4 * Color_ID)
        },
        colorValue
        );
}

void PalEdit::ChangeAllColors() {
    Character currentChar = PalEdit::Character_Vector[PalEdit::FindVectorIndexByID(PalEdit::current_character_idx)];
    for (int i = 0; i < currentChar.Character_Colors.size(); i++) {
        ChangeColor(i, currentChar.Character_Colors[i]);
    }
}

void PalEdit::NODisplayChar() {
    static char MEM[] = {0x77, 0x20};
    if (bNODisplayChar) {
        
        const char NOPE[] = { 0x90, 0x90 };
        Memory::WriteProcessMemoryWithOffsets(
            s_SG_Process,
            s_BaseAddress, {
            static_cast<uintptr_t>(AddressTable::NEW_Base_Adress_DonotdisplayCHAR())  // Приведение к нужному типу
            },
            NOPE);

    }
    else {
        Memory::WriteProcessMemoryWithOffsets(
            s_SG_Process,
            s_BaseAddress, {
            static_cast<uintptr_t>(AddressTable::NEW_Base_Adress_DonotdisplayCHAR())  // Приведение к нужному типу
            },
            MEM);
    }
}

void PalEdit::NODisplayShadow() {
    static char MEM[] = {0x75, 0x20};
    if (bNODisplayShadows) {

        const char NOPE[] = { 0xEB, 0x1D };
        Memory::WriteProcessMemoryWithOffsets(
            s_SG_Process,
            s_BaseAddress, {
            static_cast<uintptr_t>(AddressTable::NEW_Base_Adress_DonotdisplaySHADOWS())  // Приведение к нужному типу
            },
            NOPE);

    }
    else {
        Memory::WriteProcessMemoryWithOffsets(
            s_SG_Process,
            s_BaseAddress, {
            static_cast<uintptr_t>(AddressTable::NEW_Base_Adress_DonotdisplaySHADOWS())  // Приведение к нужному типу
            },
            MEM);
    }
}

void PalEdit::DisplaySuperShadow() {
    static char MEM[6] = { 0x0F, 0x8B,0x22, 0x01, 0x00, 0x00 };
    if (bDisplaySuperShadows) {

        const char NOPE[] = { 0x90, 0x90,0x90,0x90,0x90,0x90, };
        Memory::WriteProcessMemoryWithOffsets(
            s_SG_Process,
            s_BaseAddress, {
            static_cast<uintptr_t>(AddressTable::NEW_Base_Adress_Display_SuperShadowforever())  // Приведение к нужному типу
            },
            NOPE);

    }
    else {
        Memory::WriteProcessMemoryWithOffsets(
            s_SG_Process,
            s_BaseAddress, {
            static_cast<uintptr_t>(AddressTable::NEW_Base_Adress_Display_SuperShadowforever())  // Приведение к нужному типу
            },
            MEM);
    }
}

void PalEdit::ChangeLineColor() {
    int VectorID = FindVectorIndexByID(current_character_idx);
    Memory::WriteProcessMemoryWithOffsets(
        s_SG_Process,
        s_BaseAddress, {
        static_cast<uintptr_t>(AddressTable::Base_Adress()),
        static_cast<uintptr_t>(AddressTable::Offset_Character() + current_character_idx * 4),  // Приведение к нужному типу
        static_cast<uintptr_t>(AddressTable::Offset_PaletteData()),
        static_cast<uintptr_t>(AddressTable::NEW_Offset_LineColor()),
        static_cast<uintptr_t>(4 * Character_Vector[VectorID].Current_Pallete_Num),
        },
        Character_Vector[VectorID].LineColor
        );
}
void PalEdit::ChangeSuperShadow1() {
    int VectorID = FindVectorIndexByID(current_character_idx);
    Memory::WriteProcessMemoryWithOffsets(
        s_SG_Process,
        s_BaseAddress, {
        static_cast<uintptr_t>(AddressTable::Base_Adress()),
        static_cast<uintptr_t>(AddressTable::Offset_Character() + current_character_idx * 4),  // Приведение к нужному типу
        static_cast<uintptr_t>(AddressTable::Offset_PaletteData()),
        static_cast<uintptr_t>(AddressTable::NEW_Offset_SuperShadow()),
        static_cast<uintptr_t>(4 * Character_Vector[VectorID].Current_Pallete_Num),
        0
        },
        Character_Vector[VectorID].SuperShadowColor1
    );
}
void PalEdit::ChangeSuperShadow2() {
    int VectorID = FindVectorIndexByID(current_character_idx);
    Memory::WriteProcessMemoryWithOffsets(
        s_SG_Process,
        s_BaseAddress, {
        static_cast<uintptr_t>(AddressTable::Base_Adress()),
        static_cast<uintptr_t>(AddressTable::Offset_Character() + current_character_idx * 4),  // Приведение к нужному типу
        static_cast<uintptr_t>(AddressTable::Offset_PaletteData()),
        static_cast<uintptr_t>(AddressTable::NEW_Offset_SuperShadow()),
        static_cast<uintptr_t>(4 * Character_Vector[VectorID].Current_Pallete_Num),
        4
        },
        Character_Vector[VectorID].SuperShadowColor2
    );
}

void PalEdit::UpdateAllCharacters() {
    for (Character currentChar : PalEdit::Character_Vector) {
        current_character_idx = currentChar.ID;
        PalEdit::ChangeAllColors();
        PalEdit::ChangeLineColor();
        PalEdit::ChangeSuperShadow1();
        PalEdit::ChangeSuperShadow2();
        PalEdit::Read_Character();
    }
    current_character_idx = -1;
}



