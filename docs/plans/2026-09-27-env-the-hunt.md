# The Hunt cross-color Fatal card

Date: 2026-09-27. Fixed source: v0.111.0 `Models/Cards/TheHunt.cs`, with existing generic Fatal damage-result and extra-reward APIs. The original Ironclad `SEED0004` policy 1 Act 2 route receives The Hunt from Colorful Philosophers and stops at a combat root before the card can be played. Add its exact attack and Fatal reward behavior through ordinary game commands; do not infer success from the root acceptance alone. Rebuild and replay with original snapshots. CombatSolver remains frozen.
