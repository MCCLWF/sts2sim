# Generic Env: SandCastle AfterObtained

Date: 2026-09-27. This is a focused continuation of the Ironclad Orobas run work in [`2026-09-27-env-orobas-ironclad-act2.md`](2026-09-27-env-orobas-ironclad-act2.md).

## Goal

Port the v0.111.0 `SandCastle.AfterObtained` behavior so selecting SandCastle from Orobas option pool 2 applies the same upgrades and consumes the same Niche RNG draws in the generic Env.

## Source behavior

`Models/Relics/SandCastle.cs` filters the deck to upgradable cards, materializes that list, stable-shuffles it with `Owner.RunState.Rng.Niche`, takes `DynamicVars.Cards.IntValue` cards (canonical value 6), and upgrades each selected card. The grid-preview column call is presentation only.

## Implementation

- Add `game/Models/Relics/SandCastle.h` with the pickup-effect flag and the source-ordered deck filter, `CardPileCmd::StableShuffle` using Niche, capped count, and `CardCmd::Upgrade` calls.
- Use existing shared primitives only; do not modify generated code or RNG/card commands here.

## Acceptance

- SandCastle pool-2 acquisition upgrades the same cards as the game and advances Niche by the same shuffle draws.
- No eligible cards or one eligible card consumes no shuffle draws; the selected count is capped by both the canonical dynamic variable and eligible-card count.
- The downstream Env integration verifies the Orobas pool-2 branch and the Ironclad run against the v0.111.0 oracle.

## Files

- `game/Models/Relics/SandCastle.h`
- `docs/plans/2026-09-27-env-sand-castle.md`
- `docs/reports/2026-09-27-env-sand-castle.md`
