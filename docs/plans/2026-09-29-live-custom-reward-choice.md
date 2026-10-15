# 实机嵌套奖励选择桥

分支：`feat/live-custom-reward-choice`。起因：随机种子 `054C276A` 的 Neow Lost Coffer 在 `RewardsCmd.OfferCustom` 暂停于奖励选择；现有实机桥将 Env Choice 阻断。

1. 比较非战斗终结奖励屏的奖励条目及领取状态，把游戏真实领取、跳过动作映射到 Env 的 `Answer`，保持事件动作的连续前缀。
2. 用隔离正版游戏进程在该随机种子上经原版 UI 领取一种奖励并跳过剩余奖励，继续至地图/下个静止点；出现分歧保留首个差异，不能改写原版结果。
3. 构建 Mod 与 C ABI、运行完整仓库检查，记录结果、限制并合并主分支。

涉及 `tools/Sts2LiveDiff/`、`env/c_api.*`、路线图与报告。原始 trace 留在 `.local`。
