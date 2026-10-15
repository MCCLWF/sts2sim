# Shared relics batch 5 report

## Scope and source

Ported eight shared relics from the v0.111.0 source: `RingingTriangle`, `TheAbacus`, `UnceasingTop`, `MiniatureTent`, `RazorTooth`, `PrayerWheel`, `PetrifiedToad`, and `TinyMailbox`. The implementations preserve each source hook's owner, room, turn-phase, and card-type checks. Model dispatch was regenerated.

The shared relic pool contains 118 entries. C++ relic model headers now exist for 94; 24 remain without model headers. Header presence does not mean every relic behavior is implemented. The full list of missing headers is available in the current source pool and includes relics with unported merchant, timeline, and multi-hook behaviors.

## Verification

| Check | Result |
|---|---|
| `cmake --build build -j2` | Passed |
| `./build/test_relics` | Passed, including new tests for all eight models |
| `./scripts/check.sh` build and unit tests | Passed; 8/8 CTest cases |
| Whole-run smoke | Passed; observed 4 boss clears, deaths, and controlled unported stops; no crashes or hangs |
| L2 setup/maps | 200/200 identical |
| L3 Ironclad A0/A10 | 40/40 each; 784/784 and 827/827 states matched |
| L4 Ironclad A0/A10 | 40/40 each; 1958/1958 and 2131/2131 decisions matched; 0 unported and 0 diverged |

Oracle compilation reported one pre-existing nullable warning in `tools/Sts2Oracle/OracleRun.cs`; build succeeded with zero errors.

## Limits

- `TinyMailbox`'s two potion rewards are modeled. Its additional localized rest-site text is presentation-only and its hook signature is not represented in the simulator.
- L4 remains a broad integration regression; the new relic hooks are also covered by direct behavior tests because random runs do not guarantee that every one of them is acquired.
- Shared relic generation can still encounter the 24 relics without model headers. Cross-character cards in Ironclad runs remain unimplemented and should be reported as such when reached.
