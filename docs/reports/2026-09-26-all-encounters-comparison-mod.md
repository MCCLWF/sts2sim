# 全遭遇战与战士双求解器比较：阶段报告

日期：2026-09-26

分支：`feat/all-encounters-comparison-mod`
状态：**正式遭遇模型完成 90/90；双求解器比较仍在开发**

## 前置条件与版本

- 游戏与 oracle 基线：正版 v0.111.0，反编译源码 `../Slay the Spire 2/src/Core`。
- CombatSolver 已下载为 `external/CombatSolver` Git 子模块，固定提交 `d231e9e51a0e58d6bfa1373c6265cce13ffd9a45`；其 Release .NET 构建 0 警告、0 错误。
- 比较 Mod `tools/Sts2EnvCompare` Release 构建 0 警告、0 错误，已复制到本机游戏 `mods/Sts2EnvCompare/` 进行无头实机测试。没有提交游戏 DLL 或编译产物。
- 用户限定其他角色卡牌暂不实现；本批仅使用铁甲战士验证。

## 本批新增内容

正式遭遇模型由 39/90 起步，目前为 **90/90**；怪物内容模型头文件从 46/126 增至 **109/126**。初始批次新增 Devoted Sculptor、Chompers、Frog Knight、Spiny Toad、Slimed Berserker、Tunneler（弱/普通）、Soul Nexus、Scrolls of Biting（弱/普通）、Hunter Killer，以及事件用 The Architect 和历史兼容用 Deprecated Encounter。之后增加了 Exoskeleton（弱/普通）、Mytes Normal、Bowlbugs（弱/普通）、Louse Progenitor Normal、Slumbering Beetle Normal、Axebots Normal、Owl Magistrate Normal、Mysterious Knight 事件遭遇、Globe Head Normal、Turret Operator Weak、Fake Merchant Event Encounter、三个 Battleworn Dummy 事件遭遇、The Lost and Forgotten Normal、Entomancer Elite、Infested Prisms Elite、Knights Elite、Mecha Knight Elite、Vantom Boss、Lagavulin Matriarch Boss、Ovicopter Normal、Soul Fysh Boss、The Obscura Normal、Waterfall Giant Boss、The Kin Boss、The Insatiable Boss、Fabricator Normal、Ceremonial Beast Boss 和 Aeonglass Boss。Aeonglass 验证 Withering Presence 的六牌计数、Wither 状态伤害及逐轮假升级；Fabricator Normal 验证低于四个存活盟友时的生成/打击分支、满场时的消灭招式、两组不重复机器人选取以及四种机器人行为和 High Voltage 增益；Ceremonial Beast 验证 Plow 阈值触发眩晕、清除力量以及 Ringing 对牌组的施加和移除。Personal Hive 按层数把 Dazed 随机加入抽牌堆；Vital Spark 为技能牌施加 Tainted 并在其打出后令玩家暂时承受更多攻击伤害；Knights Elite 验证 Hex、Dampen 及升级牌恢复；Mecha Knight 验证固定招式循环、Artifact、Burn 生成和回合末伤害；Vantom 验证 Slippery、招式循环及 Wound 生成；Lagavulin Matriarch 验证受击/计时两种唤醒、Plating 时机和攻击循环；Ovicopter 验证蛋槽生成顺序、HatchPower 倒数和孵化 HP RNG；Soul Fysh 验证 Beckon 生成、回合末损失 HP 及 Intangible；The Obscura 验证 Parafright 召唤、幻象死亡后复活和三类不重复后续招式；Waterfall Giant 验证蒸汽蓄积、Pressure Gun 递增及致死后的恢复、自爆两阶段；The Kin 验证两只 Follower 的独立开场招式和 Priest 的状态/攻击循环；The Insatiable 验证 Frantic Escape 生成顺序、Sandpit 倒计时与到期终结。新增怪物模型还包括 Aeonglass、Ceremonial Beast、Magi Knight、Spectral Knight、Mecha Knight、Vantom、Lagavulin Matriarch、Ovicopter、Tough Egg、Soul Fysh、The Obscura、Parafright、Waterfall Giant、Kin Follower、Kin Priest、The Insatiable、Fabricator、Guardbot、Noisebot、Stabbot 和 Zapbot。Infested Prisms A0/A10 L3 细节见[本批报告](2026-09-26-encounters-infested-prisms.md)，Knights Elite 细节见[本批报告](2026-09-26-encounters-knights-elite.md)，Mecha Knight 细节见[本批报告](2026-09-26-encounters-mecha-knight.md)，Vantom 细节见[本批报告](2026-09-26-encounters-vantom-boss.md)，Lagavulin Matriarch 细节见[本批报告](2026-09-26-encounters-lagavulin-matriarch.md)，Ovicopter 细节见[本批报告](2026-09-26-encounters-ovicopter.md)，Soul Fysh 细节见[本批报告](2026-09-26-encounters-soul-fysh.md)，The Obscura 细节见[本批报告](2026-09-26-encounters-the-obscura.md)，Waterfall Giant 细节见[本批报告](2026-09-26-encounters-waterfall-giant.md)，The Kin 细节见[本批报告](2026-09-26-encounters-the-kin.md)，The Insatiable 细节见[本批报告](2026-09-26-encounters-insatiable.md)，Fabricator 细节见[本批报告](2026-09-26-encounters-fabricator.md)，Aeonglass 细节见[本批报告](2026-09-26-encounters-aeonglass.md)，Ceremonial Beast 细节见[本批报告](2026-09-26-encounters-ceremonial-beast.md)，其他逐批报告链接见各批计划。

新增怪物模型为 Exoskeleton、Myte、Bowlbug Egg、Bowlbug Silk、Bowlbug Nectar、Bowlbug Rock、Louse Progenitor、Slumbering Beetle、Axebot、Owl Magistrate、Flail Knight、Mysterious Knight 和 Globe Head。新增 Galvanic Power 与 Galvanized 状态牌。Bowlbug 正常遭遇按原版的不重复工虫抽选规则生成 Rock 加两只工虫；弱遭遇固定 Rock 并从 Egg/Nectar 中抽一只。槽位元数据补入 first/middle/last 与 odd/even，供怪物按站位选择初始招式。

Env C ABI 新增按名称重置指定遭遇并运行参考 ISMCTS 的入口、结果 JSON 和 Python 包装。结果包含首动作、搜索耗时、迭代数、深度与用于比较的根结构。根结构目前包括手牌、抽/弃/消耗堆卡牌 ID 顺序、遗物、药水、能力、HP、格挡、能量与敌人身份/状态。

独立游戏 Mod 在主线程捕获战士的 live 根，后台运行 C++ Env 搜索，并只读获取 CombatSolver 完成的搜索结果，在游戏覆盖层和日志中显示双方耗时、工作量、首动作和结构核对结果。外部求解器源码通过固定子模块提供；构建与安装步骤见 `tools/Sts2EnvCompare/README.md`。

## 验证结果

使用 `DOTNET="$HOME/.dotnet/dotnet" python3 tests/l3/l3_combat.py`，种子 `NEW001,NEW002`、角色 `IRONCLAD`、策略种子 `1,2`：

| 遭遇 | 战斗对拍 | 决策状态 |
|---|---:|---:|
| `DEVOTED_SCULPTOR_WEAK` | 4/4 | 71/71 |
| `CHOMPERS_NORMAL` | 4/4 | 80/80 |
| `FROG_KNIGHT_NORMAL` | 4/4 | 88/88 |
| `SPINY_TOAD_NORMAL` | 4/4 | 88/88 |
| `SLIMED_BERSERKER_NORMAL` | 4/4 | 95/95 |
| `TUNNELER_WEAK` | 4/4 | 86/86 |
| `TUNNELER_NORMAL` | 4/4 | 66/66 |
| `SOUL_NEXUS_ELITE` | 4/4 | 59/59 |
| `SCROLLS_OF_BITING_WEAK` | 4/4 | 59/59 |
| `SCROLLS_OF_BITING_NORMAL` | 4/4 | 47/47 |
| `HUNTER_KILLER_NORMAL` | 4/4 | 84/84 |
| **A0 合计** | **44/44** | **823/823** |

另外 `SPINY_TOAD_NORMAL`、`SLIMED_BERSERKER_NORMAL`、`TUNNELER_WEAK`、`TUNNELER_NORMAL`、`SOUL_NEXUS_ELITE`、`SCROLLS_OF_BITING_WEAK`、`SCROLLS_OF_BITING_NORMAL`、`HUNTER_KILLER_NORMAL` 在 A10、种子 `A10001`、策略种子 `1,2` 下合计 **16/16 战斗、274/274 状态**一致。两种难度合计 **60/60 战斗、1097/1097 状态**一致。

`THE_ARCHITECT_EVENT_ENCOUNTER` 使用普通 L3 完整战斗命令时，oracle 在 `max-actions=10` 和 30 的边界均报 `combat did not finish within max-actions`；它有 9999 HP，需为事件战斗设计前缀对拍。此次不把它计入已验证遭遇。`DEPRECATED_ENCOUNTER` 不进入通常战斗，未做 L3 验证。

`scripts/check.sh --quick`：8/8 单元测试通过；三组全局冒烟分别 `death=94 unported=6`、`death=100`、`boss=3 death=97`；退出码 0。该段为阶段中期记录；最终完整门禁结果见文末。

### 后续遭遇批次验证

Exoskeleton、Myte 与 Bowlbug 新增模型经 L3 逐状态对拍，A0/A10 汇总如下：

| 遭遇 | A0 战斗 / 状态 | A10 战斗 / 状态 |
|---|---:|---:|
| EXOSKELETONS_WEAK | 4/4 · 93/93 | 2/2 · 35/35 |
| EXOSKELETONS_NORMAL | 4/4 · 60/60 | 2/2 · 29/29 |
| MYTES_NORMAL | 4/4 · 78/78 | 2/2 · 35/35 |
| BOWLBUGS_WEAK | 4/4 · 66/66 | 2/2 · 29/29 |
| BOWLBUGS_NORMAL | 4/4 · 59/59 | 2/2 · 29/29 |
| **本阶段新增合计** | **20/20 · 356/356** | **10/10 · 157/157** |

| 遭遇 | A0 战斗 / 状态 | A10 战斗 / 状态 |
|---|---:|---:|
| LOUSE_PROGENITOR_NORMAL | 4/4 · 108/108 | 2/2 · 44/44 |
| SLUMBERING_BEETLE_NORMAL | 4/4 · 65/65 | 2/2 · 31/31 |
| AXEBOTS_NORMAL | 4/4 · 82/82 | 2/2 · 37/37 |
| OWL_MAGISTRATE_NORMAL | 4/4 · 78/78 | 2/2 · 34/34 |
| MYSTERIOUS_KNIGHT_EVENT_ENCOUNTER | 4/4 · 73/73 | 2/2 · 34/34 |
| GLOBE_HEAD_NORMAL | 4/4 · 82/82 | 2/2 · 39/39 |
| **本批新增合计** | **24/24 · 488/488** | **12/12 · 219/219** |

加上前面记录的 90/90 战斗和 1610/1610 状态，再加上 TURRET_OPERATOR_WEAK、FAKE_MERCHANT_EVENT_ENCOUNTER、三个 Battleworn Dummy 遭遇、The Lost and Forgotten、Entomancer Elite、Infested Prisms Elite、Knights Elite、Mecha Knight Elite、Vantom Boss、Lagavulin Matriarch Boss、Ovicopter Normal、Soul Fysh Boss、The Obscura Normal、Waterfall Giant Boss、The Kin Boss 和 The Insatiable Boss、Fabricator Normal、Ceremonial Beast Boss 和 Aeonglass Boss、Decimillipede Elite、Thieving Hopper Weak、Queen Boss、Knowledge Demon Boss、Kaiser Crab Boss、Test Subject Boss 的标准 A0/A10 对拍，本报告覆盖范围累计 **288/288 战斗、5297/5297 状态**一致。A0 使用种子 1,2、策略种子 1,2；A10 使用种子 1、策略种子 1,2。Lost/Forgotten 的 A0 样本分别验证两种敌人在死亡时返还属性；Entomancer 验证 Personal Hive 的 Dazed RNG 顺序；Infested Prism 验证 Tainted 技能的回合伤害增幅；Knights Elite 验证 Dampen 的降级与恢复；Mecha Knight 验证 Burn 留手伤害；Vantom 验证 Wound 生成与 Slippery 消耗；Lagavulin Matriarch 验证受击唤醒；Ovicopter 验证蛋生成顺序和孵化；Soul Fysh 验证 Beckon 生成及 Intangible；The Obscura 验证幻象召唤、复活以及主怪招式分支；Waterfall Giant 验证 Steam Eruption 触发、增压招式和延迟自爆；The Kin 验证 Follower 和 Priest 全循环及目标状态；The Insatiable 验证 Frantic Escape/RNG 插入及 Sandpit 到期效果。Knights Elite 另有升级牌探测 6/6 战斗、78/78 状态一致。Lagavulin Matriarch 另有纯防御计时场景 2/2 战斗、79/79 状态一致；Ovicopter 另有孵化场景 2/2 战斗、49/49 状态一致；The Insatiable 另有拖长防守并验证 Sandpit 到期的 A0 场景 2/2 战斗、47/47 状态一致。Entomancer、Infested Prisms、Knights Elite 和 Mecha Knight 批次后的 quick gate 均通过；Vantom、Lagavulin Matriarch、Ovicopter、Soul Fysh、The Obscura、Waterfall Giant、The Kin 和 The Insatiable、Fabricator、Ceremonial Beast 与 Aeonglass 批次的门禁结果见逐批报告。

### The Obscura Normal 新增对拍

| 配置 | A0 战斗 / 状态 | A10 战斗 / 状态 |
|---|---:|---:|
| 默认 Ironclad 牌组 | 4/4 · 78/78 | 2/2 · 32/32 |

### Waterfall Giant Boss 新增对拍

| 配置 | A0 战斗 / 状态 | A10 战斗 / 状态 |
|---|---:|---:|
| 默认 Ironclad 牌组 | 4/4 · 132/132 | 2/2 · 67/67 |

### The Kin Boss 新增对拍

| 配置 | A0 战斗 / 状态 | A10 战斗 / 状态 |
|---|---:|---:|
| 默认 Ironclad 牌组 | 4/4 · 77/77 | 2/2 · 35/35 |

### The Insatiable Boss 新增对拍

| 配置 | A0 战斗 / 状态 | A10 战斗 / 状态 |
|---|---:|---:|
| 默认 Ironclad 牌组 | 4/4 · 88/88 | 2/2 · 39/39 |

额外 Sandpit 到期探测：20 张 Defend 和延长生存遗物，A0 2/2 · 47/47；覆盖到期后终结玩家的路径。

### Turret Operator Weak 新增对拍

| 遭遇 | A0 战斗 / 状态 | A10 战斗 / 状态 |
|---|---:|---:|
| TURRET_OPERATOR_WEAK | 4/4 · 69/69 | 2/2 · 35/35 |

### Fake Merchant Event Encounter 新增对拍

| 遭遇 | A0 战斗 / 状态 | A10 战斗 / 状态 |
|---|---:|---:|
| FAKE_MERCHANT_EVENT_ENCOUNTER | 4/4 · 98/98 | 2/2 · 47/47 |

### Battleworn Dummy 事件遭遇新增对拍

| 遭遇 | A0 战斗 / 状态 | A10 战斗 / 状态 |
|---|---:|---:|
| BATTLEWORN_DUMMY_EVENT_V1_ENCOUNTER | 4/4 · 47/47 | 2/2 · 22/22 |
| BATTLEWORN_DUMMY_EVENT_V2_ENCOUNTER | 4/4 · 47/47 | 2/2 · 22/22 |
| BATTLEWORN_DUMMY_EVENT_V3_ENCOUNTER | 4/4 · 47/47 | 2/2 · 22/22 |

### The Lost and Forgotten Normal 新增对拍

| 配置 | A0 战斗 / 状态 | A10 战斗 / 状态 |
|---|---:|---:|
| 10 × `BLUDGEON+1`，`AKABEKO,TUNGSTEN_ROD` | 4/4 · 48/48 | 2/2 · 24/24 |

### Entomancer Elite 新增对拍

| 遭遇 | A0 战斗 / 状态 | A10 战斗 / 状态 |
|---|---:|---:|
| ENTOMANCER_ELITE | 4/4 · 85/85 | 2/2 · 44/44 |

### Infested Prisms Elite 新增对拍

| 遭遇 | A0 战斗 / 状态 | A10 战斗 / 状态 |
|---|---:|---:|
| INFESTED_PRISMS_ELITE | 4/4 · 88/88 | 2/2 · 39/39 |

### Knights Elite 新增对拍

| 配置 | A0 战斗 / 状态 | A10 战斗 / 状态 |
|---|---:|---:|
| 默认 Ironclad 牌组 | 4/4 · 52/52 | 2/2 · 30/30 |

额外升级牌探测：Impervious/Bludgeon 场景 4/4 · 46/46；成长中的 Rampage 场景 2/2 · 32/32。两组均逐状态一致，并覆盖 Magi Knight 施加 Dampen、降级卡牌、击杀施法者后恢复原升级以及随后打出 Rampage。

### Mecha Knight Elite 新增对拍

| 配置 | A0 战斗 / 状态 | A10 战斗 / 状态 |
|---|---:|---:|
| 默认 Ironclad 牌组 | 4/4 · 81/81 | 2/2 · 35/35 |

### Vantom Boss 新增对拍

| 配置 | A0 战斗 / 状态 | A10 战斗 / 状态 |
|---|---:|---:|
| 默认 Ironclad 牌组 | 4/4 · 114/114 | 2/2 · 44/44 |

### Lagavulin Matriarch Boss 新增对拍

| 配置 | A0 战斗 / 状态 | A10 战斗 / 状态 |
|---|---:|---:|
| 默认 Ironclad 牌组 | 4/4 · 115/115 | 2/2 · 53/53 |

额外计时唤醒探测：20 张 Defend 与延长生存遗物，A0 2/2 · 79/79；全程不造成伤害，验证 Asleep 倒数及最后一层 Plating 的移除时机。

### Ovicopter Normal 新增对拍

| 配置 | A0 战斗 / 状态 | A10 战斗 / 状态 |
|---|---:|---:|
| 默认 Ironclad 牌组 | 4/4 · 72/72 | 2/2 · 34/34 |

额外孵化探测：20 张 Defend 与延长生存遗物，A0 2/2 · 49/49；覆盖 Tough Egg 的 HatchPower 倒数、孵化 HP RNG 和 Nibble 招式。

### Soul Fysh Boss 新增对拍

| 配置 | A0 战斗 / 状态 | A10 战斗 / 状态 |
|---|---:|---:|
| 默认 Ironclad 牌组 | 4/4 · 96/96 | 2/2 · 43/43 |

### 真实游戏内双搜索

CombatSolver 自带的 `tools/run-unattended-test.sh` 使用隔离的正版 Linux 游戏快照运行 `IRONCLAD`、`CULTISTS_NORMAL`，`--force-short-search-only --short-search-budget-override-milliseconds 1500 --hold-after-initial-search --keep-game-open`。运行结果 `Passed`；游戏日志有 Mod 清单、程序集加载和 `[Sts2EnvCompare] READY`。从最终 `DUAL006` 的同次战斗日志摘录：

```text
[Sts2EnvCompare] ENV turn=1 encounter=CULTISTS_NORMAL structural_root_match=True live_unchanged=True iterations=100 depth=12 elapsed_ms=70.857 action=PlayCard:STRIKE_IRONCLAD:target=1
[Sts2EnvCompare] TWO_SOLVERS turn=1 encounter=CULTISTS_NORMAL env_ms=70.857 native_ms=346.025 native_over_env=4.883 env_rollouts=100 native_expanded=3 env_action=PlayCard:STRIKE_IRONCLAD:target=1 native_action=PlayCard:STRIKE_IRONCLAD:target=1 action_agree=True root_check=structural_only budgets=not_equivalent
```

较早的 `DUAL005` 同样记录 `structural_root_match=True`、双方首动作一致，Env 79.744 ms / 100 rollouts，CombatSolver 309.534 ms / 3 nodes。`DUAL004` 在 Env 搜索完成前实机状态已变化，Mod 输出 `COMPARISON_SKIPPED reason=root_or_live_mismatch`；该次没有生成双求解器比较。带 500 次 rollout 的 `DUAL003` 记录 Env 349.477 ms、CombatSolver 318.707 ms、结构根一致。以上都是不同预算、不同工作量定义的原始样例，**不能据此宣称 Env 的算法提速**。

本轮将根检查改成“先检查、匹配才搜索”后，更新构建再次通过隔离正版游戏测试。场景 `MCC-DUAL-PREFLIGHT` / seed `DUAL001` / A0 状态 `Passed`；日志显示 `structural_root_match=True`、`live_unchanged=True`，Env 108.911 ms / 100 rollouts，CombatSolver 280.522 ms / 3 expanded nodes。Bash 与 Strike 首动作不同；这说明两条策略给出不同建议，不构成模拟状态分歧。结构子集和不同预算限制仍适用，完整细节见[双求解器对拍报告](2026-09-26-combat-solver-env-bridge.md)。

## 尚未完成与限制

1. 正式遭遇模型 **90/90** 已齐全；怪物内容模型头文件仍缺 **17/126**，包含测试辅助和其他用途模型。新增遭遇的专项 L3 验证覆盖指定种子，尚未逐一做大规模随机路线回归。
2. Env 目前通过种子重新生成首回合，**没有导入任意 live 战斗完整状态**。已核对的结构字段不能覆盖卡牌实例属性、遗物内部计数、怪物内部状态、RNG、回合历史等；不能证明两个算法从严格相同的完整状态搜索。多回合和复杂牌组会出现不匹配，应跳过比较。
3. CombatSolver beam search 的 `expanded` 与 Env ISMCTS 的 `rollouts` 不等价。需要同一完整根、明确预算与多次重复测量才能回答速度提升。当前游戏覆盖层只是开发版，尚未做可视化窗口验收，也未验证 Env 动作在任意实机状态的执行。
4. Mod 通过反射只读 CombatSolver 的内部完成结果，依赖该固定提交的成员名；升级子模块时要重新验证。当前首动作按卡牌 ID 与目标 combat ID 比较，同名重复牌的实例身份尚未纳入。

结论：CombatSolver 已可复现地接入本仓库，双搜索 Mod 确实在真实游戏进程运行并给出首回合原始比较；正式遭遇模型已完成 90/90；完整 live 状态导入、可推广的偏差与速度结论仍待完成。

### Fabricator Normal 新增对拍

| 配置 | A0 战斗 / 状态 | A10 战斗 / 状态 |
|---|---:|---:|
| 默认 Ironclad 牌组 | 4/4 · 60/60 | 2/2 · 29/29 |

FABRIC01/02 覆盖四种机器人及 Fabricator 的 `DISINTEGRATE_MOVE`、`FABRICATE_MOVE`、`FABRICATING_STRIKE_MOVE` 分支。`scripts/check.sh --quick`：8/8 CTest 通过，三组全局冒烟通过，退出码 0。

### Ceremonial Beast Boss 新增对拍

| 配置 | A0 战斗 / 状态 | A10 战斗 / 状态 |
|---|---:|---:|
| 默认 Ironclad 牌组 | 4/4 · 76/76 | 2/2 · 39/39 |

10 张 `BLUDGEON+1` 的 A0/A10 额外探针各 2/2 战斗、22/22 与 26/26 状态一致，确保覆盖 Plow 阈值眩晕及 `STUNNED → BEAST_CRY → STOMP → CRUSH` 二阶段循环。

### Aeonglass Boss 新增对拍

| 配置 | A0 战斗 / 状态 | A10 战斗 / 状态 |
|---|---:|---:|
| 默认 Ironclad 牌组 | 4/4 · 69/69 | 2/2 · 35/35 |
| 20 × `DEFEND_IRONCLAD`、`TUNGSTEN_ROD,LIZARD_TAIL` | 2/2 · 54/54 | 2/2 · 49/49 |

长局探针覆盖 `EBB_MOVE`、`EYE_LASERS_MOVE`、反复 `INCREASING_INTENSITY_MOVE`，Wither 生成/回合末伤害，以及现有 Wither 的累积假升级。

## Decimillipede Elite 补充（2026-09-26）

新增三节段模型、开局招式 RNG、偶数且唯一的 HP，以及 Reattach 的保留尸体、跳过行动、25 HP 复活和复活期间受击保护。Feed 与 Hand of Greed 按攻击前的能力列表检查 Fatal，并以 DamageResult.WasTargetKilled 判断击杀。

| 配置 | 种子 / 策略 | 战斗 / 状态 | 结果 |
|---|---|---:|---|
| A0 默认牌组 | DECI01,DECI02 / 1,2 | 4/4 · 59/59 | 一致 |
| A10 默认牌组 | DECI10 / 1,2 | 2/2 · 29/29 | 一致 |

专项探针另有 14/14 战斗、250/250 状态一致，覆盖 A0/A10 复活、Fatal 卡牌及全体死亡后的战斗结束，不并入标准累计。命令、牌组和验证边界见 [Decimillipede 报告](2026-09-26-encounters-decimillipede.md)。快速门禁通过。

## Thieving Hopper Weak 补充（2026-09-26）

移植偷牌优先级及 RNG、Swipe 移除牌组原牌、Flutter 半伤和五次未被格挡的攻击后的眩晕、Escape Artist 计时及完整逃跑循环。新增 SpecialCard 奖励直接领取；击杀后返还被盗原牌，逃跑不返还。

标准 A0 HOP01,HOP02 / 策略 1,2：4/4 战斗、66/66 状态；A10 HOP10 / 策略 1,2：2/2 战斗、35/35 状态。专项 16/16 战斗、209/209 状态独立记录。L3 结束记录新增 run 状态和额外奖励，`--take-special-rewards` 能核对领取后的原牌入组；不修改 oracle 游戏逻辑。该观察扩展还重新验证了 Decimillipede Fatal 探针 6/6、119/119，现直接核对结束时金币。命令及边界见 [Thieving Hopper 报告](2026-09-26-encounters-thieving-hopper.md)。

## Queen Boss 补充（2026-09-26）

新增 Queen 与 Torch Head Amalgam、完整招式循环、Amalgam 死亡后的 Queen 转相，Chains of Binding 与 Bound。战斗历史增加本玩家回合的 Bound 施加次数；L3 观察新增手牌附魔标记以直接验证施加与清除。标准 A0 4/4 ·57/57、A10 2/2 ·28/28；专项 A0/A10 三组 8/8 ·111/111。累计 **276/276 战斗、5114/5114 状态**。详情见 [Queen 报告](2026-09-26-encounters-queen.md)。

## Knowledge Demon Boss 补充（2026-09-26）

实现三轮 Curse of Knowledge 选择、Disintegration/Mind Rot/Sloth/Waste Away 即时效果及各自能力、固定攻击和 Ponder 回血力量循环。四张选择牌均为遭遇专用 Status，不涉及其他角色牌。标准 A0 4/4 ·77/77、A10 2/2 ·37/37；长局 A0 6/6 ·272/272、A10 6/6 ·235/235。累计 **276/276 战斗、5114/5114 状态**；长局独立记录。详情见 [Knowledge Demon 报告](2026-09-26-encounters-knowledge-demon.md)。

## Kaiser Crab Boss 补充（2026-09-26）

新增 Crusher 与 Rocket 的五招循环、Surrounded 转向和反侧受击、Crab Rage 死亡增益。无头 Oracle 仅跳过二者死亡时纯视听钩子，避免原版音频单例缺失异常；L3 增加 Play 牌堆观察。标准 A0 4/4 ·64/64、A10 2/2 ·28/28；长局和双侧击杀探针 14/14 ·320/320。累计 **282/282 战斗、5206/5206 状态**，正式遭遇 **89/90**。详情见 [Kaiser Crab 报告](2026-09-26-encounters-kaiser-crab.md)。

## Test Subject Boss 与正式遭遇收尾（2026-09-26）

新增三形态 Test Subject、两次倒地复活、Enrage、Painful Stabs、Nemesis，以及动态多爪攻击、Burn 生成和各阶段招式。标准 A0 4/4 ·65/65、A10 2/2 ·26/26；专项三形态、爪击递增和防御长局 22/22 ·482/482。正式遭遇模型与原版非 Mock 名称集合完全相同，**90/90**；标准累计 **288/288 战斗、5297/5297 状态**。详情见 [Test Subject 报告](2026-09-26-encounters-test-subject.md)。

最终完整 `scripts/check.sh` 通过：CTest 8/8，L2 200 次运行零差异，L3 A0/A10 各 40/40 场、784/784 与 827/827 状态一致，L4 不带 Neow 与带 Neow 各 40/40 次 run、1958/1958 与 2131/2131 决策一致。此结果覆盖门禁的固定种子集合；不等于 90 个遭遇都做过大规模随机测试。
