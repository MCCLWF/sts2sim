# Mod 药水库存键

日期：2026-09-27。分支 `feat/combatsolver-env-backend`，前置 `a44d4da`；固定原 CombatSolver 子模块 `d231e9e51a0e58d6bfa1373c6265cce13ffd9a45` 未修改。

## 实现边界

原 `BuildPotionInventoryKey` 使用原根固定的药水槽数，对每个槽依次编码药水 `Id.Entry` 或 `"-"`，再编码 `IsPotionAvailable`。后者在原 `SimulatedCombatState` 中等价于该槽药水非空。Mod 打开 Env 根时捕获槽数，Fork/Predict 继承；每个 owned 后继从 C++ 通用快照读取当前槽引用和药水池模型 ID，由 `EnvModelCatalog` 将 ID 还原为固定原字符串，使用原 `StateFingerprintBuilder` 编码。C++ Env 不增加搜索政策字段。

槽数超出 0..8、当前槽布局与根不同、根槽位外存在药水、引用/模型/持有者无效或在位药水已标记 removed 时，Mod 明确拒绝该键。根本身仍须通过已有 live 映射门禁；本次键对拍不宣称实机 Env 正式接受。

## 验证

- 派生 Mod Release 构建：`/tmp/mcc-potion-key-mod-build.log`，0 warning、0 error。
- TestMode=false 的 16 个原 Beam 场景 owned Env 后继：`/tmp/mcc-potion-key-beam.log`，原私有 `BuildPotionInventoryKey` 与 Mod 编码逐位相同。场景包括 `fire_potion`、`power_retire` 和 `power_retire_reacquire` 的槽位消耗；原方法反射仅在测试夹具里调用。
- Getter 回归：`/tmp/mcc-potion-key-queries.log`，三组 TestMode=false 完整 live 根及一组 Play 牌堆边界通过；预测 API：`/tmp/mcc-potion-key-api.log`，64 并行、16 专用分支、协议、源隔离和 Dispose 通过。
- 完整 `scripts/check.sh`：`/tmp/mcc-potion-key-full.log`，CTest 10/10；L2 200 局零差异；L3 A0/A10 各 40/40 场、784/784 与 827/827 状态；L4 两组各 40/40 局、1958/1958 与 2131/2131 决策。Oracle 构建有既有一项 CS8604 warning、零 error。

药水键仅是原 `CycleShapeKey` 的一个组成部分。原已知敌人位图需要 `KnownEnemies` 顺序、有效生命与在场判定，搜索边界也需原政策，不能从当前活动敌人 roster 与 C++ 原始生命直接推断。完整状态键、评分、原算法 Env 执行和同预算性能仍未完成。
