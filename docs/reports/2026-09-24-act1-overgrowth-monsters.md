# Overgrowth 第一幕遭遇移植报告

日期：2026-09-24  
分支：`feat/act1-overgrowth-monsters`  
基线：STS2 v0.111.0（release `41cef1ea`）

## 前置条件

- 已阅读 `AGENTS.md`、`docs/plans/2026-09-24-m1-act1-monsters.md`、`docs/design-engine.md` 指定章节和 `docs/audits/pitfalls.md`。
- 反编译源码：`$STS2_SRC=/home/wst/Desktop/MCC/Slay the Spire 2/src/Core`。
- `tools/Sts2Oracle` Release 构建成功。
- `cmake --build build -j2` 成功。

## 遭遇结果

每个已完成遭遇均使用 8 个种子（`ENC000`–`ENC007`）和两个策略种子（`1,2`），即 A0/A10 各 16 场。

| 遭遇 | A0 | A10 | 新增类 |
|---|---:|---:|---|
| `CUBEX_CONSTRUCT_NORMAL` | 16/16（352 状态） | 16/16（366 状态） | `CubexConstruct`, `ArtifactPower`, `CubexConstructNormal` |
| `FLYCONID_NORMAL` | 16/16（384） | 16/16（359） | `Flyconid`, `FlyconidNormal` |
| `FOGMOG_NORMAL` | 跳过 | 跳过 | — |
| `INKLETS_NORMAL` | 16/16（304） | 16/16（326） | `Inklet`, `SlipperyPower`, `InkletsNormal` |
| `MAWLER_NORMAL` | 16/16（406） | 16/16（401） | `Mawler`, `MawlerNormal` |
| `NIBBITS_NORMAL` | 16/16（363） | 16/16（325） | `NibbitsNormal` |
| `OVERGROWTH_CRAWLERS` | 16/16（348） | 16/16（324） | `OvergrowthCrawlers` |
| `RUBY_RAIDERS_NORMAL` | 16/16（310） | 16/16（261） | `AxeRubyRaider`, `AssassinRubyRaider`, `BruteRubyRaider`, `CrossbowRubyRaider`, `TrackerRubyRaider`, `RubyRaidersNormal` |
| `SLIMES_NORMAL` | 16/16（436） | 16/16（363） | `SlimesNormal` |
| `SLITHERING_STRANGLER_NORMAL` | 16/16（388） | 16/16（368） | `ConstrictPower`, `SlitheringStrangler`, `SlitheringStranglerNormal` |
| `SNAPPING_JAXFRUIT_NORMAL` | 16/16（343） | 16/16（322） | `SnappingJaxfruit`, `SnappingJaxfruitNormal` |
| `VINE_SHAMBLER_NORMAL` | 跳过 | 跳过 | — |
| `BYGONE_EFFIGY_ELITE` | 跳过 | 跳过 | — |
| `BYRDONIS_ELITE` | 16/16（320） | 16/16（306） | `TerritorialPower`, `Byrdonis`, `ByrdonisElite` |
| `PHROG_PARASITE_ELITE` | 跳过 | 跳过 | — |

## 跳过项

- `BYGONE_EFFIGY_ELITE`：oracle 在执行 `BygoneEffigy.WakeMove` 时调用 `TalkCmd.Play`，headless 进程报 `NullReferenceException`，随后报 `combat is neither over nor waiting for the player`，无法产生可对拍的完整轨迹。
- `FOGMOG_NORMAL`：`Fogmog.IllusionMove` 调用 `CreatureCmd.Add<EyeWithTeeth>`；需要未移植的 `EyeWithTeeth`、`IllusionPower`，其 `DistractMove` 还会加入 `Dazed` 状态牌。
- `VINE_SHAMBLER_NORMAL`：`VineShambler.GraspingVinesMove` 应用 `TangledPower`；`TangledPower.AfterApplied` 和 `AfterCardEnteredCombat` 调用 `CardCmd.Afflict<Entangled>`，当前框架没有该 CardCmd/affliction 移植。
- `PHROG_PARASITE_ELITE`：`PhrogParasite.InfectMove` 需要 `Infection` 状态牌；`InfestedPower.AfterDeath` 要在死亡结算中召唤 4 个 `Wriggler` 并改变敌方回合结构。计划明确要求遇到这类大块召唤机制时跳过。

## 框架改动

没有改动 `game/` 框架、`tools/Sts2Oracle/`、`tests/l3/` 或 `scripts/`。所有实现均为 `game/Models/` 下的具体怪物、能力、状态牌相关能力和遭遇类；`game/generated/` 只由 `tools/gen/gen_dispatch.py` 重新生成。

## 最后一次完整对拍

命令：

```bash
SEEDS=$(python3 -c "print(','.join('ENC%03d'%i for i in range(8)))")
python3 tests/l3/l3_combat.py --seeds "$SEEDS" --policy-seeds 1,2 \
  --encounter SLITHERING_STRANGLER_NORMAL --ascension 10
```

原始输出：

```text
L3 combat: 16/16 combats identical, 368/368 states matched
```

最终构建检查：`cmake --build build -j2` 成功；oracle Release 构建成功。
