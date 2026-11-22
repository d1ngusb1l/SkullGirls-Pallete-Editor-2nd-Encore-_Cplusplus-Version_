#include "../DllEditor/PalleteEditor/DirectXWrapper/OverlayBindingMatch.h"
#include <cassert>
#include "../DllEditor/PalleteEditor/DirectXWrapper/PaletteVisibility.h"
#include "../DllEditor/PalleteEditor/DirectXWrapper/PaletteTextureMatch.h"
int main() {
    using namespace OverlayBindingMatch;
    const auto gradients=PaletteTextureMatch::Build({0,0xffa08060,0xff402000},{1,2});
    assert(gradients.size()==2 && gradients[1][0]==0x402000 && gradients[1][15]==0xa08060);
    assert(gradients[1][5]==0x604020);
    assert(PaletteTextureMatch::Close(0xffa1815f,gradients[1][15]));
    assert(!PaletteTextureMatch::Close(0xffa4815f,gradients[1][15]));
    bool invalid=false;try{PaletteTextureMatch::Build({0},{2});}catch(...){invalid=true;}assert(invalid);
    PaletteTextureMatch::VisibilityHistory visibility;
    assert(!visibility.Contains(gradients));
    visibility.Record(gradients);assert(visibility.Contains(gradients));
    auto other=gradients;other[1][0]=0xffffff;assert(!visibility.Contains(other));
    visibility.Advance();visibility.Advance();assert(visibility.Contains(gradients));
    visibility.Advance();assert(!visibility.Contains(gradients));
    visibility.Record(gradients);assert(visibility.Contains(gradients));
    visibility.Clear();assert(!visibility.Contains(gradients));
    // The target follows A/B/A/B; a second fighter changes only during startup.
    Snapshot marker{{1,10},{2,20}},primary{{1,11},{2,21}},againMarker{{1,10},{2,21}};
    auto c=Changed(marker,primary);assert(c.size()==2);
    Confirm(c,marker,againMarker);assert(c==std::set<Id>{1});
    Confirm(c,primary,primary);assert(c==std::set<Id>{1});
    // Two fighters sharing a marker remain ambiguous, never selected by order.
    c=Changed(marker,primary);Confirm(c,marker,marker);Confirm(c,primary,primary);assert(c.size()==2);
    // Missing, inconsistent, and stale texture bindings cannot pass confirmation.
    c=Changed(marker,primary);Confirm(c,marker,Snapshot{{1,0}});assert(c.empty());
    assert(Changed(Snapshot{{1,0}},Snapshot{{1,11}}).empty());
    c=Changed(marker,primary);Confirm(c,primary,Snapshot{{1,12},{2,22}});assert(c.empty());
}
