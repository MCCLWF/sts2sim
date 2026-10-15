# 实机 Neow 嵌套奖励选择

日期：2026-09-29。分支：`feat/live-custom-reward-choice`。起点是随机种子 `054C276A` 的 Lost Coffer；此前实机桥在 `RewardsCmd.OfferCustom` 的 pending Choice 受控停止。

## 实现

- Mod 捕获非终结 `NRewardsScreen.ShowScreen`，在 Env 的 `Choice.source == rewards` 下比较当前整局与奖励条目，包括领取状态。游戏 `SelectLocalReward` 的实际条目索引对应 Env `Answer`，内层卡牌选择仍由游戏 `SyncLocalChoice` 的实际答案继续前缀重放。
- 游戏显式 Skip 剩余奖励时，捕获 `SkipLocalRewardsSet` 并发送 Env 的 proceed 答案；全部奖励领取后游戏自动完成时，捕获 `CompleteRewardsSetIfNecessary`，发送同一 Env 收束答案。两条出口都在下一帧比较整局。
- `sts2_env_export_oracle_rewards` 在普通奖励决策及来源为 `rewards` 的 pending Choice 上提供同样的诊断数据。对其他 Choice 不改变支持声明。
- 开发探针通过原有奖励同步器、卡牌界面信号和奖励屏 Proceed 处理器完成动作，没有直接修改游戏牌组、药水或 RNG。

## 实机结果

| 原始 trace（仅本机 `artifacts/legacy/live-diff-runtime/`） | 路径 | 比较结果 |
|---|---|---|
| `trace-custom-045.jsonl` | `054C276A` Lost Coffer，领 Speed Potion 后 Skip，进入下一战斗入口 | 15/15 一致 |
| `trace-custom-046.jsonl` | 同种子选 Sword Boomerang 后 Skip，进入下一战斗入口 | 16/16 一致 |
| `trace-custom-047.jsonl` | 同种子选 Sword Boomerang、再领 Speed Potion，游戏自动完成，进入下一战斗入口 | 19/19 一致 |
| `trace-custom-048.jsonl` | 改动后回归领药水后 Skip | 15/15 一致 |
| `trace-custom-049.jsonl` | 新随机抽取的 `E3D201F0`，Lost Coffer 位于 Neow 选项 1，选卡再领药水、自动完成，进入下一战斗入口 | 19/19 一致 |

上述比较涵盖事件候选顺序、整局状态、嵌套奖励条目与领取状态、卡牌候选和实际玩家索引。五条 trace 无 mismatch、observer error 或 Env block。原始 JSONL 与 Godot 日志没有提交。

## 验证与限制

`cmake --build build --target sts2env_c -j4` 成功；`~/.dotnet/dotnet build tools/Sts2LiveDiff/Sts2LiveDiff.csproj -c Release` 为 0 警告、0 错误。`STS2_SRC='/home/wst/Desktop/MCC/Slay the Spire 2/src/Core' scripts/check.sh` 全部通过，含 34/34 CTest、L2 200/200、L3 两组各 40/40 及 L4 第一幕、二幕、三幕定向路径；原始输出在 `artifacts/legacy/live-diff-runtime/check-live-custom.log`。最后的“全领取”桥接只修改 C# Mod，已单独构建并在 `047`、`049` 两条实机路径验证。

当前证明的是两种种子下的 Lost Coffer、其药水和卡牌奖励以及返回地图。其他产生嵌套奖励的遗物与事件、满药水栏的替换选择、重掷、特殊卡和删牌等未逐项实机验证；完整单人三幕与真实未全解锁档案仍缺，不能把这批样本推广成全部 Choice 已覆盖。
