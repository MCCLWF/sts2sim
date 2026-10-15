# Ceremonial Beast Boss 遭遇移植报告

日期：2026-09-26  
分支：`feat/all-encounters-comparison-mod`  
游戏基线：v0.111.0；真值为反编译源码与 `Sts2Oracle`。

## 范围与实现

本批移植 `CEREMONIAL_BEAST_BOSS`、Ceremonial Beast、`PlowPower`、`RingingPower` 和 Ringing affliction。动画、音效和屏幕效果未进入模拟状态。

- 第一阶段固定执行 Stamp 后反复 Plow。Stamp 施加 150/160 HP 的 Plow 阈值；Plow 攻击后获得 2 点力量。
- 对 Plow 持有者造成非格挡伤害并使 HP 降到阈值以下时，清除永久与临时力量、移除 Plow 并插入一次 Stun move。
- Stun 后进入 Beast Cry、Stomp、Crush 的循环。Beast Cry 对所有玩家施加 Ringing；Stomp 攻击；Crush 攻击后获得 3/4 点力量。
- Ringing 在玩家牌组中给尚无 affliction 的牌加标记；玩家打出一张牌后，当回合不能再打出带 Ringing 的牌。Ringing 在该玩家回合结束时清除，并同时移除所有对应牌标记。

## Oracle 对拍

默认 Ironclad 牌组的逐决策 L3 结果：

| 配置 | 种子 | 战斗 / 状态 | 结果 |
|---|---|---:|---|
| A0 | `CERE01,CERE02`；策略种子 `1,2` | 4/4 · 76/76 | 一致 |
| A10 | `CERE10`；策略种子 `1,2` | 2/2 · 39/39 | 一致 |

默认牌组无法稳定到达二阶段，因此另用 10 张 `BLUDGEON+1` 做分支探针：

| 配置 | 种子 | 战斗 / 状态 | 结果 |
|---|---|---:|---|
| A0 | `CEREG01`；策略种子 `1,2` | 2/2 · 22/22 | 一致 |
| A10 | `CEREG10`；策略种子 `1,2` | 2/2 · 26/26 | 一致 |

两个难度的 oracle trace 均出现完整 `STAMP_MOVE → PLOW_MOVE → STUNNED → BEAST_CRY_MOVE → STOMP_MOVE → CRUSH_MOVE` 路径；状态中可见 Plow 阈值移除、力量清除及 Ringing 标记。

标准 L3 命令：

```bash
DOTNET="$HOME/.dotnet/dotnet" python3 tests/l3/l3_combat.py \
  --seeds CERE01,CERE02 --characters IRONCLAD --policy-seeds 1,2 \
  --ascension 0 --encounter CEREMONIAL_BEAST_BOSS -v

DOTNET="$HOME/.dotnet/dotnet" python3 tests/l3/l3_combat.py \
  --seeds CERE10 --characters IRONCLAD --policy-seeds 1,2 \
  --ascension 10 --encounter CEREMONIAL_BEAST_BOSS -v
```

覆盖率从 82/90 提升到 **83/90 正式遭遇**；怪物模型头文件从 97/126 提升到 **98/126**，另缺 7 个遭遇和 28 个怪物头文件。

## 门禁

模型分发生成及 `cmake --build build -j4 --target sts2sim_cli` 通过。`git diff --check` 通过。`scripts/check.sh --quick` 退出码 0：构建通过、CTest 8/8 通过；全局冒烟通过，结果分别为 `death=94 unported=6`、`death=100`、`boss=3 death=97`。完整 L2/L3/L4 门禁未在本批重跑。
