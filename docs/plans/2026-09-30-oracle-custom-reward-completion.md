# 原版 custom Rewards 采集完成合同

## 前置与首因

- 接续 seed policy/level 合同集成 `6c3c6a2`；原修前 40 自然和四选择批保留，不覆盖或补改资格。
- API 并行任务指出两端共同要求了非原版 custom Rewards 的额外 Proceed。
- 原版 RewardsSet.Offer 以 Room is CombatRoom 判定 terminal；RewardsSetSynchronizer 在 AllRewardsSuccessfullySelected 时完成并退栈；NRewardsScreen.UpdateScreenState 的非terminal 分支立即移除页面。
- 现 OracleRun.SelectRewards 在 available.Count=0 后仍 emit rewards，再抽策略 RNG 并输出 Proceed。修前 LostCoffer 的两条注入样本原始证据已保存，policy1取完仍有额外Proceed；policy2直接skip。

## 工作

1. 仅 OracleRun.SelectRewards 在非 CombatRoom 奖励集全部 SuccessfullySelected 时立即返回，不再额外发决策或抽策略 RNG。CombatRoom terminal Proceed 和有剩余奖励的 Skip 保留。
2. 修复应用于两种策略；不变更 policy-version 来伪装采集合同修复，也不改游戏 DLL、RNG 或比较字段。
3. 原 runtime 与修前捕获保留；新输出目录验证 Oracle 修复暴露旧C++差异，再接收 API 独立 RewardsCmd 修复验证原版/模拟器组合。
4. 定向 LostCoffer、CallingBell（含嵌套）、Cauldron（药水容量不足后的跳过）；验证全部领取会自动结束、未完成仍可Skip、CombatRoom正常Proceed保留。
5. 限定工具协议串行复核并独立报告首因、命令、原始结果和代码指纹。

## 限制

本工作只修改 Oracle 采集驱动。C++ RewardsCmd 修复由 API 并行任务提供；取得并组合验证前，不能把单端参照成功当作差分通过。旧trace继续可离线回放但其额外custom Proceed合同已知错误，不能按新资格解释。广泛全内容或真机合同未由这些有限注入验证完成。
