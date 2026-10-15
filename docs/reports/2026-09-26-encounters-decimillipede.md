# Decimillipede Elite 遭遇移植报告

日期：2026-09-26  
分支：`feat/all-encounters-comparison-mod`  
游戏基线：v0.111.0；真值为反编译源码与 `Sts2Oracle`。

## 实现

- 新增 `DECIMILLIPEDE_ELITE`、Front/Middle/Back 三个节段及共享逻辑；只抽一次遭遇 RNG，以错开一位的顺序分配三种开局招式。
- 入场后按源代码顺序调整各节段 HP 为不重复偶数，超过最大值时回绕到最小值，并施加 25 层 Reattach。
- 完整移植 Writhe 双段攻击、Bulk 攻击后力量、Constrict 攻击后虚弱及固定循环。
- 死亡节段保留尸体和 Reattach 能力，进入 DEAD_MOVE，随后 REATTACH_MOVE；有存活兄弟节段时恢复 25 HP，再按不连续重复的随机分支返回攻击循环。复活期间拒绝受击，最后一个节段死亡时结束战斗。
- Feed、Hand of Greed 现于攻击前读取能力的 `ShouldOwnerDeathTriggerFatal`，以攻击结果的 `WasTargetKilled` 判定奖励。这也修正了 Minion 上的相同问题。未实现其他角色卡牌。
- 动画、场景、音效及纹理变化不进入无头状态。

## Oracle 对拍

| 配置 | 种子 / 策略种子 | 战斗 / 状态 | 结果 |
|---|---|---:|---|
| A0 默认牌组 | DECI01,DECI02 / 1,2 | 4/4 · 59/59 | 一致 |
| A10 默认牌组 | DECI10 / 1,2 | 2/2 · 29/29 | 一致 |
| A0/A10 Bludgeon 防守探针 | DECIP01,DECIP101 / 1,2 | 4/4 · 87/87 | 一致 |
| A10 延长复活探针 | DECIR10 / 1,2 | 2/2 · 42/42 | 一致 |
| A0 Fatal 交互探针 | DECIF01,DECIF02,DECIF03 / 1,2 | 6/6 · 119/119 | 一致 |
| A0 全体击杀探针 | DECIWIN / 1,2 | 2/2 · 2/2 | 一致 |

标准累计增加 6 场、88 个状态：总计 **258/258 战斗、4814/4814 状态**。专项 14 场、250 状态独立记录。trace 确认 A0 与 A10 均有 0→25 HP 复活，Fatal 探针有 Feed/Hand of Greed 击杀和多次重接；全体击杀探针在第一次攻击后胜利。

命令（全部 `--characters IRONCLAD --encounter DECIMILLIPEDE_ELITE --policy-seeds 1,2 -v`）：

```bash
DOTNET="$HOME/.dotnet/dotnet" python3 tests/l3/l3_combat.py --seeds DECI01,DECI02 --ascension 0 --characters IRONCLAD --encounter DECIMILLIPEDE_ELITE --policy-seeds 1,2 -v
DOTNET="$HOME/.dotnet/dotnet" python3 tests/l3/l3_combat.py --seeds DECI10 --ascension 10 --characters IRONCLAD --encounter DECIMILLIPEDE_ELITE --policy-seeds 1,2 -v
```

专项配置：

- DECIP01 A0、DECIP101 A10：2 张 BLUDGEON+1、8 张 DEFEND_IRONCLAD+1；额外遗物 TUNGSTEN_ROD,LIZARD_TAIL。
- DECIR10 A10：相同牌组；额外遗物 TUNGSTEN_ROD,LIZARD_TAIL,ANCHOR,HORN_CLEAT,CAPTAINS_WHEEL。
- DECIF01,DECIF02,DECIF03 A0：4 张 BLUDGEON+1、3 张 FEED+1、3 张 HAND_OF_GREED+1；额外遗物同 DECIR10。
- DECIWIN A0：5 张 WHIRLWIND+1；额外遗物 CHEMICAL_X,AKABEKO。

以上配置以逗号展开传给 `--deck`、`--relics`。专项 trace 保存在本机临时目录，未提交。L3 状态包含 HP/最大 HP、牌堆、能力、意图、RNG；未输出金币，因此 Hand of Greed 的金币规则另外按原生 OnPlay 的 Fatal 判断及攻击结果逐句审查，不能宣称 L3 直接核对了金币数值。

## 门禁与覆盖

- `python3 tools/gen/gen_dispatch.py`、`cmake --build build -j4 --target sts2sim_cli` 通过。
- `scripts/check.sh --quick` 退出码 0：CTest 8/8；冒烟 death=94 unported=6、death=100、boss=3 death=97。
- `git diff --check` 通过。完整 L2/L3/L4 门禁未在本批重跑。
- 正式遭遇 **85/90**；怪物内容模型头文件 **102/126**。共享逻辑头文件不计为内容模型。
- 尚缺 KaiserCrabBoss、KnowledgeDemonBoss、QueenBoss、TestSubjectBoss、ThievingHopperWeak；全遭遇目标仍在进行。
