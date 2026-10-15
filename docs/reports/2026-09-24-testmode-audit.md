# TestMode 读取点审计执行报告

## 前置条件

- 分支：`docs/testmode-audit`。
- 真值源码：`STS2_SRC=/home/wst/Desktop/MCC/Slay the Spire 2/src/Core`，只读，未修改。
- 已阅读 `AGENTS.md` 以及计划、路线图、设计、坑点清单和偏离登记册。
- 计划要求不改代码；本次只写审计、偏离、路线图/README 和报告文档。
- 接手中断工作时，`git status --short --branch` 只有未跟踪的 `docs/audits/testmode.md`；没有发现用户已有的修改。

## 验收命令与原始结果

### TestMode 读取点

命令：

```bash
cd "$STS2_SRC"
rg -n --glob '*.cs' -g '!Nodes/**' -g '!TestSupport/**' 'TestMode\.(IsOn|IsOff)' . | wc -l
```

结果：

```text
136
```

`docs/audits/testmode.md` 的逐点表包含 136 行，分类为 V 106、T 18、P 4、G 8，与 grep 总数一致。

### NonInteractiveMode 读取点

命令：

```bash
cd "$STS2_SRC"
rg -n --glob '*.cs' -g '!Nodes/**' 'NonInteractiveMode\.IsActive' . | wc -l
```

结果：

```text
18
```

逐点表记录 18 处：V 17、T 1。

### 文档差异检查

命令：

```bash
git diff --check
```

结果：无输出，检查通过。

## G 类结论

- 已在 `ProductionBranches` 恢复：`MerchantPotionEntry.CalcCost`、`CallingBell.GenerateRewards`、`Cauldron.GenerateRewards`。
- 建议加入 `ProductionBranches`：`RunManager.UpdatePlayerStatsInMapPointHistory`。该正式分支只更新地图历史中的金币/生命统计，没有节点或 UI 依赖，可以直接使用现有 TestMode transpiler。
- 不建议直接加入：`CreatureCmd.Kill`、`RewardsSet.Offer`、`CardSelectCmd.FromChooseABundleScreen`、`RollingBoulderPower.AfterPlayerTurnStart`。它们分别依赖死亡结算/UI、奖励选择 UI、卡包本地/远程选择、滚石 VFX 命中信号，已登记为 O-007 至 O-010，并给出后续拆分或 headless 适配建议。

## 已知限制

- 本次审计没有运行 `scripts/check.sh` 或 L3 对拍；计划明确不改代码，且本次验收是源码读取点计数和逐点分类。O-001、O-003、O-005 以及 O-006 仍需后续 L3/真机对拍验证。
- `Nodes/` 和 `TestSupport/` 按计划排除；`NonInteractiveMode.IsActive` 则按计划扫描了 `Nodes/` 以外的全部源码。

## 提交尝试

沙箱允许修改工作区文件，但 worktree 的 Git 元数据位于 `/home/wst/Desktop/MCC/sts2sim/.git/worktrees/docs-testmode-audit/`，该路径只读。`git add` 因无法创建 `index.lock` 失败，`git commit` 因无法写 `COMMIT_EDITMSG` 失败；用工作区外的备用索引创建的提交对象为 `8720605e20edb17d47bb81441e7fa6ca47340c2b`，更新 `refs/heads/docs/testmode-audit` 时又因无法创建 ref lock 被拒绝。因此当前工作区文档已完成，但真实分支尚未写入该提交。
