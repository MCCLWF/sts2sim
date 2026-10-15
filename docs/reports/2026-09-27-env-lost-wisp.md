# Lost Wisp event port

Date: 2026-09-27.

## Preconditions

- The source-backed Ironclad act 2 trace stops at Lost Wisp's `CalculateVars` at decision 546 of 564; the original action is Search (index 1).
- Read `Models/Events/LostWisp.cs`, `Models/Relics/LostWisp.cs`, `EventModel.cs`, the event option and card-pile/relic command implementations, and nearby event ports.

## Change

Added `game/Models/Events/LostWisp.h`. It consumes one local `NextInt(-15, 16)` draw to store 60 plus the result in canonical event variable slot 0, produces Claim then Search, and ports each action in original call order. Claim adds Decay, obtains Lost Wisp, and finishes. Search grants the stored amount and finishes.

The exported `EventData` entry currently has an empty variable table despite the source declaring canonical Gold/Relic/Curse variables. The event stores and uses the numeric Gold value directly in its matching `EventState.vars[0]` slot. Event query output currently exposes option and description keys, not resolved event variables or hover tips.

Claim obtains the relic without approximating its separate combat passive. The relic's `AfterCardPlayed` hook remains unported and will continue to raise the generic Env's explicit unported-hook error if that claim branch later reaches a Power card play.

## Verification

- Source ordering and existing command APIs: inspected; the event option and action flow require no shared-file change.
- Integration: `tools/gen/gen_dispatch.py` and Release CMake build passed. Existing original Ironclad Act 2 trace `/tmp/mcc-env-act2-oracle.jsonl` with `l4_run.py --trace ... --verify-snapshots --require-complete --verbose` passed **1/1 run, 564/564 decisions**, zero mismatch/Unported; `/tmp/mcc-env-act2-lost-wisp-complete.log`. The route selects Search; Claim remains source checked but lacks a selected-branch Oracle path.

## Limits

The event's string dynamic vars and hover tips are presentation data not represented in the current run query. The Lost Wisp relic's combat hook is outside this event port and remains explicitly unsupported.
