# Soul Fysh Boss 遭遇移植报告

日期：2026-09-26  
分支：`feat/all-encounters-comparison-mod`  
游戏基线：v0.111.0；真值为反编译源码与 `Sts2Oracle`。

## 结果

已接入 `SOUL_FYSH_BOSS` 与 Soul Fysh 的五招循环：Beckon → De-Gas → Gaze → Fade → Scream。保留 A0/A10 攻击数值、Fade 的 2 层 Intangible、Scream 的 3 层易伤，以及 Beckon/Gaze 生成状态牌的具体牌堆位置。

新增 Beckon 状态牌：成本 1、不可升级，回合结束仍留在手牌时造成 6 点无视格挡/力量的 HP 损失。新增 IntangiblePower：本回合受到的 HP 损失和攻击伤害上限均为 1，并在敌方侧回合结束时递减。

音乐参数、Soul Fysh 隐形状态和动画 Hook 只影响表现，在无头模拟中不保存。

正式遭遇覆盖提升至 **77/90**，怪物模型头文件提升至 **86/126**，尚缺 13 个遭遇和 40 个怪物模型头文件。

## Oracle 对拍

| 配置 | 战斗 / 状态 | 结果 |
|---|---:|---|
| A0，种子 `SOULFY01,SOULFY02`，策略种子 `1,2` | 4/4 · 96/96 | 一致 |
| A10，种子 `SOULFY10`，策略种子 `1,2` | 2/2 · 43/43 | 一致 |

## 验证命令

```bash
python3 tools/gen/gen_dispatch.py
cmake --build build -j4 --target sts2sim_cli
DOTNET=/home/wst/.dotnet/dotnet python3 tests/l3/l3_combat.py --seeds SOULFY01,SOULFY02 --characters IRONCLAD --policy-seeds 1,2 --ascension 0 --encounter SOUL_FYSH_BOSS -v
DOTNET=/home/wst/.dotnet/dotnet python3 tests/l3/l3_combat.py --seeds SOULFY10 --characters IRONCLAD --policy-seeds 1,2 --ascension 10 --encounter SOUL_FYSH_BOSS -v
scripts/check.sh --quick
```

L3 结果：A0 `4/4`、`96/96`；A10 `2/2`、`43/43`；0 unported、0 diverged。`scripts/check.sh --quick` 通过：构建成功、CTest 8/8、整局 smoke 通过（`death=94 unported=6`、`death=100`、`boss=3 death=97`）。
