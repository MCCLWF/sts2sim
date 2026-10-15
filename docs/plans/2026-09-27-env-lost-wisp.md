# Generic Env: Lost Wisp

Date: 2026-09-27. The actual Ironclad act 2 trace reaches Lost Wisp at decision 546 of 564 and chooses Search (option index 1).

## Goal

Port Lost Wisp's original event options and actions, retaining its event-local gold roll and exact Claim/Search action order.

## Source behavior

The original event sets `Gold` to 60 plus one `Rng.NextInt(-15, 16)` draw before generating two options in order: Claim, then Search. Claim adds Decay to the deck, obtains Lost Wisp, then finishes the event. Search grants the rolled gold, then finishes the event.

## Implementation

- Add `game/Models/Events/LostWisp.h` with the source-ordered options and action sequence.
- The current generated `EventData` entry has no vars even though the source declares Gold, Relic, and Curse canonical vars. Keep the numeric Gold value in `EventState.vars[0]`, matching the source canonical order, rather than adding a shared table change.
- Do not approximate the separate Lost Wisp relic combat passive. Claim obtains the relic as the source does; its later `AfterCardPlayed` behavior remains an explicit unported relic hook until that relic is ported separately.

## Acceptance

- Event begin consumes exactly one event-local `NextInt(-15, 16)` draw.
- The options remain Claim at index 0 and Search at index 1.
- Claim adds Decay, obtains the Lost Wisp relic, then finishes. Search grants the rolled gold, then finishes.
- The integrating agent verifies the observed Search path in the Ironclad act 2 trace against the v0.111.0 oracle.

## Files

- `game/Models/Events/LostWisp.h`
- `docs/plans/2026-09-27-env-lost-wisp.md`
- `docs/reports/2026-09-27-env-lost-wisp.md`
