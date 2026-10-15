# 计划：第一幕剩下的 5 个常规/精英遭遇（M2）

执行者：Codex（luna）。分支：`feat/act1-remaining-encounters`（基于 `feat/run-skeleton`）。审阅与合入：Claude。

## 背景

- 第一幕 28 个常规/精英遭遇里 23 个已经 L3 零分歧。剩下 5 个之前因为缺机制被跳过（见 `docs/reports/2026-09-24-act1-*-monsters.md` 的“跳过项”）：
  - `FOGMOG_NORMAL`：`Fogmog.IllusionMove` 召唤 `EyeWithTeeth`（`CreatureCmd.Add<T>`），`IllusionPower`，`DistractMove` 加 `Dazed`。
  - `VINE_SHAMBLER_NORMAL`：`TangledPower` 用 `CardCmd.Afflict<Entangled>`。
  - `LIVING_FOG_NORMAL`：`LivingFog.BloatMove` 召唤 `GasBomb`；`SmoggyPower` 用 `Smog` 苦难。
  - `TWO_TAILED_RATS_NORMAL`：`TwoTailedRat.CallForBackup` 召唤并更新 `CallForBackupCount`。
  - `PHROG_PARASITE_ELITE`：`Infection` 状态牌；`InfestedPower.AfterDeath` 召唤 4 个 `Wriggler`。
- 这些机制现在都有了：
  - 召唤：`CreatureCmd::Add(MonsterModel, CombatState, side, slotName)`、`CreatureCmd::Add(Creature)`、`CombatState::CreateCreature`（样板：`game/Models/Powers/SurprisePower.h`、`game/Models/Monsters/GremlinMerc.h`）。
  - 苦难：`AfflictionModel`（`game/Models/AfflictionModel.h`）、`CardCmd::Afflict(AfflictionId, card, amount)`、`AfflictAndPreview`、`ClearAffliction`、`CardModel::Affliction()`、`AfflictionModel::PickRandomTargets`。`Entangled`、`Smog` 没有自己的逻辑，不需要头文件；逻辑在对应的能力里。
  - 状态牌：样板 `game/Models/Cards/Slimed.h`。

## 任务与规则

照 `docs/plans/2026-09-24-m1-act1-monsters.md` 的“每个遭遇的步骤”“规则与边界”“交付”执行（逐行移植、`gen_dispatch.py`、构建、A0/A10 各 16 场 L3 全部一致、每个遭遇一个提交）。补充：

- 召唤出来的怪物也要完整移植（它们的招式、能力）。
- 如果 L3 出现分歧并且首因在框架（召唤排序 `SortEnemiesBySlotName`、苦难、死亡结算）而不在内容：先对照 C# 找首因；是框架 bug 就最小修复，并在报告里单列（文件、方法、C# 位置、修复前后的对拍结果）。
- 报告：`docs/reports/2026-09-25-act1-remaining-encounters.md`。

## 验收

- 5 个遭遇 A0/A10 各 16/16，或者报告里写明卡在哪个 C# 方法上。
- `scripts/check.sh` 全部通过。
