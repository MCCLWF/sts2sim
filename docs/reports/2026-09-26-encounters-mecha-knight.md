# Mecha Knight Elite 遭遇移植报告

日期：2026-09-26  
分支：`feat/all-encounters-comparison-mod`  
游戏基线：v0.111.0；真值为反编译源码与 `Sts2Oracle`。

## 结果

已接入 `MECHA_KNIGHT_ELITE` 与单体 Mecha Knight。招式按原版循环为 Charge → Flamethrower → Windup → Heavy Cleave → Flamethrower；保留 A0/A10 伤害、初始 3 层 Artifact、Windup 的 15 格挡与 5 力量、Flamethrower 加入 4 张 Burn。

Mecha Knight 的 `IsWoundUp` 只控制受击/待机动画分支，没有战斗效果，故不保存为模拟状态。视觉动画 Hook 仍未移植。

对拍在 Burn 留手阶段发现通用 `Burn` 卡的回合末 Hook 缺失。本批同时移植了其源代码行为：卡留在手牌时对持有者造成牌面 2 点伤害。Burn 是该遭遇直接生成的通用状态牌；未扩展其他角色卡池。

正式遭遇覆盖提升至 **73/90**，怪物模型头文件提升至 **81/126**，尚缺 17 个遭遇和 45 个怪物模型头文件。

## Oracle 对拍

| 配置 | 战斗 / 状态 | 结果 |
|---|---:|---|
| A0，种子 `MECHAA01,MECHAA02`，策略种子 `1,2` | 4/4 · 81/81 | 一致 |
| A10，种子 `MECHAA10`，策略种子 `1,2` | 2/2 · 35/35 | 一致 |

## 验证命令

```bash
python3 tools/gen/gen_dispatch.py
cmake --build build -j4 --target test_env sts2env_c sts2sim_env_solver sts2sim_cli
DOTNET=/home/wst/.dotnet/dotnet python3 tests/l3/l3_combat.py --seeds MECHAA01,MECHAA02 --characters IRONCLAD --policy-seeds 1,2 --ascension 0 --encounter MECHA_KNIGHT_ELITE -v
DOTNET=/home/wst/.dotnet/dotnet python3 tests/l3/l3_combat.py --seeds MECHAA10 --characters IRONCLAD --policy-seeds 1,2 --ascension 10 --encounter MECHA_KNIGHT_ELITE -v
scripts/check.sh --quick
```

L3 结果：A0 `4/4`、`81/81`；A10 `2/2`、`35/35`；0 unported、0 diverged。`scripts/check.sh --quick` 通过：8/8 CTest 和三组整局冒烟通过（`death=94 unported=6`、`death=100`、`boss=3 death=97`）。完整 L2/L3/L4 门禁未运行。
