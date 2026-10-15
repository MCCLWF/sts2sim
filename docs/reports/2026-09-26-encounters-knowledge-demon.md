# Knowledge Demon Boss 移植报告

日期：2026-09-26
分支：`feat/all-encounters-comparison-mod`
基线：v0.111.0；真值为反编译源码和 Sts2Oracle。

## 实现

新增 Knowledge Demon Boss 和 Knowledge Demon。怪物以 Curse of Knowledge 开场，之后依次 Slap、Knowledge Overwhelming 三段攻击、Ponder 攻击后治疗 30 HP 并获得 2/3 力量。前三轮 Ponder 后重走 Curse；第三轮选择后只循环后三招。Burnt 视觉状态保留于可复制的怪物扩展字段。

Curse 第一次提供 Disintegration/Mind Rot，第二次提供 Disintegration/Sloth，第三次提供 Disintegration/Waste Away。每轮独立创建两张 Status 候选牌，按原始顺序走卡牌选择点，并立即施加选中牌的能力。Disintegration 当轮数值 6/7/8，在玩家侧回合结束时造成无强化伤害；Mind Rot 减少回合抽牌；Sloth 限制回合打牌数；Waste Away 减少最大能量。这四张牌均为遭遇专用 Status，不是其他可玩角色的卡牌。动画、音效和台词不进入无头状态。

## Oracle 对拍

| 配置 | 种子 / 策略种子 | 战斗 / 状态 | 结果 |
|---|---|---:|---|
| A0 默认牌组 | KNOW01,KNOW02 / 1,2 | 4/4 · 77/77 | 一致 |
| A10 默认牌组 | KNOW10 / 1,2 | 2/2 · 37/37 | 一致 |
| A0 长局 | KNOWP01,KNOWP02 / 1,2,3 | 6/6 · 272/272 | 一致 |
| A10 长局 | KNOWP101,KNOWP102 / 1,2,3 | 6/6 · 235/235 | 一致 |

标准累计增加 6 场、114 状态：**276/276 战斗、5114/5114 状态**。长局另外 12 场、507 状态独立记录。两个升天档次均各有三组候选出现 6 次；两种选择分支都出现，四种即时能力也都出现在逐决策状态里。长局检验 3 次诅咒后无后续 Curse，Ponder 回血和力量继续叠加。

标准命令：

```bash
DOTNET="$HOME/.dotnet/dotnet" python3 tests/l3/l3_combat.py --seeds KNOW01,KNOW02 --characters IRONCLAD --policy-seeds 1,2 --ascension 0 --encounter KNOWLEDGE_DEMON_BOSS -v
DOTNET="$HOME/.dotnet/dotnet" python3 tests/l3/l3_combat.py --seeds KNOW10 --characters IRONCLAD --policy-seeds 1,2 --ascension 10 --encounter KNOWLEDGE_DEMON_BOSS -v
```

长局使用 15 张 DEFEND_IRONCLAD+1、5 张 BODY_SLAM+1 的逗号展开 `--deck`，另加 `--relics TUNGSTEN_ROD,LIZARD_TAIL,ANCHOR,HORN_CLEAT,CAPTAINS_WHEEL`、`--max-actions 500`；A0 种子 KNOWP01,KNOWP02，A10 种子 KNOWP101,KNOWP102，策略 1,2,3。trace 保存在本机临时目录，未提交。

## 门禁与剩余

- 模型分发生成、C++ 构建通过；`scripts/check.sh --quick` 退出码 0：CTest 8/8，冒烟 death=94 unported=6、death=100、boss=3 death=97。
- `git diff --check` 通过；完整 L2/L3/L4 未在本批重跑。
- 正式遭遇 **88/90**、怪物内容模型头文件 **106/126**；尚缺 KaiserCrabBoss、TestSubjectBoss。
