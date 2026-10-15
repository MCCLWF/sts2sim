# 通用 Env 原版预测入口与 CombatSolver 冻结

## 用户方向与目标

2026-09-27 用户要求冻结或删除 CombatSolver 相关内容，避免其影响 Env；以 Random Foreseer 所需通用 API 为目标全面投入 Env，回退此前搜索专用修改。简单内容批次交给 GPT-6 Luna xhigh。

这项方向取代先前的“Env 完成后自动恢复 CombatSolver”。CombatSolver 冻结后，除非用户再次明确要求，不恢复开发或构建。历史代码保留作档案，不能作为原版真值或当前门禁。

## 本批前置状态

- 开工：`84d59b6`，分支 `feat/generic-env-rf-api`，工作区干净。
- 原版 v0.111.0；普通 Env 已有 Step/Predict 与完整字段快照，但导入被旧战斗用途限制。
- 存在专用终局旁路：CheckWinCondition/EndEnemyTurn/RunFlow/伤害历史的预测条件改变正常结算。
- 旧 C++ 战斗预测、C# 验证工具及 Beam 入口为 CombatSolver 接入设计，默认构建仍包含它们。

## 本批任务

1. 将旧战斗预测 session/ABI/test 存入冻结档案，停止派生 Mod 和依赖其投影的 managed 工具构建；移除当前公共 API 的参考 Beam 入口。
2. 回退原版胜负、敌方回合、战后奖励和 DamageReceived 记录旁路。药水胜负检查按原版 ActionExecutor 在普通流程同样执行。
3. 把有价值的事实采集迁为不改变规则的 ExecutionTrace；去掉搜索终局保留和六个 Beam 计数。完整伤害结果在执行观察层采集，不篡改原版 History。
4. 提供通用 owned Preview：与普通 Env 相同流程，返回后继、决策/选择、原版结果和明确为部分覆盖的事实追踪；答案前缀重放返回替换用的整段操作 trace。
5. 原版 getter 查询改为接受普通 Env，保留原版 CalculatedVar 的 base/extra/multiplier 公式输入，删除专用终局保留字段。
6. 扩展完整字段恢复至已定义的整局决策/选择边界，保留严格 schema/ref/hash 和重放验证；不复制 C# async 栈。
7. C ABI/Python 提供相同的 owned Preview、查询和恢复能力；新增通用夹具，验证预测不改变原根、结果与普通执行一致、胜利仍做战后 Hook/奖励、需选择可继续和容量失败。

## 独立内容批次

- Luna：Prepared、EscapePlan、Adrenaline、Sidestep、Expose 五张原版静默卡。
- Luna：Paper Krane 及 WeakPower 的原版倍率调用。
- Luna：只读审计剩余搜索语义，不编辑公共核心。

主线程统一运行生成器和 C++ 构建；各批不得修改生成文件或相互覆盖。每批以正版 oracle 差分为行为验收。

## 验收与限制

- 当前默认构建/导出的 ABI 不含旧 CombatSolver 预测或参考 Beam；冻结 managed 工具明确拒绝构建。
- 通用 Preview 和恢复夹具、Python 公共接口通过；普通 L2/L3/L4 回归零分歧，内容批次独立差分通过。
- `scripts/check.sh` 在最终整合后运行；原来的 combat_prediction 专用测试由针对原版流程的通用测试替代，不用减少断言来保持旧语义。
- 本批不宣称 RF 全功能、完整原版内容、真实游戏全部捕获或性能验收完成。完整目标保持 active，继续按通用 Env 路线补齐。
