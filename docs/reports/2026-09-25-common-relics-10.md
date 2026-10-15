# Shared relics batch 10 report

## Scope and source

Ported `Cauldron`, `Orrery`, and `Toolbox` from the v0.111.0 game sources.

- Cauldron offers five independently generated potion rewards on pickup. The TestMode-only fixed potion branch is omitted, per repository rules.
- Orrery offers five card rewards, each containing three Ironclad-pool cards generated with regular-encounter rarity odds.
- Toolbox generates three distinct combat-generatable Colorless cards before the owner's first hand draw, prompts the player to choose one, and adds it to hand.

The shared relic pool has 118 entries. C++ model headers now exist for 112; the six remaining without headers are `DollysMirror`, `GnarledHammer`, `MembershipCard`, `PunchDagger`, `RoyalStamp`, and `UnsettlingLamp`. This branch now has direct behavior hooks for 43 shared relics.

## Verification

| Check | Result |
|---|---|
| `cmake --build build --target test_relics -j4` | Passed |
| `./build/test_relics` | Passed; asserts five potion rewards, five Orrery offers of three Ironclad cards each, and Toolbox's selected Colorless card entering hand |
| `./scripts/check.sh` build and unit tests | Passed; all 8 CTest cases |
| Whole-run smoke | Passed; `death=93 unported=7`, `death=99 unported=1`, and `boss=4 death=96`; no crashes or hangs |
| L2 setup/maps | 200/200 identical |
| L3 Ironclad A0/A10 | 40/40 each; 784/784 and 827/827 states matched |
| L4 Ironclad A0/A10 | 40/40 each; 1958/1958 and 2131/2131 decisions matched; 0 unported and 0 diverged |

Oracle compilation reported one pre-existing nullable warning in `tools/Sts2Oracle/OracleRun.cs`; build succeeded with zero errors.

## Limits

- The six relics without headers remain unported shared content. Membership Card depends on the unported merchant-price hook; Dolly's Mirror, Gnarled Hammer, Punch Dagger, and Royal Stamp require card selection/enchantment paths; Unsettling Lamp needs stateful debuff tracking.
- L4 is a broad integration regression; direct model tests cover these relics because random runs do not guarantee acquiring them.
- Cross-character card generation during Ironclad runs remains explicitly unimplemented.
