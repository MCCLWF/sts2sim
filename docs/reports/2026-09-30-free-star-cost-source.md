# 原版本回合免费调用点与严格 seed 覆盖审计

日期：2026-09-30，基底 `5266daf`，分支 `fix/free-star-cost-source`，独立工作树 `sts2sim-seed-rules`。规则依据是 v0.111.0 源码及正版 DLL，未修改 API/ABI/Oracle/GC，CombatSolver 保持冻结。

## 前置审计与覆盖

已读 AGENTS、design、roadmap、deviations、pitfalls、严格批次/合并复核报告。只读核查 `../artifacts/legacy/strict-diff-merge-review-2026-09-30/all-525-inputs.json` 中的全部 525 个原始 trace，包含 285,615 个决策：

- 225 条正常 HP 对局全部第一幕死亡；300 条高 HP 注入中，20 条第一幕死亡、128 条第二幕死亡、69 条第三幕死亡、83 条第三幕胜利。五角色和 A0/A10 均有注入终局，但自然三幕路径仍为零。
- 全部 policy_seed=1。OracleRun.PlayCombat 仍显式跳过 Neutralize/Survivor；140,037 次出牌中二者均为 0。98,923 次 EndTurn，UsePotion/DiscardPotion 均为 0。这些分支不可能通过增加 seed 填齐。
- 全部 9,660 次选卡均非空；奖励提供 Skip 3,451 次、SACRIFICE 12 次、REROLL 68 次，但一个也没选。GetSelectedCards 总取 maxSelect，只在候选不够时缩短；GetSelectedCardReward 默认只选卡。完整选择/奖励条件覆盖仍缺。
- 观察到 63 个不同事件决策。正常模型中 LuminousChoir、RelicTrader 未出现；FakeMerchant 是 shop 决策，不能仅按事件决策 ID 判未访问；其余缺项是 deprecated/mock。
- LuminousChoir 原版 IsAllowed 要金币至少 149 且遗物袋有剩余，仅在 Overgrowth；RelicTrader 要第二幕及以后、至少 5 件可交易遗物。应以策略/注入打开条件，再检验实际执行，不把看到候选当作走过分支。
- 19 件 Neow 遗物已选过，另 10 件虽被提供但从未选；Darv BlackStar/RunicPyramid、Vakuu MusicBox、Trial DoubleDown、UnrestSite Kill、ZenWeaver ArachnidAcupuncture、Wongos MysteryBox 等有效分支也尚无本语料中的选择证据。

优先扩展 policy seed，补原生药水使用/丢弃和空选/部分选/奖励 alternatives，再扩大新 run seeds。低覆盖身份可重跑 p2–p20：`DCE2E2E678AF2/DEFECT/A0`（Unrest）、`D6F769BED99B0/SILENT/A0`（WarHistorian）、`AUFCFDE8A182/REGENT/A10`（ZenWeaver）、`AU29E9C7FD95/REGENT/A10`（SpiritGrafter）。另建明确注入的 high HP + 5 件以上可交易遗物矩阵打开 RelicTrader。高 HP 与自然 HP 必须分开统计。

## 规则首因与修复

原版 `CardModel.SetToFreeThisTurn` 同时调用 EnergyCost.SetThisTurnOrUntilPlayed(0) 和 SetStarCostThisTurn(0)，两个 override 都在出牌或回合结束清除。逐项核对全部 18 个模型调用点，Crossbow 已正确；修复另外 17 个只移植能量的调用：

- 卡牌：Abundance、BulletTime、Discovery、Distraction、InfernalBlade、MadScience、Splash、WhiteNoise。
- 药水：AttackPotion、ColorlessPotion、LiquidMemories、OrobicAcid、PowerPotion、SkillPotion。
- 遗物：JeweledMask、MummifiedHand、VexingPuzzlebox。

WhiteNoise 还错误使用 EnergyCost.SetThisTurn，导致同回合重打时能量 override 过期不同；现与原版一样共用 SetToFreeThisTurn。真正只修改能量的调用保持原规则，例如 Enlightenment、Flatten、SneckoOil、AdaptiveStrike、MomentumStrike。

原版首处分歧 `COVFREE0930A / REGENT / A0 / p1`：注入 `BULLET_TIME,DEVASTATE`、能量 3、星数 7、Cultists；BulletTime 之后打 Devastate，原版仍有 7 星，旧模拟器只有 3 星。另一条 `COVFREE0930B` 后续旧模拟器取消合法出牌。原始失败证据保留在 `/tmp/sts2sim-coverage-free0930`，没有改动作或重新抽种子。

补全调用点后，原版 `COVCAP0930` 的四张 BulletTime 同回合施放成功，C++ 却在第 4 次追加临时星数时达到容量 3。按 source first/last 与过期规则，只将相邻且 cost、两个过期标记完全相同的条目存一次，SetToFreeThisTurn/SetToFreeThisCombat 共用私有 AddTemporaryStarCost。没有扩容或改变 schema，没有合并不相邻条目、不同费用或不同寿命。内部表示与 private List 的区别登记为 [D-007](../audits/deviations.md)。

StarCostChanged 仅有 NHandCardHolder.Flash 与 CombatStateTracker.OnCardValueChanged 两类订阅；后者只安排表现通知，TestMode 下明确禁止 backend 订阅。此表示优化不省略玩法 Hook/RNG。不同寿命交替或不同值的任意容量上界仍未认证。

## 验收与原始证据

首轮 17 调用点版本：Release `-j2` 构建、46/46 CTest、完整 `scripts/check.sh` 通过；这不是新增 helper 最终版资格。其 90 条定向完整战斗、1,396/1,396 状态对齐，独立原始 trace/result 保存在 `/tmp/sts2sim-free-star-*0930*`。最终版另外保存验收结果，不覆盖首轮或容量失败。

最终 helper 版本严格离线重放：13 个输入文件中的 91 个完整战斗全部 identical，1,419/1,419 状态、1,493/1,493 总观察（含选择）一致；四次 BulletTime 的原件也通过。结果在 `/tmp/sts2sim-free-star-final-replay0930`，默认严格，无 Unported/截断/采集或进程失败。所有定向场景是显式注入，不计正常通关率或自然完整内容覆盖。

最终 `taskset -c 0-2 scripts/check.sh` 退出 0：46/46 CTest、L2 200/200、L3 81 场完整战斗与 1,625/1,625 状态，另 Untouchable 2 个短探针与 4/4 状态；L4 99 条固定路线、16,379/16,379 决策一致，含现有逐点快照恢复路径。日志 `/tmp/sts2sim-free-star-check-final0930.log` SHA256 为 `97f5a7d12609f70bcdd585b8f07e73ad4209747979e0b79f4a656b11fbf7cee5`。CPU 亲和性只限制构建/执行并行度，没有缩减门禁。

最终离线 source SHA256 `d25eb3a7d17aaefc678885d3a3490e29f315741ce76348fe200189d60fda7113`；CLI SHA256 `21223d98dfb383e9c6082574f7a688b5fd4b4792a8a45d37adae27bc78dcafb2`。离线 manifest 将 standalone 原件标为 legacy/offline，不伪造它们采集时的外部依赖冻结证据。各原始 command/process 文件仍保存真实 Oracle 路径、参数、退出码；本批没给 standalone 采集追补运行时 manifest。

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 2
taskset -c 0-2 scripts/check.sh

# 重用最初暴露错误的两个原件
python3 tests/l3/l3_combat.py \
  --trace /tmp/sts2sim-coverage-free0930/oracle.stdout.jsonl \
  --cli build/sts2sim_cli --require-complete

# 原版四次 BulletTime 的完整 trace 包括 forced policy 命令原件
python3 tests/l3/l3_combat.py \
  --trace /tmp/sts2sim-free-star-capacity0930/oracle.stdout.jsonl \
  --cli build/sts2sim_cli --require-complete
```

定向采集全部 REGENT/A0，Cultists、起始能量 3、星数 7、policy 1,2,3,4；每行两个固定 seed，共 8 个完整战斗。每条通过 `tests/l3/l3_combat.py` 采集后，最终 helper 版本复用同一 raw oracle.stdout.jsonl 重放：

| seed 尾缀（前缀 COVFREE0930） | 牌组 | 额外配置 | 状态数 |
|---|---|---|---:|
| C,D | WHITE_NOISE,STRIKE_REGENT,STRIKE_REGENT | 无 | 142 |
| E,F | DISCOVERY,SPLASH,STRIKE_REGENT | 无 | 136 |
| G,H | NEUTRON_AEGIS,DEVASTATE | MUMMIFIED_HAND | 53 |
| I,J | NEUTRON_AEGIS,STRIKE_REGENT | JEWELED_MASK | 102 |
| K,L | ABUNDANCE,DISTRACTION,INFERNAL_BLADE,STRIKE_REGENT | 无 | 145 |
| M,N | STRIKE_REGENT,STRIKE_REGENT | VEXING_PUZZLEBOX | 156 |
| O,P | STRIKE_REGENT,STRIKE_REGENT | ATTACK_POTION | 115 |
| O,P | 同上 | SKILL_POTION | 131 |
| O,P | 同上 | POWER_POTION | 119 |
| O,P | 同上 | COLORLESS_POTION | 132 |
| O,P | 同上 | OROBIC_ACID | 145 |

本批没有宣称 17 模型每个条件都已覆盖，LiquidMemories 的非空弃牌、MadScience 所有配置和完整跨幕调用组合仍需后续定向验收。原版 TestMode/无头补丁的实机资格仍沿用登记限制。
