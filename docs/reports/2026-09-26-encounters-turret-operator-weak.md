# TURRET_OPERATOR_WEAK 遭遇移植报告

日期：2026-09-26
分支：`feat/all-encounters-comparison-mod`
游戏基线：v0.111.0；真值为反编译源码与 `Sts2Oracle`。

## 结果

`TURRET_OPERATOR_WEAK` 已由 Env 生成并完整模拟。Living Shield、Turret Operator 和 Rampart 已注册。Oracle L3 对拍结果：A0 为 4/4 战斗、69/69 决策状态一致；A10 为 2/2 战斗、35/35 状态一致。`scripts/check.sh --quick` 通过。

## 移植语义

- Encounter 按原版顺序生成无命名槽位的 Living Shield、Turret Operator。
- Living Shield 初始 25 点 Rampart；首招 Shield Slam（6 点），若仍有存活队友则继续 Shield Slam；失去队友后转为 Smash（A0 16、A10 18 点）并每次增加 3 力量，此后循环 Smash。
- Turret Operator 初始两次连续五段攻击（每段 A0 3、A10 4 点），然后 Reload 增加 1 力量并循环。
- Rampart 在非额外玩家回合的玩家侧回合开始时，给场上的每只 Turret Operator 增加 25 点无来源格挡。检查挂在 Living Shield Power 的 `AfterSideTurnStart`，目标按原版从当前敌人列表中筛选 Turret Operator。

## 验证

生成分发：

```text
python3 tools/gen/gen_dispatch.py
ported models: 772; dispatch cases: EncounterModel 65, MonsterModel 113, PowerModel 34
```

构建通过：

```text
cmake --build build -j4 --target test_env sts2env_c sts2sim_env_solver
cmake --build build -j4 --target sts2sim_cli
```

L3 A0：

```text
DOTNET="$HOME/.dotnet/dotnet" python3 tests/l3/l3_combat.py \
  --seeds NEW001,NEW002 --characters IRONCLAD --policy-seeds 1,2 \
  --ascension 0 --encounter TURRET_OPERATOR_WEAK -v
ok NEW001 policy 1 (18 states)
ok NEW001 policy 2 (17 states)
ok NEW002 policy 1 (17 states)
ok NEW002 policy 2 (17 states)
L3 combat: 4/4 combats identical, 0 unported, 0 diverged, 69/69 states matched
```

L3 A10：

```text
DOTNET="$HOME/.dotnet/dotnet" python3 tests/l3/l3_combat.py \
  --seeds A10001 --characters IRONCLAD --policy-seeds 1,2 \
  --ascension 10 --encounter TURRET_OPERATOR_WEAK -v
ok A10001 policy 1 (18 states)
ok A10001 policy 2 (17 states)
L3 combat: 2/2 combats identical, 0 unported, 0 diverged, 35/35 states matched
```

Quick gate：

```text
scripts/check.sh --quick
100% tests passed, 0 tests failed out of 8
smoke: death=94 unported=6
smoke: death=100
smoke: boss=3 death=97
quick check passed (differential checks skipped)
```

第一次 L3 调用发生在静态链接版 `sts2sim_cli` 重建之前，旧二进制报告 `GenerateMonsters is not ported`。随后重建 CLI，并用上述命令重跑通过；旧二进制结果不计入对拍结论。

## 范围限制

本批只证明上述遭遇、A0/A10、指定种子与策略的完整状态一致；没有替代其他遭遇的 L3 验证，也没有运行完整 `scripts/check.sh` 的 L2/L3/L4 全门禁。总覆盖矩阵见[全遭遇阶段报告](2026-09-26-all-encounters-comparison-mod.md)。
