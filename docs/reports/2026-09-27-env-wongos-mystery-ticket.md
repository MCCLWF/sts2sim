# Wongos Mystery Ticket port

Date: 2026-09-27.

## Preconditions

- The generic Env Ironclad trace reaches Welcome to Wongos and obtains Wongos Mystery Ticket at decision 457.
- The v0.111.0 relic source and existing generic hook/reward patterns were inspected.
- `AfterCombatEnd`, `TryModifyRewards`, `AfterModifyingRewards`, `IsUsedUp`, `ShowCounter`, and `DisplayAmount` signatures and dispatch points already exist. `RewardList::Add`, `Reward::NewRelicReward`, relic extra slots, status updates, and relic dynamic-variable storage are also present.

## Change

Added `game/Models/Relics/WongosMysteryTicket.h`. It stores `CombatsFinished` and `GaveRelic` in relic extra slots, updates the `RemainingCombats` variable after combat end, adds the canonical `Repeat` count of standard relic rewards to the owner’s first combat reward set after five completed combats, and marks/disables itself after reward modification.

The original display-change notification and `Flash()` call are presentation effects; the Env exposes current counter/status values directly.

## Verification

- Source-to-hook mapping and existing primitives: inspected; no unsupported shared behavior was found.
- Integration: `tools/gen/gen_dispatch.py` and Release CMake build passed. Original v0.111.0 Oracle comparison with `l4_run.py --seeds SEED0002 --policy-seeds 1 --no-neow --hp 10000 --relics WONGOS_MYSTERY_TICKET --stop after-act1 --verify-snapshots --require-complete --verbose` passed **1/1 run, 256/256 decisions**, zero mismatch/Unported; `/tmp/mcc-env-wongo-ticket-five-combat-l4.log`. Original trace `/tmp/mcc-env-wongo-ticket-five-combat-oracle.jsonl` records five combat completions and three added relic rewards at the fifth.

## Limits

This verifies one fixed-seed five-combat path, not every reward modifier combination or more than five combat completions. Original event-acquisition path reached 546/564 decisions before an unrelated LostWisp gap; see [event report](2026-09-27-env-welcome-to-wongos.md).
