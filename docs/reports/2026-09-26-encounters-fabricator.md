# Fabricator Normal 遭遇移植报告

日期：2026-09-26  
分支：`feat/all-encounters-comparison-mod`  
游戏基线：v0.111.0；真值为反编译源码与 `Sts2Oracle`。

## 前置与范围

本批将 `FABRICATOR_NORMAL` 与 Guardbot、Noisebot、Stabbot、Zapbot 四种机器人一并移植，覆盖动态召唤、满场分支及机器人的完整战斗行为。Fabricator 每次从防御组和进攻组分别选一种机器人；同组不连续重复上次选中的种类，并使用怪物 AI RNG。机器人进入下一个可用敌方槽位，获得 `MinionPower`。

## 实现

- Fabricator 在存活盟友少于 4 个时选择 Fabricate 或 Fabricating Strike；满场时改用 Disintegrate。strike 伤害后再召唤进攻机器人。
- Guardbot 为 Fabricator 提供格挡；Noisebot 将 Dazed 放入弃牌堆并插入抽牌堆随机位置；Stabbot 攻击并施加 Frail；Zapbot 攻击并在回合结束获得 Strength。
- 加入 Zapbot 的 `HighVoltagePower`、对应遭遇槽位和自动生成的模型分发。
- 仅省略动画等表现层逻辑；所有模拟战斗状态和 RNG 路径都参与对拍。

## Oracle 对拍

使用 Ironclad 默认牌组与 `Sts2Oracle` 做逐决策 L3 对拍：

| 配置 | 种子 | 战斗 / 状态 | 结果 |
|---|---|---:|---|
| A0 | `FABRIC01,FABRIC02`；策略种子 `1,2` | 4/4 · 60/60 | 一致 |
| A10 | `FABRIC10`；策略种子 `1,2` | 2/2 · 29/29 | 一致 |

A0 和 A10 的 trace 覆盖 `DISINTEGRATE_MOVE`、`FABRICATE_MOVE`、`FABRICATING_STRIKE_MOVE`，以及四种机器人生成和行动。总覆盖提升至 **82/90 正式遭遇**、**97/126 怪物模型头文件**；剩余 8 个遭遇和 29 个怪物头文件。

```bash
DOTNET="$HOME/.dotnet/dotnet" python3 tests/l3/l3_combat.py \
  --seeds FABRIC01,FABRIC02 --characters IRONCLAD --policy-seeds 1,2 \
  --ascension 0 --encounter FABRICATOR_NORMAL -v

DOTNET="$HOME/.dotnet/dotnet" python3 tests/l3/l3_combat.py \
  --seeds FABRIC10 --characters IRONCLAD --policy-seeds 1,2 \
  --ascension 10 --encounter FABRICATOR_NORMAL -v
```

## 门禁

`git diff --check` 通过。`scripts/check.sh --quick` 退出码 0：构建通过、CTest 8/8 通过；全局冒烟通过，结果分别为 `death=94 unported=6`、`death=100`、`boss=3 death=97`。完整 L2/L3/L4 门禁未在本批重跑。

## 结果与限制

Fabricator Normal 已达到该项目的 encounter L3 对拍标准。模型未保存动画状态；这些表现效果不参与无头环境的战斗模拟。L3 使用指定种子，尚未做大样本随机路线回归。
