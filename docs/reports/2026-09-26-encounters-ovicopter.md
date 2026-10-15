# Ovicopter Normal 遭遇移植报告

日期：2026-09-26  
分支：`feat/all-encounters-comparison-mod`  
游戏基线：v0.111.0；真值为反编译源码与 `Sts2Oracle`。

## 结果

已接入 `OVICOPTER_NORMAL`、Ovicopter、Tough Egg 与 HatchPower。Ovicopter 依次 Lay Eggs → Smash → Tenderizer → 条件分支；存活敌人不多于 3 时再次产卵，否则使用 Nutritional Paste。每次产卵最多三枚，并按原版从遭遇槽位列表末尾起选择空闲 `egg*` 槽位。

Tough Egg 进入战斗时根据当前回合侧获得 1 或 2 层 HatchPower；该能力在拥有者参与的侧回合结束时递减。蛋首次行动时移除 HatchPower 和非 Minion 能力，按 ToughEnemies 范围从 Niche RNG 抽取孵化后生命值并切换到循环 Nibble 招式。初始蛋 HP 仍由通用怪物生成路径处理。

正式遭遇覆盖提升至 **76/90**，怪物模型头文件提升至 **85/126**，尚缺 14 个遭遇和 41 个怪物模型头文件。

## Oracle 对拍

| 配置 | 战斗 / 状态 | 结果 |
|---|---:|---|
| A0，种子 `OVICOPA01,OVICOPA02`，策略种子 `1,2` | 4/4 · 72/72 | 一致 |
| A10，种子 `OVICOPA10`，策略种子 `1,2` | 2/2 · 34/34 | 一致 |
| A0，20 张 Defend、延长生存遗物，种子 `OVIEGGS`，策略种子 `1,2` | 2/2 · 49/49 | 一致；验证蛋孵化 |

## 验证命令

```bash
python3 tools/gen/gen_dispatch.py
cmake --build build -j4 --target sts2sim_cli
DOTNET=/home/wst/.dotnet/dotnet python3 tests/l3/l3_combat.py --seeds OVICOPA01,OVICOPA02 --characters IRONCLAD --policy-seeds 1,2 --ascension 0 --encounter OVICOPTER_NORMAL -v
DOTNET=/home/wst/.dotnet/dotnet python3 tests/l3/l3_combat.py --seeds OVICOPA10 --characters IRONCLAD --policy-seeds 1,2 --ascension 10 --encounter OVICOPTER_NORMAL -v
DOTNET=/home/wst/.dotnet/dotnet python3 tests/l3/l3_combat.py --seeds OVIEGGS --characters IRONCLAD --policy-seeds 1,2 --ascension 0 --encounter OVICOPTER_NORMAL --deck 'DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD' --relics 'ANCHOR,ORICHALCUM,MANGO,CAPTAINS_WHEEL' -v
scripts/check.sh --quick
```

L3 结果：A0 `4/4`、`72/72`；A10 `2/2`、`34/34`；孵化场景 `2/2`、`49/49`；0 unported、0 diverged。`scripts/check.sh --quick` 通过：8/8 CTest 和三组整局冒烟通过（`death=94 unported=6`、`death=100`、`boss=3 death=97`）。完整 L2/L3/L4 门禁未运行。
