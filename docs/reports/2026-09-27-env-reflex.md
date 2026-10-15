# Reflex cross-color card status

Date: 2026-09-27. Fixed source: v0.111.0 `Models/Cards/Reflex.cs`.

Added the original OnPlay draw operation through `CardPileCmd::Draw`. Generated card data already holds its cost, Sly keyword, type and upgrade values. The original Ironclad `SEED0004` policy 1 Act 2 trace now passes the root's previous Reflex support check, but stops at another cross-color card, The Hunt, at the same decision 482/737. That means Reflex's OnPlay has not yet been executed and is not claimed as differential validated.
