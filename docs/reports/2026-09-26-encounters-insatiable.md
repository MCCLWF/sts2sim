# The Insatiable Boss 遭遇移植报告

日期：2026-09-26  
分支：`feat/all-encounters-comparison-mod`  
游戏基线：v0.111.0；真值为反编译源码与 `Sts2Oracle`。

## 前置与范围

本批涉及 The Insatiable、SandpitPower、Frantic Escape 状态牌，以及沙坑倒计时结束时的致死效果。

## 结果

已接入 `THE_INSATIABLE_BOSS` 和 Frantic Escape 状态牌。Liquify Ground 给玩家施加 4 层 Sandpit，并依次把 3 张 Frantic Escape 加入随机抽牌堆位置、再把 3 张加到随机弃牌堆位置。Frantic Escape 的能量费用在本场战斗永久增加 1；在 Sandpit 到期前打出会将倒计时加 1。

Sandpit 在敌方回合开始时递减；如果期限结束时 The Insatiable 和目标玩家仍活着，则触发终结。若玩家先击败 Boss 或玩家已经死亡，移除 Sandpit 不会造成额外死亡。音乐、沙坑位置调整和终结动画是表现逻辑，不记录在模拟状态。

正式遭遇覆盖提升至 **81/90**，怪物模型头文件提升至 **92/126**，尚缺 9 个遭遇和 34 个怪物模型头文件。

## Oracle 对拍与检查

| 配置 | 战斗 / 状态 | 结果 |
|---|---:|---|
| A0，种子 `INSATIA1,INSATIA2`，策略种子 `1,2` | 4/4 · 88/88 | 一致 |
| A10，种子 `INSATIA10`，策略种子 `1,2` | 2/2 · 39/39 | 一致 |
| A0，20 张 Defend、延长生存遗物，种子 `INSATIA_TIMER`，策略种子 `1,2` | 2/2 · 47/47 | 一致；验证 Sandpit 到期终结 |

验证命令：

```bash
python3 tools/gen/gen_dispatch.py
cmake --build build -j4 --target sts2sim_cli
DOTNET=/home/wst/.dotnet/dotnet python3 tests/l3/l3_combat.py --seeds INSATIA1,INSATIA2 --characters IRONCLAD --policy-seeds 1,2 --ascension 0 --encounter THE_INSATIABLE_BOSS -v
DOTNET=/home/wst/.dotnet/dotnet python3 tests/l3/l3_combat.py --seeds INSATIA10 --characters IRONCLAD --policy-seeds 1,2 --ascension 10 --encounter THE_INSATIABLE_BOSS -v
DOTNET=/home/wst/.dotnet/dotnet python3 tests/l3/l3_combat.py --seeds INSATIA_TIMER --characters IRONCLAD --policy-seeds 1,2 --ascension 0 --encounter THE_INSATIABLE_BOSS --deck 'DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD' --relics 'ANCHOR,ORICHALCUM,MANGO,CAPTAINS_WHEEL' -v
scripts/check.sh --quick
```

标准 L3 结果：A0 `4/4`、`88/88`；A10 `2/2`、`39/39`；timer 场景 `2/2`、`47/47`；0 unported、0 diverged。`scripts/check.sh --quick` 通过：构建成功、CTest 8/8、整局 smoke 通过（`death=94 unported=6`、`death=100`、`boss=3 death=97`）。
