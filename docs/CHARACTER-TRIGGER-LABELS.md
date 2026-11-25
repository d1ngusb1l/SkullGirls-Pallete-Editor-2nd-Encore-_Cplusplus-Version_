# Character trigger labels and resource providers

Existing local animation metadata contains useful authored names. These are
search candidates for move-based rules, not verified persistent resource states.
The names below were found in the generated preview metadata from the local game
archives; no new memory scan is needed to find these labels.

| Character | Example authored labels | Possible search use |
| --- | --- | --- |
| Annie | `s.StarPower`, `s.StarPowerDeactivate`, `j.StarPowerDeactivate` | Activation/deactivation moves |
| Umbrella | `j.HP_Overstuffed_Loop`, `HungernRush_Ascend_Empty`, `HungernRush_Ascend_Full`, `HungernRush_Ascend_Overstuffed` | Hunger-specific move variants |
| Black Dahlia | `s.Reload_LP`, `s.Reload_MP`, `s.Reload_HP`, `s.SuperReload`, `Reload_Catch` | Reload animations |
| Squigly | `DragonStance_Lv2Loop` | Stance/charge animation variants |
| Valentine | `DHC_Out_ClearLoadedPoisons`, `s.CounterSuper_Poison_A`, `s.CounterSuper_Poison_B`, `s.CounterSuper_Poison_C` | Poison-related move variants |
| Ms. Fortune | `s.HK_Headless`, `s.HP_Headless_Assist` | Headless move variants |
| Robo-Fortune | `s.DetMode_Recharge_L`, `s.DetMode_Recharge_M`, `s.DetMode_Recharge_H` | Detonation-mode move variants |
| Peacock | `s.ItemDrop_FullCharge_L`, `s.ItemDrop_FullCharge_M`, `s.ItemDrop_FullCharge_H` | Charged item-drop animations |

An animation name is not necessarily an activation tag accepted by the native
palette predicate. Inspect its authored expression/tag references before exposing
it as a live trigger. A move animation also does not establish how long a mode,
charge, ammunition count or poison remains active after that move ends.

Resource variables are commonly numeric script IDs rather than semantic labels.
For a new persistent resource, inspect authored reads/writes, resolve the native
operand type and owner, then correlate controlled gains/spends and a mirror match.
Repeat that investigation once per distinct resource, not once per palette rule.
Keep unsupported resources unavailable until the ownership and semantics are proven.

Beowulf Hype is the first typed amount provider here: completed levels 0, 1, 2 and
3 can be reused by arbitrary rules. Its validation is documented in
[the investigation](BEOWULF-RESOURCE-INVESTIGATION.md). Other resources in this table
are candidates only; they have not been enabled by this change.
