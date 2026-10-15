# 计划：oracle 的 TalkCmd 无头补丁，以及被它挡住的 4 个遭遇

执行者：Codex（luna）。分支：`feat/talkcmd-encounters`。

## 背景

- 第一幕怪物移植（`docs/reports/2026-09-24-act1-*-monsters.md`）里，有 4 个遭遇被跳过：
  - CULTISTS_NORMAL、SEAPUNK_NORMAL、GREMLIN_MERC_NORMAL、BYGONE_EFFIGY_ELITE。
  - 原因是 oracle 在无头进程里执行 `TalkCmd.Play`（怪物说话的气泡）时空引用崩溃。
- `TalkCmd` 是纯表现层代码。

## 任务

1. 读 `Commands/TalkCmd.cs`，确认它只做表现（气泡节点、音效、等待），不读写任何游戏状态、不抽随机数。**如果发现它影响逻辑，停下来在报告里说明，不要打补丁。**
2. 在 `tools/Sts2Headless/Headless.cs` 用 Harmony 跳过 `TalkCmd.Play` 的各个重载（返回已完成的 Task 或等价的空结果），照现有补丁的写法，并在代码注释里写明为什么不影响逻辑（AGENTS.md §4）。在 `docs/audits/deviations.md` 登记为 oracle 侧偏离（沿用现有 O-xxx 编号往后排）。
3. 按 `docs/plans/2026-09-24-m1-act1-monsters.md` 的步骤和规则，移植并对拍上面 4 个遭遇（A0、A10 各 16/16）。C++ 里 `TalkCmd` 不需要移植（纯表现）。
4. 报告 `docs/reports/2026-09-24-talkcmd-encounters.md`：补丁的理由、每个遭遇的结果、原始对拍输出。

## 验收

- `scripts/check.sh` 全部通过（补丁不能改变 L2、L3 的结果）；
- 4 个遭遇 A0/A10 各 16/16，或者在报告里写明卡在哪个 C# 方法上。
