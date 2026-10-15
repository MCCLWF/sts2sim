# 计划：移植实体类与模型基类（M1-d，Codex A）

- 执行者：Codex（gpt-6-sol，high）
- 分支：`feat/port-entities`，从 `feat/combat-engine` 开出
- 路线图：M1-d。与另外两个计划并行：`2026-09-24-m1d-port-commands.md`（Codex B）、`2026-09-24-m1d-port-hooks.md`（Codex C）

## 背景

C++ 引擎的框架已经就位，**动手前必须读完** [`docs/design-engine.md`](../design-engine.md)（移植约定）和 `AGENTS.md`：
- `game/` 目录与 C# 源码 `src/Core` 逐文件对应。
- 引擎类是"句柄"：`{Sim*, 池下标}`，数据在 `engine/sim/state.h` 的 `GameState` 里。
- 句柄类的头文件已经写好，声明了移植后的接口，但**大多数方法还没有实现**。本计划就是把实现补上：逐个打开对应的 C# 文件，逐行翻译。

C# 源码：`$STS2_SRC`（只读）。

## 文件归属

本计划**只**修改下列文件，并可以新建对应的 `.cpp`：

| C++ | C# 源 |
|---|---|
| `game/Entities/Creatures/Creature.{h,cpp}` | `Entities/Creatures/Creature.cs` |
| `game/Entities/Players/Player.{h,cpp}` | `Entities/Players/Player.cs`（战斗相关部分，以及遗物、药水的增删） |
| `game/Entities/Players/PlayerCombatState.{h,cpp}` | `Entities/Players/PlayerCombatState.cs` |
| `game/Entities/Cards/CardPile.{h,cpp}` | `Entities/Cards/CardPile.cs` |
| `game/Combat/CombatState.{h,cpp}` | `Combat/CombatState.cs`，**除了** `IterateHookListeners` 和 `Contains`（归 Codex C，放在 `CombatState.Hooks.cpp`） |
| `game/Runs/RunState.{h,cpp}` | `Runs/RunState.cs` 中战斗用到的部分，**除了** `IterateHookListeners`（归 Codex C） |
| `game/Combat/History/*` | `Combat/History/CombatHistory.cs` 与 `Entries/*.cs`：记录存在 `CombatData` 里（可以在 `engine/sim/state.h` 的 `CombatData` 末尾加字段） |
| `game/Models/CardModel.{h,cpp}` | `Models/CardModel.cs`（非 UI 部分）、`Entities/Cards/CardEnergyCost.cs`、`LocalCostModifier.cs`、`TemporaryCardCost.cs` |
| `game/Models/PowerModel.{h,cpp}` | `Models/PowerModel.cs` |
| `game/Models/RelicModel.{h,cpp}`、`PotionModel.{h,cpp}`、`EnchantmentModel.{h,cpp}` | 对应的 C# 基类（战斗相关部分） |
| `game/Localization/DynamicVars/*` | `Localization/DynamicVars/DynamicVar.cs`、`DynamicVarSet.cs` 及各 `*Var.cs` 中影响数值的逻辑 |
| `engine/sim/state.h` | 只允许在结构体**末尾**加字段；改动写进报告 |

各基类 `SIM_MEMBER_CUSTOM` 成员的手写默认实现 `Xxx_Default`，也在对应的 `.cpp` 里写（例如 `CardModel::OnUpgrade_Default`、`CardModel::TargetType_Default`）。

**不要改**：`game/Hooks/*`、`game/Commands/*`、`game/Combat/CombatManager.*`、`game/Models/MonsterModel.*`、`game/MonsterMoves/*`、`game/generated/*`、`*.def`、`tools/gen/*`。

- 需要调用 Commands（`CardPileCmd::Add` 等）或 `Hook::*` 时：
  - 照 C# 写调用，并 `#include "game/Commands/<Name>.h"` 或 `"game/Hooks/Hook.h"`。
  - `Hook.h` 已经存在。Commands 的头文件由 Codex B 同时在写，你这边可能还没有，因此这类调用会暂时编译不过。
  - 处理办法：在调用处上方加注释 `// PORT(commands): CardPileCmd.Add(...)`，并用 `#if 0 ... #endif` 包住调用，保证能编译。报告里列出所有这样的位置，合并后由维护者打开。
- 需要调用 `CombatManager` 时，只能用 `game/Combat/CombatManager.h` 里已经声明的方法。

## 翻译规则摘要（详见 design-engine.md）

- 属性写成同名无参方法：`card.Owner.Creature` → `card.Owner().Creature()`。
- `async`/`await` 全部去掉；`Task.WhenAll` 按列表顺序执行。
- `decimal` 用 `Dec`，`(int)decimal` 用 `.to_int()`，字面量用 `Dec(3)` 或 `Dec::lit(15, 1)`。
- LINQ：保留顺序；物化的查询（`ToList()`）先拷贝；排序用 `std::stable_sort`（`OrderBy`）或 `dotnet_sort`（`List.Sort`）。
- 泛型按 id 写：`HasPower<VulnerablePower>()` → `HasPower(PowerId::VULNERABLE_POWER)`，`ModelDb.Card<X>()` → `CardId::X`。
- 视觉、音效、日志、多人同步代码删除。删除前确认它不读写游戏状态。
- C# 的异常检查保留，抛 `GameError`。
- 每个方法开头一行注释：对应的 C# 文件。

## 任务

1. 逐个实现上表中所有已声明、但尚未实现的方法。
2. C# 类里还有战斗逻辑会用到、但头文件没有声明的公开成员，按约定补上声明和实现。
3. `CardModel::UpgradeInternal` 和 `DynamicVar::SetBaseValue` 要处理卡牌变量的覆盖值：见 `CardState::var_overrides` 的注释，以及 `data(id).levels[n].vars`。
4. 构建：`cmake --build build --target sts2game` 必须没有错误；`scripts/check.sh --quick` 必须通过。
5. 写执行报告 `docs/reports/2026-09-24-m1d-port-entities.md`，包括：
   - 实现了哪些文件和方法；
   - 所有 `PORT(commands)` 占位的位置；
   - 有意跳过或简化的地方，以及理由；
   - 对 `state.h` 的改动。

## 验收

- `cmake --build build --target sts2game` 没有错误；`scripts/check.sh --quick` 通过。
- 头文件声明的方法都有实现。确实做不了的（依赖未移植的类型），在报告里逐条列出。
- 不改归属表以外的文件。

## 不做什么

- 不移植 `CombatManager`、怪物状态机、Hook 分发、Commands，也不移植具体的卡牌、能力、怪物。
- 不写单元测试。验证靠之后的 L3 对拍。
