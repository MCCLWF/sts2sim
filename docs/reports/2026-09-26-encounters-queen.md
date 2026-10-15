# Queen Boss 移植报告

日期：2026-09-26
分支：`feat/all-encounters-comparison-mod`
基线：v0.111.0；真值为反编译源码与 Sts2Oracle。

## 实现

新增 Queen Boss、Queen、Torch Head Amalgam、Chains of Binding Power 和 Bound。Amalgam 位于 `amalgam` 槽，Queen 位于 `queen` 槽。两只怪物的完整招式循环与 A0/A10 数值已移植：Queen 开场 Puppet Strings 施加 3 层 Chains，随后 You're Mine 施加 99 层三种负面能力；Amalgam 存活时循环 Burn Bright For Me，死后转为五段 Off With Your Head→Execution→Enrage。Amalgam 先有 Minion，依固定五招循环。Queen 在 Amalgam 死亡时立即更新当前 Burn 招式为 Enrage。

Chains 在本玩家回合每次抽到可附魔卡牌时检查当回合 Bound 施加记录，尚未达到 Amount 时以 Amount 施加 Bound。第一张 Bound 牌可打出，此后本回合禁止打出其它 Bound 牌；副本出牌不消耗额度。玩家回合结束清除所有 Bound，重置标记。固定状态新增当回合施加计数，并在玩家回合开始归零。

L3 快照新增 `hand_afflictions` 字段。原生与模拟器独立读取手牌附魔 ID，从而直接对拍 Bound，而非只由牌名与费用间接判断。未修改其他角色卡牌。动画、音效、对白与镜头不进入无头状态。

## Oracle 对拍

| 配置 | 种子 / 策略种子 | 战斗 / 状态 | 结果 |
|---|---|---:|---|
| A0 默认牌组 | QUEEN01,QUEEN02 / 1,2 | 4/4 · 57/57 | 一致 |
| A10 默认牌组 | QUEEN10 / 1,2 | 2/2 · 28/28 | 一致 |
| A0 高伤转相 | QUEENP01,QUEENP02 / 1,2 | 4/4 · 51/51 | 一致 |
| A10 高伤转相 | QUEENP10 / 1,2 | 2/2 · 16/16 | 一致 |
| A0 多抽牌 Bound | QUEEND01 / 1,2 | 2/2 · 44/44 | 一致 |

标准累计增加 6 场、85 个状态：**270/270 战斗、5000/5000 状态**。专项 8 场、111 状态独立记录。高伤探针覆盖 Amalgam 离场后 Queen 存活以及 Enrage/Execution；多抽牌探针包含 Queen 长时间 Burn 循环、Amalgam 五招以及 53 次手牌 Bound 状态记录。

标准命令：

```bash
DOTNET="$HOME/.dotnet/dotnet" python3 tests/l3/l3_combat.py --seeds QUEEN01,QUEEN02 --characters IRONCLAD --policy-seeds 1,2 --ascension 0 --encounter QUEEN_BOSS -v
DOTNET="$HOME/.dotnet/dotnet" python3 tests/l3/l3_combat.py --seeds QUEEN10 --characters IRONCLAD --policy-seeds 1,2 --ascension 10 --encounter QUEEN_BOSS -v
```

专项：QUEENP01,QUEENP02 A0 与 QUEENP10 A10 使用 60 张 PERFECTED_STRIKE+1、20 张 DEFEND_IRONCLAD+1；QUEEND01 A0 使用 8 张 POMMEL_STRIKE+1、2 张 DEFEND_IRONCLAD+1。所有专项加 `--relics TUNGSTEN_ROD,LIZARD_TAIL,ANCHOR,HORN_CLEAT,CAPTAINS_WHEEL`，`--deck` 为相应逗号展开牌组。trace 存在本机临时目录，未提交。

## 门禁与剩余

- 分发生成、C++ 构建通过；oracle .NET 构建 0 错误、原有 CS8604 nullable 警告 1 个。
- `scripts/check.sh --quick` 退出码 0：CTest 8/8；冒烟 death=94 unported=6、death=100、boss=3 death=97。
- `git diff --check` 通过；完整 L2/L3/L4 未在本批重跑。
- 正式遭遇 **87/90**、怪物内容模型头文件 **105/126**；尚缺 KaiserCrabBoss、KnowledgeDemonBoss、TestSubjectBoss。
