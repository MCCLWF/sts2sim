# Infested Prisms Elite encounter

## Goal

Port `INFESTED_PRISMS_ELITE`, Infested Prism, Vital Spark, and the Tainted skill-card effect from game v0.111.0.

## Tasks

- Add Infested Prism health, four-move cycle, attack/block values, and Vital Spark growth.
- Add Tainted affliction and Tainted Power, including before-combat skill-card affliction and one enemy-turn of increased damage taken after a Tainted skill is played.
- Register the fixed elite encounter and regenerate dispatch.
- Run A0/A10 oracle L3 comparisons through card-play and enemy-turn callbacks.
- Update coverage documentation and run the quick gate.

## Acceptance

- Skills are afflicted at combat start and newly entered skill cards are afflicted while Vital Spark remains.
- Playing a Tainted skill applies the matching damage vulnerability until enemy-side turn end.
- A0/A10 L3 and `scripts/check.sh --quick` pass.

## Progress (2026-09-26)

- ✅ Infested Prism, Vital Spark, Tainted, Tainted Power, and elite encounter registered.
- ✅ A0 L3: 4/4 combats and 88/88 states; A10 L3: 2/2 and 39/39.
- ✅ `scripts/check.sh --quick`: 8/8 unit tests and all three smoke runs passed.
- ⏳ Full-repository L2/L3/L4 gate remains outstanding.
