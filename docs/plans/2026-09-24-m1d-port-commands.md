# 计划：移植 Commands（M1-e，Codex B）

- 执行者：Codex（gpt-6-sol，high）
- 分支：`feat/port-commands`，从 `feat/combat-engine` 开出
- 路线图：M1-e。与 `2026-09-24-m1d-port-entities.md`（Codex A）、`2026-09-24-m1d-port-hooks.md`（Codex C）并行

## 背景

**动手前必须读完** [`docs/design-engine.md`](../design-engine.md) 和 `AGENTS.md`：
- `game/` 目录与 C# 的 `src/Core` 逐文件对应。
- 引擎类是句柄，数据在 `GameState` 里。
- 实体和模型的句柄头文件已经写好，只有声明。它们的实现由 Codex A 同时在写。

本计划把 `Commands/` 移植过来。这是游戏效果的执行层：伤害、格挡、能力、抽牌、洗牌、弃牌、消耗、选牌……每一张卡牌和每一个怪物招式都通过它们改变状态。

C# 源码：`$STS2_SRC/Commands/`（只读）。

## 文件归属

新建并只修改：

| C++ | C# 源 |
|---|---|
| `game/Commands/Cmd.{h,cpp}` | `Commands/Cmd.cs`（等待类全部变成空操作） |
| `game/Commands/CreatureCmd.{h,cpp}` | `Commands/CreatureCmd.cs` |
| `game/Commands/DamageCmd.{h,cpp}` | `Commands/DamageCmd.cs` |
| `game/Commands/Builders/AttackCommand.{h,cpp}`、`AttackContext.{h,cpp}` | `Commands/Builders/*.cs` |
| `game/Commands/PowerCmd.{h,cpp}` | `Commands/PowerCmd.cs` |
| `game/Commands/CardPileCmd.{h,cpp}` | `Commands/CardPileCmd.cs` |
| `game/Commands/CardCmd.{h,cpp}` | `Commands/CardCmd.cs` |
| `game/Commands/PlayerCmd.{h,cpp}` | `Commands/PlayerCmd.cs` |
| `game/Commands/CardSelectCmd.{h,cpp}` | `Commands/CardSelectCmd.cs` |
| `game/Commands/PotionCmd`、`RelicCmd`、`OrbCmd`（能写多少写多少） | 对应 C# |

实体或模型句柄缺少你需要的方法时：
- 可以在对应头文件的类**末尾**加声明，放在注释 `// --- needed by Commands (port pending)` 下面。
- 实现不归你，由 Codex A 负责。报告里列出你加了哪些声明。
- **不要**改那些头文件里已有的内容。

**不要改**：`game/Hooks/*`（`Hook.h` 已经存在，直接调用）、`game/Combat/CombatManager.*`、`game/Models/MonsterModel.*`、`game/generated/*`、`*.def`、`tools/gen/*`。

## 要点

- **流式 API**：`DamageCmd.Attack(x).FromCard(card, cardPlay).Targeting(t).WithHitCount(n).Execute(ctx)` 翻译成同名的 C++ 方法链，返回 `AttackCommand&`。视觉相关的方法（`WithHitFx`、`WithAttackerAnim`……）保留成空操作，这样卡牌代码可以逐行照抄。
- **伤害管线**：`AttackCommand.Execute` 和 `CreatureCmd.Damage` 的执行顺序、各 Hook 的调用时机、`decimal` 运算，都是逐位一致的关键。**一行都不能省**。
- **选择**：`CardSelectCmd` 的所有入口最终调用 `sim.Choose(ChoiceRequest)`（`engine/sim/sim.h`）：
  - 定义一个清楚的约定：选项顺序怎么排，多选怎么编码。写进 `CardSelectCmd.h` 的注释。
  - `CardSelectorPrefs` 这类参数照搬。
  - TestMode 分支按正式版逻辑移植（oracle 已在 TestMode 下恢复了几处正式分支，见 `docs/audits/testmode.md`）。
- **泛型**按 id 写：`PowerCmd.Apply<VulnerablePower>(...)` → `PowerCmd::Apply(PowerId::VULNERABLE_POWER, ...)`。
- **`CombatManager`**：只调用 `game/Combat/CombatManager.h` 已经声明的方法，构造方式是 `CombatManager(&sim)`。缺的在报告里列出。
- **`CombatHistory`**：记录调用（`History.CardPlayStarted(...)` 等）照写，调用 `game/Combat/History/CombatHistory.h`。缺的方法按上面的"末尾声明"规则添加（这个文件归 Codex A）。

## 任务

1. 按归属表逐文件移植。
2. 构建：`cmake --build build --target sts2game` 没有错误；`scripts/check.sh --quick` 通过。
3. 写执行报告 `docs/reports/2026-09-24-m1e-port-commands.md`，包括：
   - 移植了哪些文件和函数；
   - 选择的编码约定；
   - 给别人的头文件加了哪些声明；
   - 有意跳过或简化的地方。

## 验收

- 编译通过。
- `AttackCommand`、`CreatureCmd.Damage`、`CreatureCmd.GainBlock`、`PowerCmd.Apply`、`CardPileCmd.Draw`/`Add`/`Shuffle`、`CardCmd.Exhaust`、`CardCmd.Discard` 这些核心路径**完整移植**，没有省略步骤。

## 不做什么

- 不写 Hook 分发，不写 `CombatManager`，不写具体的卡牌、能力、怪物。
- 不写单元测试。
