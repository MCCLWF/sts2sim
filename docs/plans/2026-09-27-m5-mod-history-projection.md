# M5：Mod 历史投影与通用卡牌执行事实

分支 `feat/combatsolver-env-backend`，前置 `a2fa0c3`。能力搜索账本已迁入 Mod；核心原状态键/评分/正式 Env 执行仍未完成。上轮为可验证进展，不构成最终目标完成。

1. 删除 C++ CombatSolver 风险枚举、Rampage/VoidForm 分派映射和风险列；每次真正进入 CardModel.OnPlay 前记录通用 CardEffectStarted（卡牌/owner/资源/目标/重播身份）。升级外层 v7，Clone/Hash/Import/Choice replay 同步。
2. C++ CardPlay 记录实际游戏目标；原搜索 Self 目标适配移到 C#。C# Mod 依据原固定策略将效果开始事实映射成风险条目，普通事实投影为原历史；维护 raw→policy index 映射，修正 draw/generated 的延迟引用。
3. EnvSearchBranch 消费同一个 Mod 投影，显示计数/后续状态键输入来自该投影；完整历史 coverage 仍按实际缺口拒绝执行。
4. 固定原 Beam 的全部条目序列、风险原因、六计数、卡牌/资源/伤害字段对拍；新增重播/风险后 draw/generated 索引核对、choice 重放和公开 ABI 严格版本/shape 验证。原期望不放宽、不删除风险。
5. 构建/API/原 Beam/查询/完整 scripts/check.sh 验证并写报告；登记其他 card/policy/calculated 查询的剩余迁移，继续最终同算法 Mod 目标。
