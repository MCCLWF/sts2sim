# 实机连续选卡：卡牌奖励与 Skip

日期：2026-09-29。分支：`feat/live-card-reward-choice`。前置：`main` 的 Neow → 首战 → 奖励 → 下一节点连续对拍已通过。游戏为正版 v0.111.0，隔离进程使用正常档案。

## 实现

- `Sts2LiveDiff` 在真实游戏 `RewardsSetSynchronizer.SelectLocalReward` 后保留 Env 的 pending Choice；在 `PlayerChoiceSynchronizer.SyncLocalChoice` 捕获玩家提交的真实索引。先比较屏幕中的卡牌与额外选项顺序、Env 合法答案，再把同一答案交给 Env 的 `Predict`。没有触及原游戏的选择结果或玩家状态。
- CardReward 选择完成后比较整局状态；Skip 返回 `false` 时也保留 Env 连续性并比较状态。其他未接通的 Choice 仍明确阻断当前对拍。
- C ABI 奖励诊断在原有奖励内容外导出每条 `SuccessfullySelected`。游戏显示、领金币、选卡或 Skip 后均比较奖励内容和领取标志，避免只凭牌组或金币变化判断成功。
- 开发探针通过真实游戏奖励选择屏的卡牌信号或 Skip 处理器触发选择，然后继续真实地图移动。探针不直接改牌组或资源。

## 实机证据

原始 JSONL 仅在 `/home/wst/Desktop/MCC/sts2sim/artifacts/legacy/live-diff-runtime/`：

| Trace | 种子与动作 | 结果 |
|---|---|---|
| `trace-card-039.jsonl` | `C60BA09B`，首战后选 Cinder | 41 个比较点零差异；牌组增加 Cinder |
| `trace-card-040.jsonl` | 同种子选 True Grit，继续至 Sunken Statue | 46 个比较点零差异 |
| `trace-card-041.jsonl` | 同种子选 Skip，继续至 Sunken Statue | 46 个比较点零差异；卡牌奖励未标记领取 |
| `trace-card-042.jsonl` | `67C65245`，首战后选 Twin Strike，继续至 The Legends Were True | 50 个比较点零差异；奖励领取标志一致 |
| `trace-card-043.jsonl` | `4CA97550`，首战后选 Skip，继续至 Brain Leech | 48 个比较点零差异；奖励领取标志一致 |

以上比较点包含完整战斗、整局状态、事件选项、奖励条目及卡牌候选。后两个新种子均无 mismatch、observer error 或 Env block。旧种子 039–041 运行于新增领取标志比较之前；042–043 已覆盖新增比较。

额外真正随机抽取的 `054C276A`（`trace-card-044.jsonl`）在 Neow 第一选项 Lost Coffer 进入尚未接通的选择，记录 `run_choice_pending` 和 `choice_bridge_not_implemented`；探针等待真实游戏输入超时。该样本未到战斗或奖励，不计为成功，也没有通过自动换选项掩盖边界。

## 验证与限制

`cmake --build build --target sts2env_c -j4` 成功；`~/.dotnet/dotnet build tools/Sts2LiveDiff/Sts2LiveDiff.csproj -c Release` 为 0 警告、0 错误。`STS2_SRC='/home/wst/Desktop/MCC/Slay the Spire 2/src/Core' scripts/check.sh` 完整通过：34/34 CTest，L2 200/200 开局，L3 第一幕战斗两组各 40/40，以及 L4 第一幕、二幕、三幕和定向遗物路线。原始检查日志为 `artifacts/legacy/live-diff-runtime/check-live-card-final.log`。

实机只覆盖普通档案下三条种子的首战卡牌奖励。遗物、药水、特殊卡牌和删牌奖励的 UI 选择，重掷与多轮卡牌选择、战斗内及事件内其他 pending Choice 尚未连续验证；实际未全解锁档案、完整三幕真机跑图和 UI 搜索模式也未完成。这些有限样本不能证明所有卡牌奖励或整局均一致。
