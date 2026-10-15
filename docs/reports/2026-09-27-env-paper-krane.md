# 通用 Env 小批：PaperKrane 与 Weak 乘区报告

日期：2026-09-27。分支：feat/generic-env-rf-api。计划：[PaperKrane 与 Weak 乘区](../plans/2026-09-27-env-paper-krane.md)。

## 实现

- 新增 PaperKrane 遗物模型，目标和 powered attack 条件按 v0.111.0 源码执行；适用时以 Dec::lit(15, 2) 从传入乘数中减去 0.15。
- WeakPower 对目标玩家的 PaperKrane 执行原版调用，然后执行 Weak 拥有者的 DebilitatePower。Debilitate 调用的首参也保持为原版 dealer。
- 本批未新增策略字段，也未读取或依赖 CombatSolver。

## TestMode 与 Oracle 范围

- Oracle combat 在 TestMode.IsOn 下运行；根据 docs/audits/deviations.md O-001，headless 路径由 ProductionBranches 逐方法恢复已登记的正式逻辑分支。当前恢复列表是 MerchantPotionEntry.CalcCost、CallingBell.GenerateRewards 和 Cauldron.GenerateRewards，与本批战斗逻辑无关。
- 只读检查的 PaperKrane、WeakPower、SuckerPunch、ArtifactPower、StrengthPower、AttackCommand、DamageCmd 和 PowerCmd 均无直接 TestMode 读取。MonsterModel.PerformMove 的 TestMode 分支只控制日志；move 执行、历史记录和死亡移除仍走原逻辑。
- CreatureCmd 的其他相关读取中，Heal 分支属于休息点路径，TriggerAnim 分支仅在缺失节点时记日志；O-007 则明确记录玩家全灭时 TestMode 跳过正式 RunManager.OnEnded、存档和游戏结束 UI。主矩阵的 16 场完整轨迹最终都到达玩家 HP=0，所以完整 trace 不作为正式 run 结束结算等价证明。
- 为隔离 O-007 之前的战斗结算，另将每个主矩阵 Oracle trace 截取为前三个状态、前两个动作的非致死前缀并做 L3 比较。全部结果只支持本报告列出的固定 headless 路径，不代表全面 TestMode Off 或真机验收。

## 差分结果

Oracle JSONL 原始轨迹与前缀保存在 /tmp/paper-krane-traces-2026-09-27/。四个主矩阵配置各含两个 run seed、两个 policy seed（每配置 4 场）：

| 配置 | 参数 | 完整 trace L3 | 非致死前缀 L3 | 实际观察到的条件 |
|---|---|---:|---:|---|
| A0 base | FROG_KNIGHT_NORMAL，SUCKER_PUNCH，PKNORM001,PKNORM002 | 4/4 combats，64/64 states；0 unported、0 diverged | 4/4，12/12 states；0 unported、0 diverged | Weak=1，PaperKrane 持有者受攻击；trace 中攻击 intent 为 7，敌方回合后玩家 HP 70→63。另在较后回合观察到 Strength=5 与 Weak 同时存在，intent 10，HP 51→41。 |
| A10 base | FROG_KNIGHT_NORMAL，SUCKER_PUNCH，PKA10N001,PKA10N002 | 4/4 combats，64/64 states；0 unported、0 diverged | 4/4，12/12 states；0 unported、0 diverged | Weak=1，攻击 intent 8，敌方回合后 HP 70→62；较后 Strength=5 与 Weak 同时存在，intent 11，HP 49→38。 |
| A0 upgraded | FROG_KNIGHT_NORMAL，SUCKER_PUNCH+1，PKUP0001,PKUP0002 | 4/4 combats，64/64 states；0 unported、0 diverged | 4/4，12/12 states；0 unported、0 diverged | 首次 Weak=2；攻击 intent 7，敌方回合后 HP 70→63。较后回合 Weak 累积至 5，与 Strength=5 同时存在。 |
| A10 upgraded | FROG_KNIGHT_NORMAL，SUCKER_PUNCH+1，PKUP1001,PKUP1002 | 4/4 combats，64/64 states；0 unported、0 diverged | 4/4，12/12 states；0 unported、0 diverged | 首次 Weak=2；攻击 intent 8，敌方回合后 HP 70→62。较后回合 Weak 累积至 5，与 Strength=5 同时存在。 |

表内 intent、power 和 HP 数字均为 Oracle JSONL 实际观察值，仅用于说明 trace 命中了哪些条件，不是手算期望值。主矩阵合计 16/16 完整战斗、256/256 完整状态，及 16/16 非致死前缀、48/48 前缀状态；均无 unported 或 diverged。

两个补充配置（各两个 run seed × 两个 policy seed）也有实际分支覆盖：

| 配置 | L3 结果 | 覆盖证据 |
|---|---|---|
| A0 Artifact：CHOMPERS_NORMAL，三张 SUCKER_PUNCH，PKART001,PKART002 | 4/4 combats，83/83 states；0 unported、0 diverged | Chomper 初始持有 Artifact；trace 实际对两个敌方 combat ID 使用 Sucker Punch，随后出现 Weak 和敌方攻击。例如一条 trace 中 Artifact 已消耗、Weak=1、攻击 intent=4，玩家 HP 54→46；之后另一个目标的攻击令 HP 46→38。 |
| A0 负 Strength：FROG_KNIGHT_NORMAL，DARK_SHACKLES,SUCKER_PUNCH，PKNEG001,PKNEG002 | 4/4 combats，68/68 states；0 unported、0 diverged | 四场均观察到敌人 Strength=-9、Weak=1、攻击 intent=2，敌方回合后玩家 HP 70→68。 |

补充配置合计 8/8 完整战斗、151/151 状态；均无 unported 或 diverged。主矩阵覆盖 Weak 与 PaperKrane 后发生的实际敌方攻击，并在完整 trace 中覆盖正 Strength 与 Weak 同时存在；补充配置分别覆盖初始 Artifact 后的 Weak/攻击路径和负 Strength、Weak 与攻击重叠。覆盖范围限于这些所选配置。

## 验收状态

| 检查 | 结果 |
|---|---|
| 正版源码语义核对 | 已完成 |
| PaperKrane/Weak 头文件实现 | 已完成 |
| 统一生成与 Release 构建 | 主线程已完成 |
| 主矩阵 A0/A10、普通/升级牌 L3 | 4 配置各 4/4 combats；0 unported、0 diverged |
| 非致死前缀 L3 | 4 配置各 4/4；0 unported、0 diverged |
| Artifact 补充差分 | 4/4 combats；Artifact、Weak 和敌方攻击实际出现 |
| 负 Strength 补充差分 | 4/4 combats；Strength=-9、Weak 和敌方攻击同时出现 |
| O-007 终局边界 | 已如实记录；全灭轨迹不用于证明正式 OnEnded/存档/游戏结束流程 |
| 真机或全面 TestMode Off 验收 | 未执行；本报告不作此结论 |
| TestMode Off 阻塞状态 | 已移除；O-001 的逐方法恢复机制支持这些有限 headless L3 对拍，O-007 仅限制全灭后的正式 run 结算主张 |

原始 Oracle trace、派生非致死前缀和 L3 输出保存在 /tmp/paper-krane-traces-2026-09-27/，不作为仓库跟踪文件。未运行 CMake；Release 构建由主线程执行。本报告记录的是有限 headless L3 路径差分，不是全面 TestMode Off 或真机证明。
