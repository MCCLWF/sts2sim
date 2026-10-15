# 计划：TestMode 读取点全量审计（M1-b 附属）

- 执行者：Codex（gpt-6-luna，xhigh）
- 分支：`docs/testmode-audit`
- 路线图：M1-b（oracle 战斗记录）。本计划为 oracle 的保真提供证据。

## 背景

oracle（`tools/Sts2Oracle`）现在和游戏自己的测试运行器一样，打开 `TestMode.IsOn` 来运行游戏逻辑，这样所有节点、特效、音频、资源加载都由游戏自己跳过。但有一部分 `TestMode` 读取点改变的是**游戏逻辑**，不只是表现层。目前已知并已处理的有三处，它们在 `tools/Sts2Headless/Headless.cs` 的 `ProductionBranches` 里用 transpiler 恢复成了正式版分支：

- `MerchantPotionEntry.CalcCost`：正式版会用 `Shops` 流抽价格浮动。
- `CallingBell.GenerateRewards`：TestMode 下给固定遗物。
- `Cauldron.GenerateRewards`：TestMode 下给固定药水。

本计划要把**所有**读取点审计一遍，确认没有遗漏。

反编译源码：`$STS2_SRC`，默认是 `/home/wst/Desktop/MCC/Slay the Spire 2/src/Core`。**只读，不要修改。**

## 任务

1. 列出 `src/Core` 下所有 `TestMode.IsOn`、`TestMode.IsOff` 的读取点，排除 `Nodes/` 和 `TestSupport/` 目录。预计约 137 处，以实际 grep 结果为准，报告里写明所用命令和总数。
2. 逐个阅读上下文，把每一处归入下面一类：
   - **V 表现层**：节点、特效、动画、音频、资源、日志、存档 IO、平台接口、多人同步。跳过它不影响游戏状态。
   - **T 测试接口**：只在测试里调用的 setter、断言、`throw`。正常流程不会走到。
   - **P 档案与引导**：新手引导、未发现的幕、首局强制事件等。TestMode 下的行为等价于"老玩家存档"，与偏离 D-001（全解锁存档假设）一致。
   - **G 游戏逻辑**：TestMode 分支改变了游戏状态、RNG 抽取、奖励内容、选择结果或结算顺序。
3. 对每一处 G 类，写清楚三件事：
   - TestMode 下怎么做，正式版怎么做。
   - 是否已在 `ProductionBranches` 里恢复。
   - 如果没有恢复：能否用同样的 transpiler 恢复（即正式分支里没有节点或 UI 依赖）；不能的话，说明原因和建议的处理方式。已知需要重点看的有 `RollingBoulderPower`（正式版由特效信号驱动伤害）、`CardSelectCmd` 的卡包选择（TestMode 总是选第一个包）、`RewardsSet`（TestMode 自动领取全部奖励或调用 `testSelector`）。
4. 另外列出 `NonInteractiveMode.IsActive` 在 `Nodes/` 以外的读取点，同样分类。TestMode 下它恒为 true。
5. 写审计文档 `docs/audits/testmode.md`：
   - 开头是汇总：各类的数量、G 类清单。
   - 然后是完整表格，每行一处：`文件:行号`、所在方法、类别、说明。
6. 更新 `docs/audits/deviations.md` 的 B 节：
   - 按"oracle 打开 TestMode，只恢复少数正式分支"的现状改写 O-001、O-003、O-005。之前手写的视觉补丁已经删除；`wip/oracle-combat` 分支已改名为 `feat/oracle-combat` 并实现了这些内容，删掉那些"wip 分支"的标注。
   - G 类里没法恢复的，逐条登记为新的 O 编号。
7. 写执行报告 `docs/reports/2026-09-24-testmode-audit.md`。

## 验收

- 表格行数与 grep 总数一致，报告里附上 grep 命令和输出的总数。
- 每个 G 类条目都有明确结论：已恢复、可恢复（建议加进 `ProductionBranches`）或不可恢复（已登记 O 编号）。

## 不做什么

- **不改任何代码**。需要改 `ProductionBranches` 的，在报告里列出建议，由维护者决定。
- 不审计 `Nodes/` 目录下的 TestMode 读取点（纯 UI）。
