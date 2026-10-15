# Reflex cross-color card

Date: 2026-09-27. Fixed source: v0.111.0 `Models/Cards/Reflex.cs`.

The original Ironclad `SEED0004` policy 1 Act 2 trace obtains this Silent card from Colorful Philosophers and then cannot enter combat because its card model is Unported. Add the original OnPlay draw effect through CardPileCmd. The card's Sly keyword, cost, type, upgrade and vars are supplied by the generated content model. Rebuild and replay that trace with snapshots; record any new blocker rather than changing the oracle policy.
