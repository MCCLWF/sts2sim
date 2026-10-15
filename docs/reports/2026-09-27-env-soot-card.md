# Soot card port for generic Env

Date: 2026-09-27. Result: **header and source-backed documentation staged; generic dispatch is present in the shared worktree**.

## Preconditions

- The fixed decompiled source at `../Slay the Spire 2/src/Core/Models/Cards/Soot.cs` is the v0.111.0 reference, as required by `AGENTS.md`.
- The existing `data/content.json` row for `CARD.SOOT` records `CanBeGeneratedInCombat: false`, `MaxUpgradeLevel: 0`, `CanonicalKeywords: [Unplayable]`, energy cost `-1`, Status type/rarity, and no target.
- `alloc_card` initializes local card keywords from the generated content row, so `Soot.h` does not need to reconstruct the canonical keyword set.

## Changes

- Added `game/Models/Cards/Soot.h` with `SIM_MODEL(Soot, CardModel)`, `CanBeGeneratedInCombat() == false`, and `MaxUpgradeLevel() == 0`.
- Added the scoped plan at `docs/plans/2026-09-27-env-soot-card.md`.

## Source comparison

The C# source declares exactly those two executable overrides. The constructor's base arguments and the canonical `Unplayable` keyword are already represented in `data/content.json` and consumed by the generic model's content-backed initialization.

## Validation and limits

- `rg -n 'CanBeGeneratedInCombat|MaxUpgradeLevel|CanonicalKeywords|base\(-1' '../Slay the Spire 2/src/Core/Models/Cards/Soot.cs'` returned source lines 8, 10, 12, and 15: false, zero, Unplayable, and the `(-1, Status, Status, None)` base arguments.
- `rg -n 'Soot.h|case CardId::SOOT' game/generated/AllModels.h game/generated/Dispatch.cpp` returned the Soot include at `AllModels.h:218` and dispatch cases at `Dispatch.cpp:2650` and `Dispatch.cpp:2753` (plus a `Dispatch.cpp:4998` card switch case). This confirms the generated integration currently present in the shared worktree.
- Inspected the Soot content row and `alloc_card`: the row has `-1` energy cost and keyword bit `0x10` (`Unplayable`); `alloc_card` copies the canonical level keyword bits from `CardData` into card state.
- At the first inspection, `game/generated/AllModels.h` and `game/generated/Dispatch.cpp` lacked Soot integration. Before this report was finalized, the shared worktree gained the `Soot.h` include and `CardId::SOOT` branches for both methods. Those generated-file edits are not part of this header/docs contribution; the current tree now wires these overrides to generic dispatch.
- No build, tests, or oracle comparisons were run, per the assigned scope. No runtime parity is claimed.
- No other Soot behavior is represented here. The source class has no additional play, draw, or turn-end behavior; localization, presentation, and UI behavior are outside this card-model port.

Integration update: the C++ build passed after generated dispatch included Soot. A fresh original `SEED0003` policy 2, A0, no Neow, 10000 HP trace compared with per-decision snapshot restore advanced from Biiig Hug's 234-decision prefix to 245/595 matching decisions; replay then explicitly stopped at the unrelated unported `METAMORPHOSIS` combat root. Raw log: `/tmp/mcc-env-seed0003-p2-soot-l4.log`. This is a finite path result, not general Soot generation coverage.

After the separate Metamorphosis port, the same fresh configuration passed **595/595 decisions** through the Act 2 boundary with per-decision snapshot restore (`/tmp/mcc-env-seed0003-p2-metamorphosis-l4.log`). Soot is therefore exercised on this one full path, not across all possible shuffles.
