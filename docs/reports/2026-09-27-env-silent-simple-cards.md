# 静默猎手五张基础牌移植报告

日期：2026-09-27。状态：五张牌、Sidestep 所需 Power、主线程 Release 构建及指定有限 L3 矩阵均已完成。

## 前置条件

- 已阅读 `AGENTS.md`、`docs/audits/pitfalls.md`、当前 Env 路线图与完成计划。
- 当前分支：`feat/generic-env-rf-api`；开始前工作树干净。
- 原版依据：`../Slay the Spire 2/src/Core/Models/Cards/{Prepared,EscapePlan,Adrenaline,Sidestep,Expose}.cs` 与 `../Slay the Spire 2/src/Core/Models/Powers/EnergyNextTurnPower.cs`，版本 v0.111.0；另查阅相关 CardSelect/CardPile/Creature/VFX 命令源码。
- 本批编辑五张卡牌头文件、Sidestep 必需的 EnergyNextTurnPower 头文件、计划与报告；没有读取或依赖 CombatSolver 代码，没有改生成文件/共享测试/主计划。

## 实施

- `Prepared` 保留源码里的 `Cards.IntValue` 转换，按转换后的数量抽牌并以相同数量选择弃牌；若手牌不足，复用 `FromHand` 的源码对应行为。
- `Escape Plan` 检查抽牌结果非空且牌类型为 Skill 后才获得格挡。
- `Adrenaline` 先加能量，再按 Cards.BaseValue 抽牌；Exhaust 关键词由现有生成内容表提供。
- `Sidestep` 按 Energy.IntValue 将 EnergyNextTurnPower 施于自身。
- L3 首轮发现 EnergyNextTurnPower 的 `AfterEnergyReset` 是必需依赖且尚未移植；已按原版补齐 Power：仅当回合玩家等于持有者时，先获得 Power 当前层数的能量，再移除 Power。Buff/Counter 属性仍由现有内容表提供。
- `Expose` 按原顺序移除目标当前格挡、移除仍存在的 Artifact、施加易伤；Exhaust 关键词由生成表提供。
- 五个 `OnUpgrade` 都只提升 DynamicVar；已生成升级等级分别为 Prepared Cards 1→2、Escape Plan Block 3→5、Adrenaline Energy 1→2、Sidestep Energy 1→2、Expose Power 2→3，因此未重复覆写升级逻辑。
- 已审阅五类的每个 C# override：所有 `CanonicalVars` 对应生成表；Escape Plan 的 `GainsBlock` 已保留；Adrenaline/Expose 的 Exhaust `CanonicalKeywords` 对应表中关键词；Adrenaline、Sidestep、Expose 的 `ExtraHoverTips` 仅供 UI 显示，未移植为规则行为。
- Expose 的 `CreatureCmd.TriggerAnim` 只获取表现节点、触发动画并等待；`VfxCmd.PlayOnCreatureCenter` 只在生物表现节点播放 VFX。Adrenaline 的 `VfxCmd.PlayFullScreenInCombat` 只创建全屏表现节点并设置位置。这些调用都未写游戏模型状态，已省略。

## 验收结果

| 检查 | 结果 |
|---|---|
| 原版源码与相近命令/卡牌 API 逐项核对 | 完成；见上列源码路径与五张卡牌对应行为 |
| 升级值查对现有生成 delta | 完成；数值列于上文 |
| 生成及 Release 构建 | 主线程统一执行；`/tmp/mcc-env-build.log` 退出码 0 |
| 公共 oracle L3 | 80/80 combats identical，0 unported，0 diverged；1170/1170 状态匹配 |

## L3 轨迹覆盖与限制

- 矩阵：Silent、`CULTISTS_NORMAL`；种子 `SC_001,SC_002`；策略种子 `17,29`；A0/A10；每张目标牌分别使用普通版及 `+1` 升级版。牌组包含目标牌 5 张、`STRIKE_SILENT` 3 张、`DEFEND_SILENT` 4 张；共 20 个配置、80 场对局。
- 保留 trace 位于 `/tmp/sts2-silent-simple-cards-l3/`。80 场对局均实际打出目标牌；40 场升级牌配置中实际打出了 `+1` 牌。Prepared trace 中记录 63 次 card-select 决策。Escape Plan trace 中出现顶部抽到 `DEFEND_SILENT`（Skill）和 `STRIKE_SILENT`（非 Skill）的分支。
- Sidestep 的四组复验（普通/升级 × A0/A10，每组 2 seed × 2 policy）全部逐状态匹配。每场在下回合重置能量时均观察到 EnergyNextTurnPower 按已记录层数加能量并被移除；基础版与升级版分别按 1、2 能量生效。
- `tests/l3/l3_combat.py` 使用 headless TestMode + ProductionBranches 生成 oracle 轨迹。这些结果是指定种子、策略和路径的有限差分证据，不代表 TestMode Off 或真机运行；未运行全量 `check.sh`。

目标牌效果、升级值、Prepared 选牌、Escape Plan 两个条件分支及 Sidestep 下一回合能量都在上述有限矩阵内对齐正版 oracle。期望值来自 oracle trace，没有从模拟器自我生成。
