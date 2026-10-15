# 计划：整局骨架（M1-g、M1-i、M1-j）

执行者：Claude。分支：`feat/run-skeleton`。

## 目标

M1 验收第 3 条：随机智能体能从开局走到第一幕 Boss 前，不崩溃，不卡死。

## 任务

1. M1-g 选择前缀：`env/Env` 在顶层决策点做快照，效果中途的选择抛 `NeedChoice`，带着前缀重放；`CardSelectCmd` 全部选择路径（有序子集编码）、奖励三选一、选包、嵌套奖励界面。
2. M1-i 整局骨架：地图移动、问号房、战斗奖励、对象回收（GC）、事件框架与 Neow、休息点（休息、锻造）、商店（完整库存与购买、删牌）、宝箱（金币、遗物选择）、`RewardsCmd.OfferCustom`，以及它们用到的命令（变牌、苦难、删牌、诅咒）。
3. oracle 整局记录与 C++ 重放逐点比对（L4）：`tools/Sts2Oracle/OracleRun.cs`、`tools/cli/replay_cmd.cpp`、`tests/l4/l4_run.py`。
4. M1-j 冒烟：`sts2sim_cli smoke`，接进 `scripts/check.sh`。

## 验收

- `scripts/check.sh` 全部通过（新增冒烟与 L4 两步）。
- L4：不许出现 diverged；遇到未移植内容时受控停止（`Unported`）。
- 冒烟：0 个 `error`，0 个卡死（`max_steps`）。
