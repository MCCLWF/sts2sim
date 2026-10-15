# M5：实机解锁档案进入 Env 状态

分支 `feat/combatsolver-env-backend`，前置 `4c99a23`。当前 Mod 已捕获 Run 与 Player 的真实档案，但 C++ `GameState` 没有这些事实，因此非全解锁档案在组装前即被拒绝。

1. 在通用 POD 状态中分别保存 Run/Player 的运行次数、按游戏 `EpochModel.AllEpochIds` 排列的解锁位及按生成 `EncounterId` 编号的已见遭遇位。全解锁 seeded 环境保留原语义。快照、克隆和哈希自动覆盖新字段。
2. Mod 从已捕获的真实 `UnlockState` 映射这些字段；移除组装前的全解锁要求，让部分档案可以导入为 owned 状态。目录版本与位数超界明确拒绝。
3. 卡池、药水池、首局教程与已见遭遇尚未移植；通用 Env 对部分档案的动作执行明确拒绝，不能用全解锁内容偷偷预测。将来逐项放开。
4. 对拍全解锁现有路径，测试部分档案的状态往返与执行拒绝，跑 Mod 构建及 `scripts/check.sh`，实机检查当前档案能否导入。失败或未覆盖处写入报告。
