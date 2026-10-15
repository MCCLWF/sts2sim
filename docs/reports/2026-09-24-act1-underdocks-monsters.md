# 第一幕 Underdocks 遭遇移植报告

日期：2026-09-24  
分支：`feat/act1-underdocks-monsters`  
真值：v0.111.0 oracle；铁甲战士起始牌组；每个 A0/A10 组合为 8 个 `ENC000`–`ENC007` 种子 × 2 个 policy seed。

## 结果汇总

| 遭遇 | A0 | A10 | 新增类 |
|---|---:|---:|---|
| `CORPSE_SLUGS_NORMAL` | 16/16（389/389 状态） | 16/16（368/368） | `CorpseSlugsNormal` |
| `HAUNTED_SHIP_NORMAL` | 16/16（447/447） | 16/16（410/410） | `HauntedShipNormal`, `HauntedShip` |
| `FOSSIL_STALKER_NORMAL` | 16/16（268/268） | 16/16（285/285） | `FossilStalkerNormal`, `FossilStalker`, `SuckPower` |
| `PUNCH_CONSTRUCT_NORMAL` | 16/16（455/455） | 16/16（494/494） | `PunchConstructNormal`, `PunchConstruct`, `ArtifactPower` |
| `SEWER_CLAM_NORMAL` | 16/16（489/489） | 16/16（487/487） | `SewerClamNormal`, `SewerClam`, `PlatingPower` |
| `PHANTASMAL_GARDENERS_ELITE` | 16/16（273/273） | 16/16（273/273） | `PhantasmalGardenersElite`, `PhantasmalGardener`, `SkittishPower` |
| `SKULKING_COLONY_ELITE` | 16/16（373/373） | 16/16（339/339） | `SkulkingColonyElite`, `SkulkingColony`, `HardenedShellPower` |
| `TERROR_EEL_ELITE` | 16/16（403/403） | 16/16（346/346） | `TerrorEelElite`, `TerrorEel`, `ShriekPower`, `VigorPower` |

所有通过项均保持 RNG 和调用顺序；没有为对拍修改 RNG 或放宽比较。

## 跳过项

- `CULTISTS_NORMAL`：oracle 在 `CalcifiedCultist.IncantationMove` 的 `TalkCmd.Play` 处因无头环境 `NullReferenceException` 崩溃，随后报 `combat is neither over nor waiting for the player`。
- `SEAPUNK_NORMAL`：同样包含 `CalcifiedCultist`，在同一 `CalcifiedCultist.IncantationMove`/`TalkCmd.Play` 路径崩溃。
- `GREMLIN_MERC_NORMAL`：oracle 在 `GremlinMerc.GimmeMove` 的 `TalkCmd.Play` 处因无头环境 `NullReferenceException` 崩溃。
- `LIVING_FOG_NORMAL`：`LivingFog.BloatMove` 通过 `CreatureCmd.Add<GasBomb>` 召唤并改变敌人列表，`AdvancedGasMove` 还依赖 `SmoggyPower` 的卡牌苦难逻辑；当前框架没有这批机制，按计划跳过。
- `TWO_TAILED_RATS_NORMAL`：`TwoTailedRat.CallForBackup` 通过 `CreatureCmd.Add<TwoTailedRat>` 召唤、重排敌人并更新全部老鼠的 `CallForBackupCount`，会改变回合结构；按计划跳过。

## 框架改动

没有修改 `game/` 框架代码、`tools/Sts2Oracle/`、`tests/l3/` 或 `scripts/`。仅新增具体 Models，并重新生成 `game/generated/Dispatch.cpp` 与 `AllModels.h`。

## 验收命令与原始结果

最后一次完整对拍：

```bash
SEEDS=$(python3 -c "print(','.join('ENC%03d'%i for i in range(8)))")
python3 tests/l3/l3_combat.py --seeds "$SEEDS" --policy-seeds 1,2 \
  --encounter PHANTASMAL_GARDENERS_ELITE --ascension 10
```

原始输出：

```text
L3 combat: 16/16 combats identical, 273/273 states matched
```

构建与单测：

```text
scripts/check.sh --quick
100% tests passed, 0 tests failed out of 2
quick check passed (differential checks skipped)
```
