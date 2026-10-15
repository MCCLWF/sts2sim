# Fake Merchant Event Encounter port report

Date: 2026-09-26
Branch: `feat/all-encounters-comparison-mod`
Game baseline: v0.111.0; source of truth is decompiled source and `Sts2Oracle`.

## Result

`FAKE_MERCHANT_EVENT_ENCOUNTER` now generates the Fake Merchant in the exported `merchant` slot. Its encounter metadata supplies the fixed 300-gold reward. The monster's health and attack values, weighted move branches, three-move Enrage cooldown, eight-hit coin spray, Frail, and Strength effects are ported.

Oracle L3 results:

| Ascension | Fights | Decision states | Unported | Diverged |
|---|---:|---:|---:|---:|
| A0 | 4/4 | 98/98 | 0 | 0 |
| A10 | 2/2 | 47/47 | 0 | 0 |

## Validation

Model dispatch generation:

```text
python3 tools/gen/gen_dispatch.py
ported models: 774; dispatch cases: MonsterModel 114, EncounterModel 66
```

Targeted build:

```text
cmake --build build -j4 --target test_env sts2env_c sts2sim_env_solver sts2sim_cli
```

Build completed successfully.

A0 L3:

```text
DOTNET="$HOME/.dotnet/dotnet" python3 tests/l3/l3_combat.py \
  --seeds NEW001,NEW002 --characters IRONCLAD --policy-seeds 1,2 \
  --ascension 0 --encounter FAKE_MERCHANT_EVENT_ENCOUNTER -v
L3 combat: 4/4 combats identical, 0 unported, 0 diverged, 98/98 states matched
```

A10 L3:

```text
DOTNET="$HOME/.dotnet/dotnet" python3 tests/l3/l3_combat.py \
  --seeds A10001 --characters IRONCLAD --policy-seeds 1,2 \
  --ascension 10 --encounter FAKE_MERCHANT_EVENT_ENCOUNTER -v
L3 combat: 2/2 combats identical, 0 unported, 0 diverged, 47/47 states matched
```

The first L3 attempt diverged because the C# overload `AddBranch(state, 3, MoveRepeatType.CannotRepeat)` uses `3` as cooldown, not weight. The port was corrected to preserve that overload; the results above are from rerunning both ascensions after rebuilding the CLI.

Quick gate:

```text
scripts/check.sh --quick
100% tests passed, 0 tests failed out of 8
smoke: death=94 unported=6
smoke: death=100
smoke: boss=3 death=97
quick check passed (differential checks skipped)
```

## Scope

The tests cover the listed seeds, ascensions, and Ironclad combat policies. They do not validate every player deck or random branch history, and the full repository L2/L3/L4 gate remains outstanding.
