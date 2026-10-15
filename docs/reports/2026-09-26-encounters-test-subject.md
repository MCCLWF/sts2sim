# Test Subject Boss 移植报告

日期：2026-09-26  
分支：`feat/all-encounters-comparison-mod`  
基线：v0.111.0；真值为游戏源码与 Sts2Oracle。

## 实现

新增 Test Subject Boss 单怪遭遇和 Test Subject 三形态：100/200/300 HP（A10 为 111/212/313）。第一形态 Bite 与 Skull Bash 交替；第一次死亡后进入 Respawn，第二形态反复使用 Multi Claw，攻击次数从 3 次逐次增长。第二次死亡后再 Respawn，第三形态循环 Lacerate、Big Pounce、Burning Growl。最后一次死亡结束战斗。

新增 Adaptable、Enrage、Painful Stabs、Nemesis；每次倒地更新 run 的 Test Subject 击杀计数。Adaptable 保留前两次死亡后的尸体、阻止倒地期间受击、阻止战斗提前结束；Enrage 在技能牌打出后增加力量；Painful Stabs 在未格挡的攻击命中后生成 Wound；Nemesis 在第三形态的敌方回合末交替施加/移除 Intangible。Burning Growl 将遭遇专用 Burn 加到弃牌堆并增加力量。未新增其他角色的可玩卡牌。

## Oracle 逐状态对拍

| 配置 | 种子 / 策略 | 战斗 / 状态 | 结果 |
|---|---|---:|---|
| A0 默认牌组 | TEST01,TEST02 / 1,2 | 4/4 · 65/65 | 一致 |
| A10 默认牌组 | TEST10 / 1,2 | 2/2 · 26/26 | 一致 |
| A0 高伤三形态 | TESTP01,TESTP02 / 1,2,3 | 6/6 · 126/126 | 一致 |
| A10 高伤三形态 | TESTP10 / 1,2,3 | 3/3 · 65/65 | 一致 |
| A0 第二形态长局 | TESTM01,TESTM02 / 1,2,3 | 6/6 · 134/134 | 一致 |
| A10 第二形态长局 | TESTM10 / 1,2,3 | 3/3 · 59/59 | 一致 |
| A0 防御长局 | TESTL01,TESTL02 / 1,2 | 4/4 · 98/98 | 一致 |

标准累计增加 6 场、91 状态：**288/288 战斗、5297/5297 状态**。专项另计 22 场、482 状态。高伤样本到达三形态并完整结束；逐状态可见两次 Respawn、run 击杀计数 0→1→2、Wound、Burn、Nemesis/Intangible 交替。较慢样本的 Multi Claw 意图从 3、4、5 到 6 次递增。

高伤样本使用 60 张 `PERFECTED_STRIKE+1` 与 20 张 `DEFEND_IRONCLAD+1`；第二形态长局改用 12 张 `PERFECTED_STRIKE+1` 与 8 张 `DEFEND_IRONCLAD+1`；防御长局为 15 张 `DEFEND_IRONCLAD+1` 与 5 张 `BODY_SLAM+1`。专项样本另加 `TUNGSTEN_ROD,LIZARD_TAIL,ANCHOR,HORN_CLEAT,CAPTAINS_WHEEL`。通过 `tests/l3/l3_combat.py --encounter TEST_SUBJECT_BOSS` 比较全部决策状态及战斗结尾。为核对复活进度，L3 双方状态增加 `test_subject_kills` 字段。

## 门禁与范围

- 模型分发生成、C++ 构建、Oracle Release 构建通过；`scripts/check.sh --quick` 退出码 0：CTest 8/8，三组冒烟为 `death=94 unported=6`、`death=100`、`boss=3 death=97`。
- 正式遭遇模型 **90/90**，与原版 90 个具象正式遭遇名集合完全相同。原版另外有 8 个测试用 Mock 遭遇和 1 个 BattlewornDummyEventEncounter 抽象基类，不计入正式遭遇目标。
- 怪物内容模型 **109/126**。其余 17 个怪物内容类包括测试辅助、抽象基类或其他用途；这不改变正式遭遇模型 90/90 的判断。
