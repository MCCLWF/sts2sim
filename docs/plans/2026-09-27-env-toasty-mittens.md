# Generic Env: Toasty Mittens

Date: 2026-09-27.

## Goal

Port the v0.111.0 `Models/Relics/ToastyMittens.cs` turn-start effect to a generic Env relic header, preserving its owner check, hand selection, exhaust, and Strength order.

## Source behavior

- `AfterPlayerTurnStart` returns for a non-owner player.
- For the owner, flash, choose one card from hand with the exhaust selection prompt, exhaust the selected card, then apply the relic's `StrengthPower` dynamic variable (canonical value 1).
- An empty hand produces no selected cards, but the Strength application still occurs.

## Implementation

- Add `game/Models/Relics/ToastyMittens.h`.
- Use the existing resumable `CardSelectCmd::FromHand`, `CardCmd::Exhaust`, and `PowerCmd::Apply` APIs. The generic selector carries the `FromHand` choice source; the localized selection prompt is presentation-only and is not represented by this Env API.
- Do not modify generated dispatch or other generated files in this task. Dispatch integration remains necessary before the new hook can be reached at runtime.

## Acceptance

- The header preserves the v0.111.0 hook condition and command ordering, including Strength after an empty-hand result.
- Report dispatch integration as pending; leave build and Oracle validation to the integrating agent.
