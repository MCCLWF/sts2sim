# Spirit Grafter differential result

Date: 2026-09-27. Fixed game source: v0.111.0 `Models/Events/SpiritGrafter.cs`.

The generic Env now generates both original options. Let It In heals before adding Metamorphosis to the deck. Rejection performs the existing resumable deck upgrade selection, upgrades the chosen card, then applies 10 unblockable and unpowered HP loss. Both use the content table's dynamic values.

The original Ironclad `SEED0003`, policy 1, A0, no Neow, 10000 HP trace advanced from 279/602 to 328/602 decisions with no difference under `--verify-snapshots`. The actual route chooses Rejection and upgrades a Strike. This validates that path and its pending choice restore; the alternate Let It In branch still needs targeted differential evidence. The next stop was Ranwid the Elder, which was subsequently ported.
