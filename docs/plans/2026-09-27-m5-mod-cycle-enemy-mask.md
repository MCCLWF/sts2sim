# M5：Mod 循环键的敌人位图

分支 `feat/combatsolver-env-backend`，前置 `af8a143`。固定原 `CycleShapeKey` 的牌堆与药水组成已分别逐位对齐；剩余敌人位图必须遵循原 `KnownEnemies` 顺序及 `EffectiveEnemyHp > 0 && ContainsCreature`，不能直接读 Env 当前 roster 的 `is_alive`。

1. C# Mod 从 owned 根保存原始敌人顺序与身份，在 Fork/Predict 继承；当前快照只对证明可精确映射的普通生命/名单情形计算位图。
2. 对新敌人、逃跑、槽复用、活跃零 HP 复活窗口及 Waterfall Giant Steam 特例明确拒绝，后续再各自移植完整语义；这次不在 C++ 放原搜索位图。
3. Mod 将牌堆形状、位图、药水库存和显式 C# 搜索边界按固定原顺序组合，对照 16 个原 Beam 后继的 `CycleShapeKey`，报告有限范围。
4. 完整回归 `scripts/check.sh`、Mod 构建与 API 验收。保留原状态键、评分和正式 Env 搜索门禁。
