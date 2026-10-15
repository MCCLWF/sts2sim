# 补齐原版本回合免费的星数语义

分支：`fix/free-star-cost-source`，基于 `5266daf`。范围为内容模型调用点，不修改 Env/ABI/Oracle/GC。

## 目标与依据

v0.111.0 的 `CardModel.SetToFreeThisTurn` 同时设置能量和星数临时费用，出牌或回合结束时清除。逐项核对源码发现 18 个模型使用此方法；Crossbow 已正确，另外 17 个模型只移植了能量变动，WhiteNoise 还错误使用保留到回合末的能量 override。

已有原版首处分歧：`COVFREE0930A / REGENT / A0 / p1`，注入牌组 `BULLET_TIME,DEVASTATE`、起始能量 3、星数 7、Cultists。第二个后继的星数应为 7，当前为 3。该案例属于注入测试，不代表自然分布。

## 工作

1. 逐项确认原版 18 个调用点，与当前 C++ 对应模型对照。
2. 将 17 个遗漏调用改为既有 `SetToFreeThisTurn`；保留原版明确只改能量的调用。
3. 以同一原版 trace 复验已发现分歧，并定向覆盖储君本职业的遗物/生成牌、跨池 BulletTime/WhiteNoise，以及回合/出牌清除。
4. 原版四张 BulletTime 同回合施放确认临时星数列表上限 3 失败；只合并相邻、cost 与两个过期标记完全相同的记录，保留不同值/寿命及 first/last 顺序，不变更状态 schema。审计 StarCostChanged 的订阅方并登记表示优化。
5. 构建、CTest、相关已有差分，记录原始命令与结果，更新现状并提交。

## 验收

- 所有原版 `SetToFreeThisTurn` 模型的 C++ 调用相符。
- 原版已发现分歧及定向矩阵零分歧，所有注入标记保留。
- 有关正常消费/回合清除的既有回归通过；不将定向短探针计作自然整局。

涉及 `game/Models/{Cards,Potions,Relics}` 的 17 个头文件及计划、报告、路线图/README。
