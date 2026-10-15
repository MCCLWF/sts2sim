# Shared relics batch 8 report

## Scope and source

Ported `BurningSticks`, `Pocketwatch`, `Vambrace`, and `RippleBasin` from the v0.111.0 source. Their combat counters and Vambrace's triggering card reference are stored in fixed-size relic state.

The shared relic pool contains 118 entries. C++ relic model headers now exist for 106; the 12 without headers are: `Cauldron`, `DollysMirror`, `GnarledHammer`, `LavaLamp`, `MembershipCard`, `Orrery`, `PunchDagger`, `RoyalStamp`, `StoneCracker`, `Toolbox`, `UnsettlingLamp`, and `WingCharm`.

## Verification

| Check | Result |
|---|---|
| `cmake --build build -j2` | Passed |
| `./build/test_relics` | Passed, including once-per-combat skill copy, Pocketwatch turn threshold, Vambrace card-linked doubling, and Ripple Basin attack-free turn block |
| `./scripts/check.sh` build and unit tests | Passed; 8/8 CTest cases |
| Whole-run smoke | Passed; observed 4 boss clears, deaths, and controlled unported stops; no crashes or hangs |
| L2 setup/maps | 200/200 identical |
| L3 Ironclad A0/A10 | 40/40 each; 784/784 and 827/827 states matched |
| L4 Ironclad A0/A10 | 40/40 each; 1958/1958 and 2131/2131 decisions matched; 0 unported and 0 diverged |

Oracle compilation reported one pre-existing nullable warning in `tools/Sts2Oracle/OracleRun.cs`; build succeeded with zero errors.

## Limits

- L4 is a broad integration regression; direct model tests cover these relic hooks because random runs do not guarantee acquisition of each one.
- The 12 relics without headers remain unported shared content. Cross-character cards in Ironclad runs remain unimplemented.
