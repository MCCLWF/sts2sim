# 通用 Env 小批：PaperKrane 与 Weak 乘区

日期：2026-09-27。分支：feat/generic-env-rf-api。游戏基线：v0.111.0。

## 目标

按正版 PaperKrane.cs 和 WeakPower.cs 实现 PaperKrane 对 Weak 攻击乘数的修改，清除 WeakPower.ModifyDamageMultiplicative 中对应的 throw_unported。保持原版 Hook 顺序和 decimal 运算语义；不增加策略字段或引入客户端规则。

## 源码语义

- WeakPower.ModifyDamageMultiplicative 先验证 dealer 是 Weak 的拥有者，再验证 powered attack；以 DamageDecrease 为起始值，随后先调用目标玩家的 PaperKrane，再调用 dealer 的 DebilitatePower，最后返回结果。
- PaperKrane 仅在受击目标是遗物拥有者的 Creature、且攻击为 powered attack 时将传入乘数减去 Dec::lit(15,2)；其他情况原样返回传入值。
- C# 的 DebilitatePower.ModifyWeakMultiplier 调用首参是 dealer。移植保留此参数、调用次序及十进制运算。

## 工作项

- [x] 新增 game/Models/Relics/PaperKrane.h，逐项移植目标、属性和减值条件。
- [x] 在 game/Models/Powers/WeakPower.h 中，以原版顺序调用 PaperKrane 后再调用 DebilitatePower。
- [x] 检查 tests/l3/l3_combat.py 的 relic、牌组、遭遇和升阶参数支持；用 Oracle trace 确认条件路径实际出现后执行 L3 状态差分。
- [x] 记录有限 headless L3 证据、Trace 中实际观察到的分支以及验收边界。

## TestMode / headless 验收边界

- Oracle combat 启动路径设置 TestMode.IsOn；docs/audits/deviations.md 的 O-001 说明正式逻辑分支通过 ProductionBranches 逐方法恢复。当前 ProductionBranches 覆盖商店药水价格随机、Calling Bell 和 Cauldron 奖励生成，不涉及本批战斗路径。
- 只读检查的 PaperKrane、WeakPower、SuckerPunch、ArtifactPower、StrengthPower、AttackCommand、DamageCmd 和 PowerCmd 没有直接读取 TestMode。MonsterModel.PerformMove 中相关读取只包住日志，实际 move、history 更新和死亡移除仍执行。
- CreatureCmd.Kill 的 O-007 是本批重要限制：TestMode 下玩家全灭时仍调用 LoseCombat，但跳过正式 RunManager.OnEnded、存档和游戏结束界面。因此非致死前缀用于隔离验证 PaperKrane/Weak/敌方攻击结算；完整战斗轨迹可证明有限 headless 路径走到战斗结束事件，但不能证明正式 run 结束结算等价。
- 这些结果是所选固定遭遇、牌组、种子和策略种子的有限 headless L3 差分，不是全面 TestMode Off 或真机验收证明。

## L3 轨迹矩阵与覆盖判据

主矩阵每个配置使用两个 run seed 和 --policy-seeds 1,2，角色 SILENT、遭遇 FROG_KNIGHT_NORMAL、遗物 PAPER_KRANE、--max-actions 300：

| 配置 | Ascension | 牌组 | Run seeds | 覆盖判据 |
|---|---:|---|---|---|
| A0 base | 0 | SUCKER_PUNCH | PKNORM001,PKNORM002 | 观察 Sucker Punch 造成 Weak、PaperKrane 持有者随后承受敌方攻击 |
| A10 base | 10 | SUCKER_PUNCH | PKA10N001,PKA10N002 | 同上 |
| A0 upgraded | 0 | SUCKER_PUNCH+1 | PKUP0001,PKUP0002 | 同上，并记录升级牌产生的 Weak 层数 |
| A10 upgraded | 10 | SUCKER_PUNCH+1 | PKUP1001,PKUP1002 | 同上，并记录升级牌产生的 Weak 层数 |

每个完整 Oracle JSONL trace 通过 tests/l3/l3_combat.py --trace <trace> --cli build/sts2sim_cli 重放；相同 trace 的前三个状态、前两个动作另存为 .prefix.jsonl，作为玩家存活时的非致死前缀差分。每个 L3 比较检查实际状态行，不手算或预设伤害期望值。

可复现的 Oracle 命令模板（按表替换 seed、ascension、deck 和输出名；stderr 单独保存）：

    ~/.dotnet/dotnet tools/Sts2Oracle/bin/Release/net9.0/Sts2Oracle.dll combat \
      --seeds PKNORM001,PKNORM002 --characters SILENT --policy-seeds 1,2 \
      --max-actions 300 --ascension 0 --encounter FROG_KNIGHT_NORMAL \
      --deck SUCKER_PUNCH --relics PAPER_KRANE \
      > /tmp/paper-krane-traces-2026-09-27/a0-base.jsonl \
      2> /tmp/paper-krane-traces-2026-09-27/a0-base.stderr
    python3 tests/l3/l3_combat.py \
      --trace /tmp/paper-krane-traces-2026-09-27/a0-base.jsonl \
      --cli build/sts2sim_cli --ascension 0 --encounter FROG_KNIGHT_NORMAL \
      --deck SUCKER_PUNCH --relics PAPER_KRANE -v

补充配置（总计两个配置；每个仍使用两个 run seed × 两个策略 seed）：

| 条件 | Ascension、遭遇、牌组 | Run seeds | 覆盖判据 |
|---|---|---|---|
| Artifact | A0，CHOMPERS_NORMAL，三个 SUCKER_PUNCH，PAPER_KRANE | PKART001,PKART002 | trace 中敌人初始有 Artifact，实际打出 Sucker Punch，随后观察到 Weak 和受攻击状态 |
| 负 Strength | A0，FROG_KNIGHT_NORMAL，DARK_SHACKLES,SUCKER_PUNCH，PAPER_KRANE | PKNEG001,PKNEG002 | 敌人同时有负 Strength 与 Weak，之后其攻击在 trace 中出现 |

## 涉及文件

- game/Models/Relics/PaperKrane.h
- game/Models/Powers/WeakPower.h
- docs/plans/2026-09-27-env-paper-krane.md
- docs/reports/2026-09-27-env-paper-krane.md
