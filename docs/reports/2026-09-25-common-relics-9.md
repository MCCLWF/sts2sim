# Shared relics batch 9 report

## Scope and source

Ported `LavaLamp`, `StoneCracker`, and `WingCharm` from v0.111.0. Stone Cracker sorts eligible draw-pile cards with .NET ordering, then shuffles on `CombatCardSelection`; Wing Charm selects one enchantable reward card through the Niche RNG stream and applies Swift.

The shared relic pool contains 118 entries. C++ relic model headers now exist for 109; the nine without headers are: `Cauldron`, `DollysMirror`, `GnarledHammer`, `MembershipCard`, `Orrery`, `PunchDagger`, `RoyalStamp`, `Toolbox`, and `UnsettlingLamp`.

## Verification

| Check | Result |
|---|---|
| `cmake --build build -j2` | Passed |
| `./build/test_relics` | Passed, including Lava Lamp damage qualification/reward upgrade, Stone Cracker upgrade count, and one Swift reward enchantment |
| `./scripts/check.sh` build and unit tests | Passed; 8/8 CTest cases |
| Whole-run smoke | Passed; observed 4 boss clears, deaths, and controlled unported stops; no crashes or hangs |
| L2 setup/maps | 200/200 identical |
| L3 Ironclad A0/A10 | 40/40 each; 784/784 and 827/827 states matched |
| L4 Ironclad A0/A10 | 40/40 each; 1958/1958 and 2131/2131 decisions matched; 0 unported and 0 diverged |

Oracle compilation reported one pre-existing nullable warning in `tools/Sts2Oracle/OracleRun.cs`; build succeeded with zero errors.

## Limits

- L4 is a broad integration regression; direct model tests cover these relic hooks because random runs do not guarantee acquisition of each one.
- The nine relics without headers remain unported shared content. Cross-character cards in Ironclad runs remain unimplemented.
