# The Lost and The Forgotten 遭遇移植报告

日期：2026-09-26
分支：`feat/all-encounters-comparison-mod`
游戏基线：v0.111.0，真值为反编译源码与 `Sts2Oracle`。

## 结果

已移植 `THE_LOST_AND_FORGOTTEN_NORMAL`，固定生成 The Lost 与 The Forgotten。两者依次施放夺取属性招式与攻击招式：The Lost 降低玩家力量、提高自身力量；The Forgotten 降低玩家敏捷、获得 8 格挡并提高自身敏捷。新增的 Possess Strength / Possess Speed 记录由该敌人施加的玩家属性损失，并在该敌人死亡时返还。

验证使用 10 张升级 `BLUDGEON`、`AKABEKO` 和 `TUNGSTEN_ROD`，以确保 A0 的两个策略分别击杀 The Lost 与 The Forgotten，从而对拍属性返还分支。

| 难度 | 战斗 | 决策状态 | 未移植 | 分歧 |
|---|---:|---:|---:|---:|
| A0 | 4/4 | 48/48 | 0 | 0 |
| A10 | 2/2 | 24/24 | 0 | 0 |
| **合计** | **6/6** | **72/72** | **0** | **0** |

A0 中，种子 `NEW001,NEW002`、策略种子 1 均击杀 The Lost；策略种子 2 均击杀 The Forgotten。完整状态对拍覆盖两种死亡后的属性返还。A10 的两个策略样本中，The Lost 死亡并返还力量；The Forgotten 死亡分支已由 A0 覆盖。

## 验证命令

模型分发：

```text
python3 tools/gen/gen_dispatch.py
ported models: 786; dispatch cases: MonsterModel 124, EncounterModel 70
```

构建：

```text
cmake --build build -j4 --target test_env sts2env_c sts2sim_env_solver sts2sim_cli
```

A0：

```text
DOTNET="$HOME/.dotnet/dotnet" python3 tests/l3/l3_combat.py \
  --seeds NEW001,NEW002 --characters IRONCLAD --policy-seeds 1,2 \
  --ascension 0 --encounter THE_LOST_AND_FORGOTTEN_NORMAL \
  --deck BLUDGEON+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1 \
  --relics AKABEKO,TUNGSTEN_ROD -v
L3 combat: 4/4 combats identical, 0 unported, 0 diverged, 48/48 states matched
```

A10：

```text
DOTNET="$HOME/.dotnet/dotnet" python3 tests/l3/l3_combat.py \
  --seeds A10001 --characters IRONCLAD --policy-seeds 1,2 \
  --ascension 10 --encounter THE_LOST_AND_FORGOTTEN_NORMAL \
  --deck BLUDGEON+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1 \
  --relics AKABEKO,TUNGSTEN_ROD -v
L3 combat: 2/2 combats identical, 0 unported, 0 diverged, 24/24 states matched
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

游戏当前限定单人战斗，因此状态只记录一个玩家遭受的属性损失；多人目标字典尚未移植。L3 是指定种子、卡组与遗物的抽样验证，完整 `scripts/check.sh` L2/L3/L4 门禁尚未运行。
