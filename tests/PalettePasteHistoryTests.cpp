#include "../DllEditor/PalleteEditor/UI/PalettePasteHistory.h"
#include <stdexcept>
#include <iostream>
void Check(bool ok){if(!ok)throw std::runtime_error("Paste undo identity/bounds check failed");}
int main(){
 PalettePasteHistory::Entry entry{0,1,2,{0x11,0x22},{0x33,0x44},5};
 std::vector<uint32_t> colors={0,1,0x33,0x44};
 Check(entry.CanUndo(0,1,5,colors));
 Check(!entry.CanUndo(1,1,5,colors));Check(!entry.CanUndo(0,2,5,colors));
 // A new fighter can reuse both the palette and its colors; identity still differs.
 Check(!entry.CanUndo(0,1,6,colors));
 colors[3]=0x55;Check(!entry.CanUndo(0,1,5,colors));
 colors.resize(3);Check(!entry.CanUndo(0,1,5,colors));
 entry.start=-1;Check(!entry.CanUndo(0,1,5,colors));
 entry.start=99;Check(!entry.CanUndo(0,1,5,colors));
 entry.start=2;colors={0,1,0x33,0x44};entry.before.pop_back();Check(!entry.CanUndo(0,1,5,colors));
 entry.before.clear();entry.after.clear();Check(!entry.CanUndo(0,1,5,colors));
 std::cout<<"Paste undo tests passed: matching state, changed state, slot/palette/generation isolation and truncated bounds.\n";
}
