# 实机卡牌奖励选择桥

分支：`feat/live-card-reward-choice`。前置：真实游戏 Neow → 首战 → 奖励展示 → 领取金币 → 下一地图事件已连续对拍。

1. 在游戏原有的 `PlayerChoiceSynchronizer.SyncLocalChoice` 捕获卡牌奖励索引；对齐 Env 的 pending Choice 描述和候选顺序，向 Env 发送相同答案，不代选、不改变游戏规则。
2. 在隔离游戏进程让自动探针从真实奖励界面选择一张卡，比较选择前后整局、奖励与 RNG，继续到下一地图节点。
3. 单独构建 Mod、运行完整仓库检查，记录实机原始差异与未覆盖边界，再合并主分支。

涉及 `tools/Sts2LiveDiff/`、必要的 C ABI、路线图与报告。原始游戏 trace 留在 `.local`。
