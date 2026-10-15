# The Hunt differential result

Date: 2026-09-27. Fixed source: v0.111.0 `Models/Cards/TheHunt.cs`.

The generic Env now attacks through the ordinary card damage builder, checks the target's pre-attack Fatal eligibility, and, on a qualifying kill, adds the room's ordinary card reward and applies The Hunt Power. The original Ironclad `SEED0004` policy 1, A0, no Neow, 10000 HP Act 2 trace actually played this cross-color card twice after obtaining it from Colorful Philosophers. With this port, the route reaches the third act at 737/737 decisions, no differences, with per-decision snapshot restoration. The previous stop was 482/737 at the root's unsupported The Hunt card.

This validates that fixed route, including the two original plays. Other damage/target/Fatal conditions need targeted coverage before broad completeness claims. The run starts with test-aided 10000 HP and ends at the Act 3 boundary, not a full victory.
