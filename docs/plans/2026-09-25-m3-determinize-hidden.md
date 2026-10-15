# M3 hidden-state determinization

- Branch: `feat/m3-determinize-hidden`
- Scope: extend `Env::Determinize` beyond combat draw/discard piles to future room-set queues and remaining relic grab bags that are not exposed by the observation.
- Preserve active combat/event/reward/shop/treasure state and the visible map/hand/board. Shuffle only unvisited future queue suffixes and remaining relic bag contents with a dedicated deterministic stream.
- Acceptance: same seed gives the same hidden hash, different seeds preserve `Observe()` and legal actions while changing hidden state on a prepared run, pending choices still reject determinization, and full `scripts/check.sh` remains green.
- Clone performance remains a separate M3/M5 measurement item.
