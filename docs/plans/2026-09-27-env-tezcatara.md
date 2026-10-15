# Generic Env: Tezcatara options

Date: 2026-09-27. The first-act/second-act Oracle batch has three warrior runs blocked at Tezcatara's initial option generation.

## Goal

Port v0.111.0 `Tezcatara.GenerateInitialOptions` with the original three relic pools, the conditional Nutritious Soup option, and exact mutable relic allocation and local RNG order.

## Source behavior

The event materializes pool 1 in order: Very Hot Cocoa, Yummy Cookie, and Nutritious Soup only if the deck has a Basic Strike. It selects one option with the event-local RNG. It then materializes pool 2 in order: Biiig Hug, Storybook, Toasty Mittens, and draws once. Finally it materializes pool 3 in order: Golden Compass, Pumpkin Candle, Toy Box, Seal of Gold, and draws once. Each option is an AncientRelicOption, which obtains the selected relic and finishes the event when chosen.

## Implementation

- Add `game/Models/Events/Tezcatara.h` with all candidate relic options created before each pool's draw.
- Preserve source pool order and the three `Rng().NextItem` calls.
- Do not approximate effects of relics whose model overrides are not yet ported; option selection will continue through the existing explicit Unported dispatch for those behaviors.

## Acceptance

- Pool 1 contains two candidates without a Basic Strike and three with one.
- All three pools allocate their relic candidates before their respective selection draw.
- Selected options match the source RNG sequence and use ancient relic option semantics.
- The integrating agent verifies the initial option arrays and next RNG state for the three blocked warrior traces against the oracle.

## Files

- `game/Models/Events/Tezcatara.h`
- `docs/plans/2026-09-27-env-tezcatara.md`
- `docs/reports/2026-09-27-env-tezcatara.md`
