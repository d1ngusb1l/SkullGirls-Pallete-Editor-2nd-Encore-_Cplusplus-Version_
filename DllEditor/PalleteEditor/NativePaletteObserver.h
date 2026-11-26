#pragma once
#include <cstdint>
namespace NativePaletteObserver {
using Callback=void(*)(uintptr_t palette,int index,uintptr_t cache,uintptr_t entity);
bool Supported(uintptr_t moduleBase);
bool Installed();
void Install(uintptr_t moduleBase,Callback observer);
void Remove();
}
