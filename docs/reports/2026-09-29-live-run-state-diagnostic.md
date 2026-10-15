# 游戏内连续整局对拍：Neow 至第二回合

日期：2026-09-29。分支：`feat/live-run-state-diagnostic`。正版游戏 v0.111.0，隔离游戏目录 `/home/wst/Desktop/MCC/sts2sim/artifacts/legacy/live-diff-source/`。

## 实现

- C ABI 增加按角色名称正常开局、OracleRun.DumpRun 格式的整局诊断导出、事件私有 RNG 计数导出。
- Mod 在首房间建立连续 Env；对比整局状态及事件选项键、顺序、事件私有 RNG。订阅真实 `EventOption.BeforeChosen`，把同一索引送入 Env；订阅真实 `MoveToMapCoordAction`，把同一坐标送入 Env。进入战斗后沿用这个 Env 后继，不再按 seed 和遭遇另起一个战斗实例。
- 后继进入 `Choice` 时记录 `run_choice_pending` 或 `combat_choice_pending` 并停止当前对拍，避免把尚未传递的游戏选择算成成功。事件页变化后的第一帧重新对比；房间进入后再对比整局状态。
- 真实游戏里若连续 Env 已停用，下一场战斗记录 `run_lineage_unavailable` 并停止对拍；按 seed/遭遇重建战斗仅允许显式独立调试探针，不能把丢失的整局历史伪装成连续成功。

## 实机证据

- `7155534B`：Neow 选 Winged Boots、实际地图移动、下一房间真实 Strike，整局及完整战斗状态各边界一致；`trace-continuous-025.jsonl`。
- 新随机种子 `C60BA09B`：同一路径 8 个整局/战斗边界一致；`trace-continuous-026.jsonl`。
- 新随机种子 `67C65245`：Neow 的 Winged Boots 在选项 0，事件私有 RNG 为游戏/Env `18/18`；地图移动、Strike、EndTurn 和第二玩家回合起点全部一致；`trace-continuous-030.jsonl`。
- 新随机种子 `4CA97550`：Winged Boots 在选项 1，事件私有 RNG 为游戏/Env `19/19`；事件、地图、Strike 均一致；`trace-continuous-029.jsonl`。
- 加入连续性门禁后，`C60BA09B` 复跑至 EndTurn 和第二回合，10 个整局/战斗边界全部一致；`trace-continuous-031.jsonl`。独立调试路径 `78A3E971` 仍完成 Strike、EndTurn 的 4 个战斗边界；`trace-standalone-032.jsonl`。
- `NE0WD1FF22` 的 Neow Small Capsule 进入额外选择。Mod 明确记录 `run_choice_pending`、`choice_bridge_not_implemented`；自动探针等待 600 帧后以 `TimeoutException` 收尾，游戏没有因差分工具被修改；`trace-choice-028.jsonl`。这不是成功样本。

原始 JSONL 和 Godot 日志只保存在 `/home/wst/Desktop/MCC/sts2sim/artifacts/legacy/live-diff-runtime/`，不提交游戏文件或原始日志。

## 验证与边界

`cmake --build build --target sts2env_c -j4` 和 `~/.dotnet/dotnet build tools/Sts2LiveDiff/Sts2LiveDiff.csproj -c Release --verbosity quiet` 均成功，Mod 构建 0 警告、0 错误。`STS2_SRC='/home/wst/Desktop/MCC/Slay the Spire 2/src/Core' scripts/check.sh` 完整通过：34/34 CTest，L2 200/200 开局，L3 80/80 战斗，L4 80/80 第一幕路线，以及定向二幕、三幕和献祭路线均零分歧。原始输出位于本机 `artifacts/legacy/live-diff-runtime/check-live-run.log`。门禁后的 C# 小改再次构建并完成上述两条实机回归；仓库检查脚本不编译此 Mod。

当前路径使用本机普通档案，但该档案的全解锁进度与 Env 的 veteran 基准兼容。尚未接入真实未全解锁档案、事件内部选牌/奖励选择、战斗结束奖励及下一地图节点、商店/休息/宝箱、存档恢复；战斗结束后还未把 Env 所有权交回整局路由。对拍仅证明上述采样路径，不证明整局或全部事件一致。Neow 原版选项中的需要选择遗物仍须接选择桥。
