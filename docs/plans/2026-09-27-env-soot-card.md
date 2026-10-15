# Soot card port for generic Env

Date: 2026-09-27. Status: **header and source-backed documentation staged**.

## Goal

Port the executable behavior of the v0.111.0 `Models/Cards/Soot.cs` card into the generic Env model layer. Use the fixed decompiled game source and the existing exported content row as the source of truth. Do not add CombatSolver behavior.

## Source facts

`Soot` is a sealed `CardModel`. It overrides `CanBeGeneratedInCombat` to return `false`, reports `MaxUpgradeLevel` as `0`, and its constructor calls the base constructor with energy cost `-1`, `CardType.Status`, `CardRarity.Status`, and `TargetType.None`. Its canonical keywords contain `Unplayable`.

The existing `data/content.json` Soot row already records the constructor metadata and canonical keyword. `alloc_card` initializes local keywords from the generated content table, and the generated content tables carry the card's cost/type/rarity/target data; these values do not require another hand-written override in `Soot.h`.

## Tasks

1. Add `game/Models/Cards/Soot.h` with the two source-defined executable overrides and the normal `SIM_MODEL` declaration.
2. Record the source mapping and the generated-dispatch dependency in the execution report.
3. Leave test, build, oracle, and CombatSolver changes to the integrating task. Generated model/dispatch integration is owned by the integrating task.

## Acceptance criteria

- The header matches the two executable overrides in the fixed v0.111.0 source.
- Existing content data remains the source for Soot's constructor-defined metadata and `Unplayable` keyword.
- The report records whether generic Env dispatch is wired in the shared worktree at report time and attributes generated-file changes separately from this header/docs contribution.
- No build, test, or oracle result is claimed for this header-only stage.
