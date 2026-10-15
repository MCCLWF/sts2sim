# 静默猎手抽弃牌与多段攻击牌移植报告

日期：2026-09-27。状态：生成、Release 构建与有限原版 L3 差分通过。

## 前置条件与范围

- 分支：`feat/generic-env-rf-api`。依据原版 `../Slay the Spire 2/src/Core` v0.111.0。
- 本批新增 Outmaneuver、Flechettes 两卡，并只为 Flechettes 的 `CalculatedHits` 在 `DynamicVarSet.cpp` 增加倍率分支。
- Dagger Throw 文件在本批开始前已存在、已被生成 Dispatch 使用；复核其效果顺序与原版相同，因此未编辑。
- EnergyNextTurnPower 已由先前内容批次移植，当前不存在缺失依赖。
- Luna xhigh 只修改模型与 Flechettes 倍率分支；root 接手生成、构建和定向验收，日志列于下文。

## 实现核对

- Outmaneuver 按 `Energy.IntValue` 向自身施加 `ENERGY_NEXT_TURN_POWER`。C# 的 `ExtraHoverTips`、`VisualCardPool` 为表现/内容展示属性，由生成内容表处理；Power 下回合重置后加能量并移除的实现已存在。
- Dagger Throw 现有实现与源码顺序一致：目标攻击 → 抽 1 → `FromHandForDiscard` 选择 1 → 若结果非空则弃牌。C# prompt 仅供显示；其 `CardSelectorPrefs(prompt, 1)` 的最小和最大选择数都是 1，与当前 `CardSelectorPrefs(1)` 相同。攻击 VFX 已省略。
- Flechettes `OnPlay` 先通过 `DynamicVars()["CalculatedHits"].Calculate(target)` 计算命中次数，再将结果传给 `DamageCmd::WithHitCount`。新增的 raw multiplier 分支只统计当前手牌中 `Type()==Skill` 的牌；没有绕过 `Calculate`，CalculationBase、CalculationExtra 和现有 active combat/attachment guard 顺序保持原样。
- 升级改变量均由现有生成 level delta 提供：Outmaneuver Energy +1、Dagger Throw Damage +3、Flechettes Damage +2；没有新增 `OnUpgrade` 覆写。

## 验收状态

| 检查 | 状态 |
|---|---|
| 对照原版源码和相关 C++ API | 已完成 |
| Dagger Throw 既有顺序与选择数量复核 | 已完成，未改文件 |
| 内容生成及 Release 构建 | root 统一执行通过 |
| 定向 Oracle L3 | 50/50 场，702/702 状态对齐；0 divergence，0 Unported |

## L3 配置与报告要求

- 使用 `tests/l3/l3_combat.py`，Silent、可控普通遭遇、自定义牌组包含目标牌及 Silent 基础牌；普通与 `+1` 版本分别运行，两 seed、两 policy、A0/A10。
- Dagger Throw trace 检查攻击后抽牌，然后出现 `card_select` 且实际执行弃牌；若目标死亡而提前跳过选牌，应更换 seed/deck，直到取得真实选择分支。
- Outmaneuver trace 检查牌确实被打出、`ENERGY_NEXT_TURN_POWER` 被施加，随后在下一次 energy reset 按层数加能量并移除。升级牌应使用生成后的 Energy delta。
- Flechettes trace 从动作前手牌状态统计 Skill 数，并排除“猜测 hits 写入”做法；观察 actual card play 后的状态差分覆盖 0、1 和多个 Skill 计数。若有限随机矩阵缺分支，只调整 seeds/deck 做有限补充。
- L3 的 headless Oracle 设置 TestMode 并恢复所选 ProductionBranches。结果只描述指定路径上的有限差分，不声称 TestMode Off 或真机等价。

## 实际结果

`SDRAW001,SDRAW002` × policies `13,29` × A0/A10 × 普通/+1 × 三张目标牌：48 场。Dagger Throw 牌组为目标牌 4、Strike 4、Defend 7；Outmaneuver 为 5/4/6；Flechettes 为 3/6/6。原版 `combat --deck ...` 记录存入 `/tmp/mcc-env-silent-<CARD>-a<ASC>-u<UPGRADE>.jsonl`，随后 `python3 tests/l3/l3_combat.py --trace <file> --ascension <ASC> --deck <同一牌组>` 重放比较。逐格原始结果在 `/tmp/mcc-env-silent-l3-matrix.log`。

- Dagger Throw：16 场、244 状态；普通/升级各真实打出 26 次，共 52 个实际 discard 选择。现有文件未改。
- Outmaneuver：16 场、220 状态；普通/升级各打出 19 次。原版状态中观察到 EnergyNextTurnPower 2/3 及叠加 4/6/9；后续回合状态继续逐字段对齐。
- Flechettes：16 场、208 状态；普通/升级各打出 12 次。动作前原版手牌中 Skill 为 1 的出牌 16 次，为 2 的 8 次。
- 补 0 Skill：`SDRAW001` policy `13`、A0、普通/+1；Flechettes 4 + Strike 11，不放 Skill。2 场、30 状态对齐；普通/升级各实际打出 2 次。日志 `/tmp/mcc-env-silent-zero-skill.log`。合计 50 场、702 状态，0/1/2 Skill 均在普通与升级场景执行。

出牌版本从动作前原版 hand 的 index 读取，不能用 Oracle action 的 `card` 字段（该字段没有升级后缀）推断。上述计数源自原版 trace。每格额外要求 stdout 包含 `0 unported`，弥补默认 L3 只因 divergence 失败的门禁边界。完整集成门禁见[主报告](2026-09-27-env-run-query-results.md)；这些是有限场景，不代表所有修饰器、附件或其他卡牌交互都已覆盖。
