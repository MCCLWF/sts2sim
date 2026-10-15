# Pael's Legion run content

The v0.111.0 Pael event can offer this relic. Its source grants a pet and doubles owner-card block while ready, then enters a two-turn cooldown after that CardPlay finishes. The Env now ports the pet's 9999 HP, hidden health bar, idle move machine, summon timing, block hook, cooldown, status and counter, and combat cleanup. The pet's skin and animations are presentation-only; the skin's separate RNG does not consume run RNG.

The source's pending `CardPlay` reference is represented by card index plus series play index in relic POD state. This survives Env snapshots. A nested second `CardPlay` of the *same card index and series index* during the first play would need a distinct transient identity; that case has not been demonstrated or oracle-tested.

Verification: `test_paels_legion` checks in-combat pickup, pet state, actual block calculation, cooldown and reset. Direct original combat oracle: 4/4 fights and 81/81 states identical (`PAELNEW01/02`, policies 1/2, Cultists). Original run oracle with the relic granted: 4/4 first-act runs and 1513/1513 decisions identical (`PAELRUN01/02`, policies 1/2, snapshot restore). Full `scripts/check.sh` passed: 23/23 CTest, L2 200/200, L3 80/80 combats and 1611/1611 states, default L4 80/80 runs and 4089/4089 decisions, and all strict focused and multi-act routes with zero divergence or unported stops. The fixed gate traces are regression samples, not random 100+ seed coverage.

This does not prove all Pael relics or event outcomes complete. Pael's Blood, Claw, Eye, Flesh, Growth, Horn, and Tooth still lack C++ source ports.
