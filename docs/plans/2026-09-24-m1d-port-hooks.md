# 计划：移植 Hook 分发（M1-d，Codex C）

- 执行者：Codex（gpt-6-sol，high）
- 分支：`feat/port-hooks`，从 `feat/combat-engine` 开出
- 路线图：M1-d。与 `2026-09-24-m1d-port-entities.md`（Codex A）、`2026-09-24-m1d-port-commands.md`（Codex B）并行

## 背景

**动手前必须读完** [`docs/design-engine.md`](../design-engine.md)（尤其第 7 节）和 `AGENTS.md`。

`Hooks/Hook.cs` 是游戏里所有"触发时机"的分发中心，一共约 150 个函数。每个函数做两件事：
- 按固定顺序遍历监听者（能力、遗物、卡牌……）；
- 调用它们的虚方法，并按规则汇总结果（例如伤害修正先加后乘、`Should*` 取与）。

分发的**顺序**和**遍历时的存活检查**，是整个模拟器逐位一致的基础。

`game/Hooks/Hook.h` 已经生成（`tools/gen/gen_hook_header.py`），声明了其中 114 个函数。剩下的等相关类型移植后再加。对具体模型虚方法的调用，走 `AbstractModel` 上已经生成的分发成员：`model.AfterAttack(ctx, command)`，见 `game/generated/Dispatch.cpp`。

C# 源码：`$STS2_SRC/Hooks/Hook.cs`、`Combat/CombatState.cs`、`Runs/RunState.cs`（只读）。

## 文件归属

新建并只修改：
- `game/Hooks/Hook.cpp`：`Hook.h` 里所有函数的实现，以及 `Hook.cs` 里的私有辅助函数。
- `game/Combat/CombatState.Hooks.cpp`：`CombatState::IterateHookListeners` 和 `CombatState::Contains`，逐行照抄 `CombatState.cs` 的同名方法。其余方法归 Codex A。
- `game/Runs/RunState.Hooks.cpp`：`RunState::IterateHookListeners`，照抄 `RunState.cs`。
- `tools/gen/gen_hook_header.py` 和 `game/Hooks/Hook.h`：只在类型映射需要修正时改，并重新生成。

**不要改**其他文件。需要实体句柄的方法而头文件里没有声明时：在类末尾的注释 `// --- needed by Hooks (port pending)` 下面加声明，实现归 Codex A，并在报告里列出。

## 要点

- **遍历语义**：
  - `IterateCombatHookListeners` 在 `CombatManager(&sim).IsOverOrEnding() && !IsStarting()` 时什么都不产出，而且只在开始时判断一次。
  - 监听者列表在遍历开始时生成快照（`HookListenerList`）。
  - 每一项在轮到它时调用 `Contains(model)`，不在战斗中就跳过。
  - 少数函数直接用 `IterateHookListeners`（不带战斗结束抑制），照抄 C#。
- **覆写检查**：分发成员内部已经会检查"该类是否覆写了这个成员"，你不用再判断。快照时可以按 `model.Overrides(Member::...)` 过滤，以减少开销；但这不能改变顺序和语义。
- **汇总规则**：`Modify*` 系列（加法修正、乘法修正、上限、`out` 列出做了修改的模型）、`Should*` 系列（任一返回 false 即 false，并通过 `out` 给出阻止者）、`TryModify*` 系列，全部逐行照抄，包括 early、normal、late 三档的顺序。
- `InvokeExecutionFinished()` 是 UI 事件，删除。

## 任务

1. 实现 `Hook.h` 里全部 114 个函数，以及两个 `IterateHookListeners` 和 `CombatState::Contains`。
2. 构建：`cmake --build build --target sts2game` 没有错误（其他人负责的方法此时可能还没有实现，但只编译库不会链接，所以不影响）；`scripts/check.sh --quick` 通过。
3. 写执行报告 `docs/reports/2026-09-24-m1d-port-hooks.md`，包括：
   - 实现了哪些函数；
   - 哪些函数不走战斗结束抑制；
   - 给别人的头文件加了哪些声明；
   - 与 C# 不一致的地方（应当没有）。

## 验收

- 编译通过，`Hook.h` 里每个函数都有实现，并且与 C# 逐行对应。

## 不做什么

- 不移植具体模型，不写 `CombatManager` 和 Commands。
- 不写单元测试。
