# M5：通用计算变量输入与 Mod 终局政策

分支 `feat/combatsolver-env-backend`，前置 `63b3a53`。卡牌指纹筛选、风险历史和 CanPlayCard 核心规则已迁入 Mod；完整原算法 Env 执行仍未完成。

1. 删除游戏 DynamicVar 的 CalculateForCombatPrediction/布尔 policy 分支，游戏 Calculate 只执行正版 active/attachment 条件。提取通用公式 RawCalculationMultiplier 查询，返回实际分支目标的公式原始乘数。
2. 独立计算查询升 v2，仅输出 game value 和 calculation base/extra/raw multiplier 精确 bits。删除 C++ 原预测语义名、prediction_value 和政策完整度。
3. C# Mod 由这些只读公式输入计算原 Base + Extra * Multiplier，公共双语义 GetValue 保留；严核原13模型绑定，未知公式拒绝。不调用原生未来 effects。
4. 正版游戏 getter + 固定原 CalculatedVarSpecRegistry：三组根/13变量/52目标和终局 BodySlam exact bits；原Beam16、API/C ABI、全 scripts/check.sh。更新报告/README/roadmap，保留完整statekey/score/正式Env gate未完成事实。
