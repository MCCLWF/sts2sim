# Aeonglass Boss 遭遇移植报告

日期：2026-09-26  
分支：`feat/all-encounters-comparison-mod`  
游戏基线：v0.111.0；真值为反编译源码与 `Sts2Oracle`。

## 范围与实现

本批移植 `AEONGLASS_BOSS`、Aeonglass、`WitheringPresencePower` 和 Wither 状态牌。动画、音乐参数、相机与卡牌预览不属于无头战斗状态。

- 开战时每名玩家获得独立的 Withering Presence 计数，每打出 6 张牌就在手牌生成一张 Wither。
- Aeonglass 循环执行 Ebb、Eye Lasers、Increasing Intensity。Intensity 提升全体现有 Wither 的伤害 3 点，向弃牌堆生成 1/2 张 Wither，并令 Aeonglass 获得逐次递增的力量；新生成的 Wither 匹配当前假升级次数。
- Wither 无法打出，在玩家回合结束时造成 3 点伤害；每次 Intensity 后其伤害继续累积。
- Aeonglass 开战还获得 3 层 Artifact；Ebb 的 33 格挡、双段 Eye Lasers 与 A0/A10 伤害值均对齐原版。

## Oracle 对拍

默认 Ironclad 牌组的逐决策 L3 结果：

| 配置 | 种子 | 战斗 / 状态 | 结果 |
|---|---|---:|---|
| A0 | `AEONG01,AEONG02`；策略种子 `1,2` | 4/4 · 69/69 | 一致 |
| A10 | `AEONG10`；策略种子 `1,2` | 2/2 · 35/35 | 一致 |

默认牌组不稳定地产生足够牌数来走多轮循环；另用 20 张 `DEFEND_IRONCLAD` 并配 `TUNGSTEN_ROD,LIZARD_TAIL` 做长局探针：

| 配置 | 种子 | 战斗 / 状态 | 结果 |
|---|---|---:|---|
| A0 | `AEONGP01`；策略种子 `1,2` | 2/2 · 54/54 | 一致 |
| A10 | `AEONGP10`；策略种子 `1,2` | 2/2 · 49/49 | 一致 |

oracle trace 覆盖三种招式循环、牌数到 6 后的 Wither 入手、回合末伤害，以及多轮 Intensity 对现有/新生成 Wither 的逐级假升级。此次新增 6 个标准战斗、104 个逐状态检查；4 个长局探针独立记录，不并入总计。

标准 L3 命令：

```bash
DOTNET="$HOME/.dotnet/dotnet" python3 tests/l3/l3_combat.py --seeds AEONG01,AEONG02 --characters IRONCLAD --policy-seeds 1,2 --ascension 0 --encounter AEONGLASS_BOSS -v
DOTNET="$HOME/.dotnet/dotnet" python3 tests/l3/l3_combat.py --seeds AEONG10 --characters IRONCLAD --policy-seeds 1,2 --ascension 10 --encounter AEONGLASS_BOSS -v
```

覆盖率从 83/90 提升到 **84/90 正式遭遇**；怪物模型头文件从 98/126 提升到 **99/126**。还缺 6 个遭遇和 27 个怪物头文件。

## 门禁

模型分发生成及 `cmake --build build -j4 --target sts2sim_cli` 通过。`git diff --check` 通过。`scripts/check.sh --quick` 退出码 0：构建通过、CTest 8/8 通过；全局冒烟通过，结果分别为 `death=94 unported=6`、`death=100`、`boss=3 death=97`。完整 L2/L3/L4 门禁未在本批重跑。
