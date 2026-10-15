# Tezcatara option generation port

Date: 2026-09-27.

## Preconditions

- Three warrior traces in `/tmp/mcc-env-act2-batch-oracle.jsonl` stop at Tezcatara's `GenerateInitialOptions`.
- Read the v0.111.0 `Models/Events/Tezcatara.cs`, `EventModel.cs`, and the existing Tezcatara option candidates in `RelicModel` content tables.
- The generic Env already provides `AncientRelicOption`, materialized relic pools, and `Rng::NextItem`.

## Change

Added `game/Models/Events/Tezcatara.h`. It constructs pool 1 in source order, appends Nutritious Soup only when the deck has a card tagged Strike with Basic rarity, and draws once. It then constructs and draws from pool 2 followed by pool 3, preserving source candidate order and allocating every candidate before its pool's draw.

Each option uses `AncientRelicOption`, so the selected relic is obtained and the event finishes through the existing ancient-event callback. The event itself does not implement any relic effect. Existing ports include Very Hot Cocoa and Golden Compass; other candidate relics without a port will retain the engine's explicit Unported behavior when an unsupported override runs.

## Verification

- Source pool construction, order, condition, and three draw sites: inspected.
- Generated dispatch and Release build: passed in integration.
- Eight fixed original Ironclad Act 2 traces: the three that previously stopped at Tezcatara now reach their original option arrays and choices without difference. With per-decision snapshot restore, aggregate replay reached 3703/5684 decisions, with 3 complete runs, 5 explicit Unported stops and 0 differences. The next stops were Spirit Grafter, Biiig Hug and Toasty Mittens.

## Limits

The chosen options and subsequent RNG state are validated only on those three original paths. Relic effects remain governed by each relic's existing model coverage; two selected relics on the corpus were explicitly Unported.
