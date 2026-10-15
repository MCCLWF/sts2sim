# Infested Prisms Elite 遭遇移植报告

日期：2026-09-26
分支：`feat/all-encounters-comparison-mod`
游戏基线：v0.111.0，真值为反编译源码与 `Sts2Oracle`。

## 结果

已移植 `INFESTED_PRISMS_ELITE`、Infested Prism、Vital Spark、Tainted affliction 与 Tainted Power。怪物按 Jab → Radiate → Whirlwind → Pulsate 循环；Pulsate 增加格挡并提高 Vital Spark 层数。Vital Spark 在战斗开始时给玩家所有技能牌加 Tainted，之后进入战斗的新技能牌也会被污染。打出污染技能会令玩家获得同层数的 Tainted Power，受到的已强化攻击伤害增加相同点数，该能力在敌方侧回合结束时移除。

Oracle L3 对拍通过：A0 4/4 战斗、88/88 状态；A10 2/2、39/39；无未移植项、无分歧。

## 验证

分发生成：

```text
python3 tools/gen/gen_dispatch.py
ported models: 794; dispatch cases: PowerModel 35, MonsterModel 128, AfflictionModel 5, EncounterModel 72
```

构建通过：

```text
cmake --build build -j4 --target test_env sts2env_c sts2sim_env_solver sts2sim_cli
```

A0：

```text
DOTNET="$HOME/.dotnet/dotnet" python3 tests/l3/l3_combat.py \
  --seeds NEW001,NEW002 --characters IRONCLAD --policy-seeds 1,2 \
  --ascension 0 --encounter INFESTED_PRISMS_ELITE -v
L3 combat: 4/4 combats identical, 0 unported, 0 diverged, 88/88 states matched
```

A10：

```text
DOTNET="$HOME/.dotnet/dotnet" python3 tests/l3/l3_combat.py \
  --seeds A10001 --characters IRONCLAD --policy-seeds 1,2 \
  --ascension 10 --encounter INFESTED_PRISMS_ELITE -v
L3 combat: 2/2 combats identical, 0 unported, 0 diverged, 39/39 states matched
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

测试覆盖 Ironclad 默认牌组中已有技能、其打出后获得的 Tainted Power、敌方回合伤害及 Vital Spark 堆叠。未运行更大规模路线回归；完整 L2/L3/L4 门禁仍待运行。
