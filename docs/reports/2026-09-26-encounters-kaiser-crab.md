# Kaiser Crab Boss 移植报告

日期：2026-09-26  
分支：`feat/all-encounters-comparison-mod`  
基线：v0.111.0；真值为游戏源码与 Sts2Oracle。

## 实现

新增 Kaiser Crab Boss 固定双怪遭遇、Crusher 和 Rocket。各自按原版五招循环，伤害与力量数值随升天变化。开局按原版顺序施加 Back Attack、Crab Rage 与玩家的 Surrounded。Surrounded 根据出牌目标转向，反侧攻击伤害乘 1.5；蟹钳死亡时幸存者获得 6 力量与 99 格挡。音效和背景动画不参与无头模拟。

Oracle 为无界面死亡加了窄范围补丁：Crusher/Rocket 的 `BeforeDeath` 仅执行音频/背景动画，却直接访问不存在的 `NAudioManager.Instance`，使原版死亡过程抛异常。补丁只跳过这两个纯视听方法，保留 `CreatureCmd` 的死亡、能力钩子和战斗结算。L3 状态新增 Play 牌堆观察，用来辨别击杀牌执行中断。

## 逐状态对拍

| 配置 | 种子 / 策略 | 战斗 / 状态 | 结果 |
|---|---|---:|---|
| A0 默认牌组 | CRAB01,CRAB02 / 1,2 | 4/4 · 64/64 | 一致 |
| A10 默认牌组 | CRAB10 / 1,2 | 2/2 · 28/28 | 一致 |
| A0 长局 | CRABL01,CRABL02 / 1,2 | 4/4 · 176/176 | 一致 |
| A10 长局 | CRABL10 / 1,2 | 2/2 · 63/63 | 一致 |
| A0 击杀双侧 | CRABK01,CRABK02,CRABK03 / 1,2 | 6/6 · 60/60 | 一致 |
| A10 击杀双侧 | CRABK10 / 1,2 | 2/2 · 21/21 | 一致 |

标准累计增加 6 场、92 状态，达到 **282/282 战斗、5206/5206 状态**。长局与击杀探针另计 14 场、320 状态。长局覆盖两只蟹钳所有十招；击杀探针覆盖两侧先死、幸存者 6 力量/99 格挡，以及完整战斗结束。

长局使用 15 张 `DEFEND_IRONCLAD+1`、5 张 `BODY_SLAM+1`；击杀探针使用 60 张 `PERFECTED_STRIKE+1`、20 张 `DEFEND_IRONCLAD+1`。两者都另加 `TUNGSTEN_ROD,LIZARD_TAIL,ANCHOR,HORN_CLEAT,CAPTAINS_WHEEL`。所有验证都由 `tests/l3/l3_combat.py --encounter KAISER_CRAB_BOSS` 驱动，升天分别为 0、10。

## 门禁与剩余

- 分发生成、Oracle Release 构建、C++ 构建通过；`scripts/check.sh --quick` 退出码 0：CTest 8/8，三组冒烟为 `death=94 unported=6`、`death=100`、`boss=3 death=97`。
- `git diff --check` 通过。尚未重跑完整 L2/L3/L4 门禁。
- 正式遭遇 **89/90**；最后剩 `TestSubjectBoss`。怪物内容模型进度 **108/126**，其余模型并不都属于正式遭遇目标。
