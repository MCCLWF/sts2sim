# Shared relics batch 11 report

## Scope and source

Ported `DollysMirror`, `GnarledHammer`, `PunchDagger`, and `RoyalStamp` from the v0.111.0 game sources.

- Dolly's Mirror selects a non-Quest deck card, clones it into the run, and adds the clone to the deck.
- Gnarled Hammer allows skipping or selecting up to three eligible attack cards and applies Sharp 3 to each selected card.
- Punch Dagger selects one eligible attack card and applies Momentum 5.
- Royal Stamp filters for eligible attack/skill cards, consumes a Niche-stream unstable shuffle, selects one, and applies Royally Approved 1. The selection helper presents cards in deck order, matching the source helper's ordering.

The shared relic pool has 118 entries. C++ model headers now exist for 116; the two remaining without headers are `MembershipCard` and `UnsettlingLamp`. This branch now has direct behavior hooks for 47 shared relics.

## Verification

| Check | Result |
|---|---|
| `cmake --build build --target test_relics -j4` | Passed |
| `./build/test_relics` | Passed; asserts Dolly's Mirror clone, Sharp 3 and Momentum 5 on attacks, and Royally Approved 1 on an attack/skill |
| `./scripts/check.sh` build and unit tests | Passed; all 8 CTest cases |
| Whole-run smoke | Passed; `death=93 unported=7`, `death=99 unported=1`, and `boss=4 death=96`; no crashes or hangs |
| L2 setup/maps | 200/200 identical |
| L3 Ironclad A0/A10 | 40/40 each; 784/784 and 827/827 states matched |
| L4 Ironclad A0/A10 | 40/40 each; 1958/1958 and 2131/2131 decisions matched; 0 unported and 0 diverged |

Oracle compilation reported one pre-existing nullable warning in `tools/Sts2Oracle/OracleRun.cs`; build succeeded with zero errors.

## Limits

- Membership Card depends on the merchant-price hook and merchant entry API, which are not ported yet.
- Unsettling Lamp needs stateful tracking of the triggering card and temporary debuff powers.
- L4 is a broad integration regression; direct model tests cover these relics because random runs do not guarantee acquiring them.
- Cross-character card generation during Ironclad runs remains explicitly unimplemented.
