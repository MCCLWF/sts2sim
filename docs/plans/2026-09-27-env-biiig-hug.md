# Generic Env: Biiig Hug

Date: 2026-09-27.

## Goal

Port the v0.111.0 `BiiigHug` relic's deck removal on pickup and Soot insertion after the owner's shuffle.

## Source behavior

- On pickup, ask the owner to select up to four removable deck cards using the relic's `Cards` dynamic variable, then remove the selected cards in selection order. The source orders curses before the rest of the deck.
- After shuffle, only when the owner shuffled, create a Soot owned by the relic owner and add it to the draw pile at a random position, then flash the relic.
- The source also shows a localized removal prompt, previews Soot for 0.75 seconds, and waits one second. These are presentation behaviors.

## Implementation

- Add `game/Models/Relics/BiiigHug.h` using `CardSelectCmd::FromDeckForRemoval`, `CardPileCmd::RemoveFromDeck`, and `CardPileCmd::AddGeneratedCardToCombat`.
- Use the `Cards` dynamic variable for the selection count and `CardPilePosition::Random` for insertion so the existing shuffle RNG path determines the insertion index.
- The generic Env has no localized card-selection prompt, card preview, or wait API; leave those presentation details explicitly unported.
- Do not change generated dispatch, build scripts, Oracle, CombatSolver, or unrelated files.

## Acceptance

- Selection count, removable-card filtering and ordering, selected-card removal order, owner check, Soot creation/ownership, draw-pile destination, and random insertion match the source.
- Only the new relic header and this plan/report are edited; build, Oracle, and CombatSolver validation remain with the integrating agent.
