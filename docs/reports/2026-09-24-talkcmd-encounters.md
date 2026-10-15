# TalkCmd 无头补丁与第一幕遭遇移植报告

日期：2026-09-24
分支：`feat/talkcmd-encounters`
基线：STS2 v0.111.0（release `41cef1ea`）

## 前置条件

- 已阅读 `AGENTS.md`、`docs/plans/2026-09-24-m1-talkcmd-encounters.md`、`docs/plans/2026-09-24-m1-act1-monsters.md`、`docs/design-engine.md` 指定章节和 `docs/audits/pitfalls.md`。
- 反编译源码：`STS2_SRC=/home/wst/Desktop/MCC/Slay the Spire 2/src/Core`。
- 中断前工作区检查：`git status --short --branch` 为干净的 `feat/talkcmd-encounters`，没有可恢复的未提交实现。
- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`、C++ 构建和 oracle Release 构建均成功。

## TalkCmd 审计与补丁

源码 `Commands/TalkCmd.cs:15` 在当前游戏版本只有一个 `Play` 重载：

```text
Play(LocString line, Creature speaker, VfxColor vfxColor, VfxDuration duration = VfxDuration.Custom)
```

方法只检查说话者是否死亡、格式化本地化文本、读取 `FastMode` 偏好、计算气泡时长、创建 `NSpeechBubbleVfx` 并把节点挂到说话者的 VFX 容器；没有写入战斗状态，也没有 RNG 抽取。因此 `tools/Sts2Headless/Headless.cs` 用 Harmony 按精确参数签名挂前缀，直接返回已完成调用的等价结果 `null`。代码注释和 oracle 侧偏离 O-013 已登记在 `docs/audits/deviations.md`。

## 遭遇结果

每个组合使用 `ENC000`–`ENC007` 八个种子和 policy seed `1,2`，即 A0/A10 各 16 场。

| 遭遇 | A0 | A10 | 新增或补齐的类 |
|---|---:|---:|---|
| `CULTISTS_NORMAL` | 16/16（323/323 状态） | 16/16（292/292） | `CalcifiedCultist`, `DampCultist`, `RitualPower`, `CultistsNormal` |
| `SEAPUNK_NORMAL` | 16/16（350/350） | 16/16（307/307） | `SeapunkNormal`；复用已有 `Seapunk`，补入 `CalcifiedCultist` 与 `RitualPower` |
| `GREMLIN_MERC_NORMAL` | 16/16（376/376） | 16/16（374/374） | `GremlinMerc`, `FatGremlin`, `SneakyGremlin`, `SurprisePower`, `ThieveryPower`, `HeistPower`, `GremlinMercNormal` |
| `BYGONE_EFFIGY_ELITE` | 16/16（340/340） | 16/16（315/315） | `BygoneEffigy`, `SlowPower`, `BygoneEffigyElite` |

所有已完成对拍均保持 RNG 抽取、状态机转移、伤害/能力状态和决策动作一致；没有改 RNG、补抽随机数或放宽比较。

## 框架改动

- `tools/Sts2Headless/Headless.cs`：增加 `TalkCmd.Play` 的 Harmony 前缀，理由见上文和 O-013。
- `engine/sim/state.h`：`CombatData` 增加 `gremlin_merc_gold_was_stolen`，保存 `GremlinMercNormal.GoldWasStolen`；每次新战斗的 `CombatData{}` 会清零它。
- `game/generated/AllModels.h`、`game/generated/Dispatch.cpp`：由 `python3 tools/gen/gen_dispatch.py` 重新生成。
- 其余实现均为 `game/Models/{Monsters,Powers,Encounters}/` 下的具体模型。

`HeistPower.BeforeDeath` 的奖励房间和地图点历史写入暂未接入 C++，因为当前战斗状态没有奖励房间对象；该有意偏离登记为 D-004。它不影响本报告的 L3 决策点状态。

## 最后一次完整的 4 遭遇对拍命令与原始输出

命令：

```bash
SEEDS=$(python3 -c "print(','.join('ENC%03d'%i for i in range(8)))")
for spec in 'CULTISTS_NORMAL 0' 'CULTISTS_NORMAL 10' \
  'SEAPUNK_NORMAL 0' 'SEAPUNK_NORMAL 10' \
  'GREMLIN_MERC_NORMAL 0' 'GREMLIN_MERC_NORMAL 10' \
  'BYGONE_EFFIGY_ELITE 0' 'BYGONE_EFFIGY_ELITE 10'; do
  set -- $spec
  echo "$1 A$2"
  python3 tests/l3/l3_combat.py --seeds "$SEEDS" --policy-seeds 1,2 \
    --encounter "$1" --ascension "$2"
done
```

原始输出：

```text
CULTISTS_NORMAL A0
L3 combat: 16/16 combats identical, 323/323 states matched
CULTISTS_NORMAL A10
L3 combat: 16/16 combats identical, 292/292 states matched
SEAPUNK_NORMAL A0
L3 combat: 16/16 combats identical, 350/350 states matched
SEAPUNK_NORMAL A10
L3 combat: 16/16 combats identical, 307/307 states matched
GREMLIN_MERC_NORMAL A0
L3 combat: 16/16 combats identical, 376/376 states matched
GREMLIN_MERC_NORMAL A10
L3 combat: 16/16 combats identical, 374/374 states matched
BYGONE_EFFIGY_ELITE A0
L3 combat: 16/16 combats identical, 340/340 states matched
BYGONE_EFFIGY_ELITE A10
L3 combat: 16/16 combats identical, 315/315 states matched
```

## 合入门槛

命令：

```bash
scripts/check.sh
```

关键原始输出：

```text
100% tests passed, 0 tests failed out of 2
L2 newrun: 200 runs, 0 mismatched
L3 combat: 40/40 combats identical, 784/784 states matched
L3 combat: 40/40 combats identical, 827/827 states matched
all checks passed
```

结论：计划中的 TalkCmd 补丁和 4 个遭遇均完成，验收通过。
