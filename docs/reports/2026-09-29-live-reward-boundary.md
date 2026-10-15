# 实机连续路径：战斗奖励至下一地图事件

日期：2026-09-29。分支：`feat/live-reward-boundary`。正版游戏 v0.111.0，隔离游戏目录 `/home/wst/Desktop/MCC/sts2sim/artifacts/legacy/live-diff-source/`。

## 实现

- C ABI 提供当前奖励集合的诊断导出，复用现有 `OracleRun.RewardJson` 格式。Mod 在游戏实际显示战斗奖励时，将连续战斗 Env 后继交回整局路由，同时比较整局状态、奖励顺序与内容。
- 监听游戏原有的 `SelectLocalReward` 与 `ProceedFromTerminalRewardsScreen`。玩家领赏和返回地图时，将同一动作送入 Env，等待游戏动作完成后比较状态。若 Env 要求尚未接通的选择，记录并停止该条对拍。
- 开发探针使用游戏动作自然打完首战、领取金币、跳过其余奖励、返回地图并移动到下一节点；不强制结束战斗或直接改写玩家状态。

## 实机证据

种子 `C60BA09B`、普通档案、铁甲战士：Neow 获得 Winged Boots，真实地图移动进入 Nibbits Weak 战斗；13 个捕获战斗动作后，双方生成相同的 13 金币与三张候选卡牌（Cinder、True Grit、Pommel Strike）。游戏通过原有奖励入口领取金币，金币从 99 增至 112，Env 同样；随后跳过剩余奖励，Env 决策回到地图。真实移动至 `[2,2]` 后，双方进入 Sunken Statue，事件选项键与顺序一致。

最终样本 `trace-reward-038.jsonl` 共 118 条记录：25 次完整战斗比较、13 次整局比较、4 次事件选项比较、1 次奖励列表比较，全部零差异；无观察器异常和阻断。`trace-reward-035.jsonl`、`036`、`037` 分别验证奖励展示、领取金币、返回地图的递进边界。原始 JSONL 和游戏日志只留在 `artifacts/legacy/live-diff-runtime/`，不入 Git。

## 验证与限制

`cmake --build build --target sts2env_c -j4` 成功；`~/.dotnet/dotnet build tools/Sts2LiveDiff/Sts2LiveDiff.csproj -c Release` 0 警告、0 错误。`STS2_SRC='/home/wst/Desktop/MCC/Slay the Spire 2/src/Core' scripts/check.sh` 完整通过，含 L2 200 局、L3 战斗和 L4 第一幕 80 局及二、三幕定向路线；原始输出在 `artifacts/legacy/live-diff-runtime/check-live-reward.log`。检查脚本构建原版 Oracle 时有一条已有的 C# 可空参数警告，未影响通过。最后的 Mod 探针增量只改 C#，已再次单独构建并实机验证。

该样本只覆盖首战金币奖励和剩余卡牌奖励的跳过路径。选卡、遗物、药水奖励的游戏内选择与需要额外输入的分支还未闭环；尚无完整真实三幕跑图、两模式 UI 或 CombatSolver 搜索替换。因此这只是连续边界的实机证据，不是完整替代证明。
