# Entomancer Elite 遭遇移植报告

日期：2026-09-26
分支：`feat/all-encounters-comparison-mod`
游戏基线：v0.111.0，真值为反编译源码与 `Sts2Oracle`。

## 结果

已添加 `ENTOMANCER_ELITE`、Entomancer 和 Personal Hive。招式循环为 Bees（3 点 × A0 7 段 / A10 8 段）、Spear（A0 18 / A10 20）和 Pheromone Spit。初始 Hive 为 1；Spit 在 Hive 小于 3 时 Hive 与力量各加 1，满 3 后只加 2 力量。Personal Hive 在 Entomancer 受到玩家的已强化攻击伤害后，按 Hive 层数将 Dazed 加入攻击者的抽牌堆随机位置。

L3 Oracle 对拍通过：A0 4/4 战斗、85/85 决策状态；A10 2/2、44/44；无未移植项、无分歧。

## 验证

生成分发：

```text
python3 tools/gen/gen_dispatch.py
ported models: 789; dispatch cases: MonsterModel 126, EncounterModel 71
```

构建通过：

```text
cmake --build build -j4 --target test_env sts2env_c sts2sim_env_solver sts2sim_cli
```

A0：

```text
DOTNET="$HOME/.dotnet/dotnet" python3 tests/l3/l3_combat.py \
  --seeds NEW001,NEW002 --characters IRONCLAD --policy-seeds 1,2 \
  --ascension 0 --encounter ENTOMANCER_ELITE -v
L3 combat: 4/4 combats identical, 0 unported, 0 diverged, 85/85 states matched
```

A10：

```text
DOTNET="$HOME/.dotnet/dotnet" python3 tests/l3/l3_combat.py \
  --seeds A10001 --characters IRONCLAD --policy-seeds 1,2 \
  --ascension 10 --encounter ENTOMANCER_ELITE -v
L3 combat: 2/2 combats identical, 0 unported, 0 diverged, 44/44 states matched
```

Quick gate:

```text
scripts/check.sh --quick
100% tests passed, 0 tests failed out of 8
smoke: death=94 unported=6
smoke: death=100
smoke: boss=3 death=97
quick check passed (differential checks skipped)
```

## 范围

测试为 Ironclad、指定种子与随机策略。对拍验证了 Hive 状态、Dazed 数量与抽牌堆状态；尚未运行更大规模路线回归。完整 `scripts/check.sh` L2/L3/L4 门禁仍待运行。
