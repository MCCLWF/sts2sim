# Colorful Philosophers differential result

Date: 2026-09-27. Fixed source: v0.111.0 `Models/Events/ColorfulPhilosophers.cs`.

The generic Env now builds the foreign character card pools in the source color order, removes a random offer until three remain, and uses ordinary nested card rewards for the selected pool's Common, Uncommon and Rare cards. It preserves three separate reward instances and their later selection. The original Ironclad `SEED0004` policy 1 Act 2 trace chooses Silent and advanced from 465/737 to 482/737 decisions with no difference and per-decision snapshot restore.

That run next stopped at an unported cross-color card in the combat root. Reflex was subsequently ported, leaving The Hunt as the next explicit unported card. Only the full veteran profile was exercised. Partial unlock filtering and alternate chosen colors still need targeted original evidence.
