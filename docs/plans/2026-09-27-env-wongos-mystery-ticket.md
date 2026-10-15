# Generic Env: Wongos Mystery Ticket

Date: 2026-09-27. This continues the Ironclad act 2 Env trace; Welcome to Wongos grants the relic at decision 457.

## Goal

Port the v0.111.0 `WongosMysteryTicket` combat counter and reward modification behavior exactly, including the first eligible combat reward set and the one-time used-up state.

## Source behavior

The original relic starts with zero completed combats and `GaveRelic == false`. Each `AfterCombatEnd` increments `CombatsFinished` and sets the `RemainingCombats` dynamic variable to `max(5 - CombatsFinished, 0)`. Its display amount is `5 - CombatsFinished`, and it shows a counter only while that amount is positive. On the owner’s combat-room rewards, once five combats have ended and before it has granted its reward, it appends three standard relic rewards and reports itself as a modifier. `AfterModifyingRewards` marks it used up and disables it.

## Implementation

- Add `game/Models/Relics/WongosMysteryTicket.h`.
- Store the two saved properties in existing relic extra slots.
- Use existing combat-end, reward-modification, reward-list, relic-state, and dynamic-variable primitives.
- Leave generated dispatch and build integration to the integrating agent.

## Acceptance

- The first five completed combats decrement the counter; the next eligible combat reward set gets three standard relic rewards.
- Other players, non-combat rooms, reward sets before the fifth combat, and any reward set after the relic was used do not receive extra rewards.
- After the reward hook finishes, the relic is used up and disabled.
- Oracle verification covers the actual Ironclad trace at decision 457 and reward generation.

## Files

- `game/Models/Relics/WongosMysteryTicket.h`
- `docs/plans/2026-09-27-env-wongos-mystery-ticket.md`
- `docs/reports/2026-09-27-env-wongos-mystery-ticket.md`
