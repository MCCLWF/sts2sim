# M5：计算型卡牌变量与原预测查询

分支：`feat/combatsolver-env-backend`。前置提交 `8655f92`；完整原 state key、policy trace/score 和正式 Env 执行仍未完成。

## 目标

移植已支持模型中 CalculatedVar 家族的纯计算语义，消除通用攻击与 Thrash 对 BaseValue 的错误代用；live 根验证实际静态公式绑定，公开分支查询保留 target/null 与 exact decimal，作为原评分读取边界的输入。原算法、候选、评分权重和预算不变。

## 工作

1. 对照正版 CalculatedVar/CalculatedDamageVar/CalculatedBlockVar 与所有已移植模型（当前 13 个）逐项确定公式、IsInProgress/CombatState 条件和目标语义；未移植公式明确拒绝。
2. DynamicVar 增加类型识别和 Calculate，通用攻击每次命中在原安全点计算；已移植效果调用同一公式，Thrash 使用 Calculate(null)。普通变量仍读取 BaseValue。
3. Live mapper 接受已审计标量及计算型变量，校验 exact 类型/属性、实际 owner 和 canonical 静态 delegate 绑定，不把未来计算在 native 中执行后注入 Env。
4. 独立 owned 计算查询按全部相关卡牌、null/每个目标返回 exact decimal，并对源 hash/全部状态/RNG 纯度检查；接入 managed 查询与未来 EnvSearchBranch 读取边界。政策/威胁完整标志保持 false。
5. 正版 TestMode=false getter 与固定原预测器的 branch-local Calculate 同根逐项对拍，覆盖 mutable base/extra、目标能力、格挡/历史、牌堆数量、非空充能球及离战条件；真实 Thrash+BodySlam、BodySlam 与 Barrage 修改变量出牌/后续两回合全字段差分。非零 Osty/宠物闭包仍属后续根适配范围。
6. 完整回归、派生构建、owned/API/C ABI 门禁，记录初始失败与尚未覆盖内容；最终 Mod 完整替换仍待后续正式执行适配。

## 验收

所有判据来自正版/原政策；不放宽全字段比较或跳过已知失败分支。无法精确计算时明确 Unported，源/兄弟不变。完整 `scripts/check.sh` 与相关 differential/API 工具通过，更新路线图和报告。
