# 计划：L3 自定义牌组、遗物与战斗内选牌（M2 卡牌移植的验收工具）

执行者：Codex（sol）。分支：`feat/l3-custom-decks`（基于 `feat/run-skeleton`）。审阅与合入：Claude。

## 背景

- L3（`tests/l3/l3_combat.py`）目前只能用角色的起始牌组打一场战斗。移植铁甲战士全部卡牌（M2）需要：
  - 用**指定的牌组**（和可选的额外遗物）打指定遭遇，让新卡牌大量出现在手牌里；
  - 战斗中需要玩家选牌的卡（`CardSelectCmd.FromHand` 等）也能对拍。
- 模拟器侧的选牌已经就绪：`game/Commands/CardSelectCmd.{h,cpp}` 通过 `Sim::Choose` 抛 `NeedChoice`，`env/env.{h,cpp}` 用前缀重放回答；选牌编码见 `CardSelectCmd.h` 顶部注释（先答数量，再按选择顺序答每个下标；奖励三选一是单个答案）。
- oracle 整局记录（`tools/Sts2Oracle/OracleRun.cs`）已经有一个 `ICardSelector`（`Controller`），可以照搬它的决策/动作 JSON 格式。

## 任务

1. **oracle `combat` 命令**（`tools/Sts2Oracle/OracleCombat.cs`）：
   - `--deck ID[+N],ID[+N],...`：进入战斗前把玩家牌组替换成这些卡（`+N` 为升级次数），顺序照给出的顺序。用游戏自己的 API 建卡、加入牌组（例如 `RunState.CreateCard` + `CardPileCmd.Add(..., PileType.Deck)` 或者直接操作 Deck 的等价方法）；在报告里写明用了哪个 API、为什么它不改变随机数流（如果改变了，C++ 侧必须做完全相同的调用）。
   - `--relics ID,ID,...`：额外获得这些遗物（`RelicCmd.Obtain`），在起始遗物之后。
   - 给战斗加一个 `ICardSelector`（`CardSelectCmd.UseSelector`），决策和动作的 JSON 与 `OracleRun.Controller` 相同（`card_select` 决策，`indices` 动作），策略用同一个 policy RNG。
   - 不带新参数时，输出必须与现在**逐字节相同**（L3 基线不许变）。
2. **C++ `combat` 命令**（`tools/cli/combat_cmd.cpp`）：
   - 同样的 `--deck`、`--relics`，做与 oracle 完全相同的调用（同样的顺序、同样的 RNG 消耗）。
   - 动作里加入选牌：例如 `s<n>:<i1>.<i2>...`。重放时用 `Sim` 的前缀机制（`set_prefix`）或改用 `env/Env` 驱动，二选一，说明理由。
   - 选牌决策点输出与 oracle 相同的 JSON，供比对。
3. **`tests/l3/l3_combat.py`**：透传 `--deck`、`--relics`；把 oracle 的选牌动作翻译成 C++ 的动作格式；比对 `card_select` 决策（`options.source` 字段不比，理由见 `tests/l4/l4_run.py`）。
4. **自测**：
   - `scripts/check.sh` 全部通过（起始牌组的 L3 不变）。
   - 用已移植的卡组一个自定义牌组，例如 `--deck STRIKE_IRONCLAD+1,STRIKE_IRONCLAD,DEFEND_IRONCLAD+1,DEFEND_IRONCLAD,BASH+1,BASH`，对 3 个已移植遭遇（如 `SLIMES_WEAK`、`NIBBITS_WEAK`、`CULTISTS_NORMAL`）A0/A10 各 16 场全部一致。
   - 含未移植卡牌的牌组：C++ 报 `unported` 并干净退出，`l3_combat.py` 把它计为 unported 而不是 diverged。
5. 更新 `docs/reports/2026-09-25-l3-custom-decks.md`（命令与原始输出）、`README.md` 里 L3 的用法。

## 规则

- 先读 `AGENTS.md`、`docs/design-engine.md` §6（选择前缀）、`docs/audits/pitfalls.md`。
- 不改 `game/` 下的游戏逻辑。如果发现选牌机制在 C++ 侧有 bug，最小修复并在报告里单列（文件、方法、对应的 C# 位置）。
- 对拍分歧查模拟器，不改参照、不放宽比对。
- 每完成一步提交一次，提交信息 `<类型>: <描述>`。
