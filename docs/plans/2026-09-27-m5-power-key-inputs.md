# M5：原能力实例键输入

分支：`feat/combatsolver-env-backend`。前置提交 `cce9cd6`。完整原状态键/历史/评分与正式执行门禁仍未完成。

## 工作与验收

1. 对照固定原 SimulatedCombatState.GetPowerFingerprint、PowerLifecycleSupport、SemanticStateFieldPolicy 和正版模型，映射 owner/applier/target CombatId/null、amount、三个模型的 AmountOnTurnStart、Weak/Vulnerable/Frail duration skip、Ritual flag、Surrounded direction 和语义变量 unordered 编码。
2. 独立 C++/C ABI/managed owned 只读输入查询；从实际状态读取，未分类字符串/模型明确拒绝，private clone/full hash/source/兄弟不变。能力个体指纹不冒充完整 P aggregate、retired root slots 或完整原 key。
3. EnvSearchBranch 接入部分指纹，原 Beam 诊断逐个核对当前能力身份和值。编码复用固定原 StateFingerprintBuilder。
4. 三组 TestMode=false 实际根，覆盖非零/忽略的 turn-start 字段、duration skip、nullable 来源、Ritual/Surrounded 和小数变量；调用实际原 private GetPowerFingerprint 作为参照。管理/C ABI 对拍、所有权/失败/并发/释放与完整 scripts/check.sh 门禁。
5. 写实际报告，更新 README/路线图/同算法计划；完整历史、原评分、一般 live 接受及正式 Env 执行仍为后续要求。
