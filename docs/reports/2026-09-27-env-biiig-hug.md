# Biiig Hug relic port

Date: 2026-09-27.

## Preconditions

- Read `AGENTS.md` and `docs/audits/pitfalls.md`.
- Inspected v0.111.0 `Models/Relics/BiiigHug.cs`, `CardSelectCmd.FromDeckForRemoval`, `CardSelectorPrefs`, and `Models/Cards/Soot.cs`.
- Inspected the simulator's deck-removal selection, deck-removal command, generated-card insertion command, relic hook signature, and canonical `BIIIG_HUG` `Cards` value (4).

## Change

Added `game/Models/Relics/BiiigHug.h`. On pickup it uses the `Cards` dynamic variable as the exact selection count, calls the existing removable-deck selector, and removes the selected cards in the order returned. The selector retains the source ordering of curses first and the remaining cards in deck order.

After the owner's shuffle, it creates `CardId::SOOT` in that combat state with the relic owner and adds it to the draw pile at `CardPilePosition::Random`. The existing command uses the run's Shuffle RNG stream for the insertion position. A non-owner shuffle has no effect.

## Verification

- Source conditions, selection count, removable-card filter/order, card-removal order, Soot owner, pile type, random insertion position, and hook order: inspected against the v0.111.0 source and existing simulator APIs.
- Build, tests, Oracle, and CombatSolver: not run, as assigned to the integrating agent.

Integration update: generated dispatch now registers Biiig Hug and the C++ build passes. Against the existing eight-run Act 2 oracle corpus, its pickup selection and following decisions match through 234/619 on `SEED0003` policy 2, up from 224; replay then stops because the generated Soot card is unported. Per-decision snapshot restore was enabled. This does not yet validate Biiig Hug's shuffle effect or a completed run. The full-run oracle has since received a presentation-only Kaiser death patch, so future fresh traces may have different decision totals after the Act 2 boss than this earlier corpus.

Later integration ported Soot and Metamorphosis. A fresh, valid original `SEED0003` policy 2 trace now matches **595/595 decisions** with snapshot restore through the Act 2 boundary (`/tmp/mcc-env-seed0003-p2-metamorphosis-l4.log`). This path includes the Biiig Hug pickup and generated Soot in combat, but remains one finite choice path.

## Unported

- The localized `TO_REMOVE` selection prompt has no representation in the headless Env selection API.
- The post-insertion `CardCmd.Preview(soot, 0.75f)` and `Cmd.Wait(1f)` are presentation-only and have no simulator API. The relic flash call is retained; `RelicModel::Flash()` is a no-op in the simulator.
