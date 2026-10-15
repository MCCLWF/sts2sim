# Pael's Legion run content

Source: v0.111.0 `Models/Relics/PaelsLegion.cs`, `Models/Monsters/PaelsLegion.cs`, and `Models/Events/Pael.cs`. Pael's Legion is an ancient relic offered by Pael and is also in the event relic pool.

Implement the pet's creature and idle move machine, summon it on combat start or mid-combat pickup, and port the block multiplier, card-play cooldown, counter, and turn/combat reset hooks in source order. Keep mutable state in relic POD slots. Animation and skin selection do not affect run RNG or decisions. Regenerate dispatch and source inventory; test local hooks, direct combat oracle, snapshot-restored run oracle, and the full gate. Record any unverified card-play identity case as a coverage gap rather than assuming all paths are proven.
