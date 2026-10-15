# 实机首战奖励后的事件与第四层

日期：2026-09-29。分支：`feat/live-next-event-route`。前置：主分支已连续对拍 Neow、首战、奖励和第三层入口。

## 改动

开发探针在实际进入第三层事件后，通过游戏 `EventSynchronizer.ChooseLocalOption` 选择一个真实可用选项，等待其任务完成后比较 Env 同索引后继；事件完成时再通过游戏动作队列移动到下一地图点。事件内等待额外选择时，探针记录停点而不代选。Mod 对玩家的普通操作仍只观察、不修改游戏效果。

## 实机证据

原始 JSONL/Godot 日志均在 `/home/wst/Desktop/MCC/sts2sim/artifacts/legacy/live-diff-runtime/`，未提交：

| Trace | 路径 | 结果 |
|---|---|---|
| `trace-event-050.jsonl` | `C60BA09B` 首战领金币和 True Grit → Sunken Statue 选 Grab Sword → 第四层 The Legends Were True | 56/56 比较点一致，无阻断 |
| `trace-event-051.jsonl` | 同一起点，Sunken Statue 选 Dive Into Water，获得金币并失去生命 → 第四层 The Legends Were True | 56/56 一致，无阻断 |
| `trace-event-052.jsonl` | `67C65245` 首战领金币和 Twin Strike → The Legends Were True 获得 Spoils Map → 第四层商店入口 | 57/57 一致，无阻断 |
| `trace-event-053.jsonl` | `4CA97550` | 角色资源预加载时游戏进程收到 SIGSEGV，尚未进入对局；不能算差分结论 |
| `trace-event-054.jsonl` | 复跑 `4CA97550`，首战后进 Brain Leech 选 Share Knowledge | 游戏正常启动；Env 到 `FromSimpleGridForRewards` Choice，Mod 记录 `choice_bridge_not_implemented`，探针等真实选牌输入超时。此路径是已知桥接缺口，不算通过 |

成功样本的比较点包括完整战斗状态、整局资源/RNG、奖励内容与领取标志、事件选项键与顺序。`053` 的启动崩溃未复现；因无崩溃栈，不能归因于并行检查或 Mod。

## 验证与限制

`~/.dotnet/dotnet build tools/Sts2LiveDiff/Sts2LiveDiff.csproj -c Release` 为 0 警告、0 错误。`STS2_SRC='/home/wst/Desktop/MCC/Slay the Spire 2/src/Core' scripts/check.sh` 全部通过，包括 34/34 CTest、L2 200/200、L3 两组 40/40 和 L4 定向三幕；原始输出为 `artifacts/legacy/live-diff-runtime/check-live-event.log`。本批只改 Mod 探针与文档，检查脚本本身不编译 Mod，故另做上述 Mod 构建及三条实机路径。

当前实机路径最多抵达第四层商店入口。Brain Leech 的通用网格选牌、商店交互及完整三幕真实游戏路径还未接入。本报告只证明列出的路线与字段，不构成整局全内容覆盖。
