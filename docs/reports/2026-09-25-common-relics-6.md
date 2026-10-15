# Shared relics batch 6 report

## Scope and source

Ported `LizardTail`, `Kunai`, `ReptileTrinket`, and `LoomingFruit` from the v0.111.0 source. Added `ReptileTrinketPower` with the inherited temporary-strength behavior: it raises Strength when applied and removes that amount at the end of its owner's turn.

The shared relic pool contains 118 entries. C++ relic model headers now exist for 98; 20 remain without model headers. Header presence does not imply that every behavior is complete. Remaining entries include relics that depend on not-yet-ported merchant pricing, selection, and other hooks.

## Verification

| Check | Result |
|---|---|
| `cmake --build build -j2` | Passed |
| `./build/test_relics` | Passed, including death prevention and healing, pickup max HP, Kunai turn counter/reset, and temporary-strength expiration |
| `./scripts/check.sh` build and unit tests | Passed; 8/8 CTest cases |
| Whole-run smoke | Passed; observed 4 boss clears, deaths, and controlled unported stops; no crashes or hangs |
| L2 setup/maps | 200/200 identical |
| L3 Ironclad A0/A10 | 40/40 each; 784/784 and 827/827 states matched |
| L4 Ironclad A0/A10 | 40/40 each; 1958/1958 and 2131/2131 decisions matched; 0 unported and 0 diverged |

Oracle compilation reported one pre-existing nullable warning in `tools/Sts2Oracle/OracleRun.cs`; build succeeded with zero errors.

## Limits

- L4 is a broad integration regression; direct model tests cover the new hooks because random runs do not guarantee acquisition of every relic.
- The 20 relics without model headers remain available as unported shared content.
- Cross-character cards in Ironclad runs remain unimplemented and should be reported as such when reached.
