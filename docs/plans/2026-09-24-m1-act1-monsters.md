# 计划：第一幕常规与精英遭遇的怪物移植（M1-j 前置）

执行者：Codex（每个分支一幕，见下）。审阅与合入：Claude。

## 目标

把第一幕的常规遭遇和精英遭遇移植到 C++，包括其中的怪物、怪物用到的能力和状态牌。每个遭遇用 L3 对拍验收：**铁甲战士起始牌组打该遭遇，A0 和 A10 各 16 场，每个决策点的状态与游戏 0 分歧**。

| 分支 | 幕 | 遭遇 |
|---|---|---|
| `feat/act1-overgrowth-monsters` | Overgrowth | CUBEX_CONSTRUCT_NORMAL FLYCONID_NORMAL FOGMOG_NORMAL INKLETS_NORMAL MAWLER_NORMAL NIBBITS_NORMAL OVERGROWTH_CRAWLERS RUBY_RAIDERS_NORMAL SLIMES_NORMAL SLITHERING_STRANGLER_NORMAL SNAPPING_JAXFRUIT_NORMAL VINE_SHAMBLER_NORMAL BYGONE_EFFIGY_ELITE BYRDONIS_ELITE PHROG_PARASITE_ELITE |
| `feat/act1-underdocks-monsters` | Underdocks | CORPSE_SLUGS_NORMAL CULTISTS_NORMAL FOSSIL_STALKER_NORMAL GREMLIN_MERC_NORMAL HAUNTED_SHIP_NORMAL LIVING_FOG_NORMAL PUNCH_CONSTRUCT_NORMAL SEAPUNK_NORMAL SEWER_CLAM_NORMAL TWO_TAILED_RATS_NORMAL PHANTASMAL_GARDENERS_ELITE SKULKING_COLONY_ELITE TERROR_EEL_ELITE |

## 先读

1. `AGENTS.md`（全部）。
2. `docs/design-engine.md`：第 2、4、5、8、9、11、13 节。
3. 已移植的样板，照它们的写法：
   - 怪物：`game/Models/Monsters/Toadpole.h`（条件分支、字段）、`CorpseSlug.h`（`AfterAddedToRoom`、按字段选初始状态）、`LeafSlimeS.h`（随机分支、加状态牌）、`Nibbit.h`；
   - 遭遇：`game/Models/Encounters/ToadpolesWeak.h`、`SlimesWeak.h`（遭遇 Rng）、`CorpseSlugsWeak.h`；
   - 能力：`game/Models/Powers/*.h`（`VulnerablePower.h` 演示了“引用未移植的类时抛 Unported”）；
   - 卡牌：`game/Models/Cards/Slimed.h`。
4. `docs/audits/pitfalls.md`。

## 每个遭遇的步骤

1. 读 C# 源码：`$STS2_SRC/Models/Encounters/<遭遇>.cs`、它的怪物 `Models/Monsters/*.cs`、怪物用到的能力 `Models/Powers/*.cs` 和状态牌 `Models/Cards/*.cs`。
2. 逐行移植到 `game/Models/{Encounters,Monsters,Powers,Cards}/<类名>.h`：
   - 类名、成员名、调用顺序与 C# 完全一致。
   - 动画、音效、特效、`TestMode.IsOff` 分支里的表现代码删掉，其余一行不少。
   - `AscensionHelper.GetValueIfAscension(...)` 写成 `AscensionHelper::GetValueIfAscension(sim(), ...)`。
   - 字段放在 `extra(k)` 槽位，用具名访问器。
3. 运行 `python3 tools/gen/gen_dispatch.py`：它会检查每个 C# 覆写的成员你都声明了。
4. 构建：`cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j`。
5. 构建 oracle（一次即可）：`~/.dotnet/dotnet build tools/Sts2Oracle -c Release -v quiet -nologo`。
6. 对拍：
   ```bash
   SEEDS=$(python3 -c "print(','.join('ENC%03d'%i for i in range(8)))")
   python3 tests/l3/l3_combat.py --seeds $SEEDS --policy-seeds 1,2 --encounter <遭遇> --ascension 0
   python3 tests/l3/l3_combat.py --seeds $SEEDS --policy-seeds 1,2 --encounter <遭遇> --ascension 10
   ```
   两条都必须输出 `16/16 combats identical`。
7. 出现分歧时，按 `DIFF` 行给出的 JSON 路径找模拟器的首因，回到 C# 源码逐行核对。**不许为了对上结果改 RNG、补抽随机数或放宽比对**。
8. 每完成一个遭遇就提交一次：`feat: port <遭遇>`，正文列出新增的类。

## 规则与边界

- **可以改的**：`game/Models/` 下新增具体类；重新生成 `game/generated/`。
- **尽量不改的**：`game/` 下的框架代码（Cmd、Hook、CombatManager、实体、基类）。如果某个遭遇确实需要框架里还没有的东西：
  - 缺一个简单的、C# 里有的重载或方法（例如 `CardPileCmd` 的某个重载、`Creature` 的某个属性），可以逐行照 C# 源码补上，并在报告里单列出来。
  - 需要大块新机制（玩家选择 `CardSelectCmd`、充能球、召唤后改变回合结构等），或者你不确定：**跳过这个遭遇**，在报告里写清楚卡在哪个 C# 方法上，继续下一个。
- 不改 `tools/Sts2Oracle/`、`tests/l3/`、`scripts/`。
- 不提交构建产物、游戏文件、反编译源码。
- oracle 在需要玩家选择时会报 `combat is neither over nor waiting for the player`：这类遭遇同样跳过并写进报告。

## 交付

- 分支上的提交（每个遭遇一个）。
- 报告 `docs/reports/2026-09-24-<分支名去掉 feat/>.md`，内容：
  - 每个遭遇的结果表：遭遇、A0、A10、新增的类；
  - 跳过的遭遇及原因（具体到 C# 方法）；
  - 对框架的每一处改动（文件、方法、对应的 C# 位置）；
  - 最后一次完整的对拍命令和原始输出。
- 结论如实：没通过就写没通过。
