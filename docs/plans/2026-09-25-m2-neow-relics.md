# 计划：Neow 的 30 个遗物（M2）

执行者：Codex（sol）。分支：`feat/neow-relics`（基于 `feat/run-skeleton`）。审阅与合入：Claude。

## 背景

- 开局的 Neow 事件已经逐位对齐（`game/Models/Events/Neow.h`）。选中选项后获得一个先古遗物，然后遗物的 `AfterObtained` 等成员报 `Unported`。整局冒烟（`sts2sim_cli smoke`）200 局里 190 多局停在这里。
- 30 个遗物：`ARCANE_SCROLL BOOMING_CONCH FISHING_ROD GOLDEN_PEARL KALEIDOSCOPE LEAD_PAPERWEIGHT LOST_COFFER MASSIVE_SCROLL NEOWS_TORMENT NEW_LEAF PHIAL_HOLSTER PRECISE_SCISSORS SCROLL_BOXES WINGED_BOOTS LAVA_ROCK SMALL_CAPSULE NUTRITIOUS_OYSTER STONE_HUMIDIFIER NEOWS_TALISMAN POMANDER CURSED_PEARL DOWSING_ROD HEFTY_TABLET LARGE_CAPSULE LEAFY_POULTICE NEOWS_BONES NEOWS_SACRIFICE PRECARIOUS_SHEARS SILKEN_TRESS SILVER_CRUCIBLE`（C# 在 `$STS2_SRC/Models/Relics/<类名>.cs`）。
- 其中一部分已经是部分移植（只有 `IsAllowed`/`IsAllowedAtNeow`，头部有 `// Members left to Unported` 标记），例如 `Kaleidoscope.h`、`ScrollBoxes.h`、`MassiveScroll.h`、`SilverCrucible.h`、`WingedBoots.h`：把它们补成完整移植，删掉标记行和 `SIM_NOT_PORTED`。
- 已有的命令（直接用，不要重写）：`CardSelectCmd`（含 `FromDeckForRemoval`、`FromDeckForTransformation`、`FromDeckForUpgrade`、`FromChooseACardScreen`、`FromChooseABundleScreen`）、`CardCmd.Transform/TransformToRandom/Upgrade`、`CardPileCmd.Add/AddCurseToDeck/RemoveFromDeck`、`RewardsCmd.OfferCustom`（奖励界面，嵌套选择）、`RelicCmd.Obtain`、`PotionCmd.TryToProcure`、`PlayerCmd.GainGold/LoseGold/GainMaxPotionCount/GainEnergy`、`CreatureCmd.GainMaxHp/LoseMaxHp/Damage/Heal`、`CardFactory`、`RelicFactory`、`PotionFactory`、`Reward::New*Reward`。

## 任务

1. 逐个完整移植 30 个遗物（`game/Models/Relics/<类名>.h`），所有 C# 覆写的成员都移植，逐行照 C#：调用顺序、随机数流与抽取次数一致；动画、音效、`Flash()` 这类表现代码删掉。
2. 遗物要用到的卡牌（诅咒、Neow 给的特殊卡等）也要移植到 `game/Models/Cards/`；卡牌在战斗里的 `OnPlay` 也要移植（L4 会在战斗里打出它们）。
3. 需要框架里没有的东西：
   - 缺一个简单的、C# 里有的重载或方法，逐行照 C# 补上，在报告里单列（文件、方法、C# 位置）。
   - 大机制（附魔 `CardCmd.Enchant`、地图改写 `ModifyGeneratedMap`、新的奖励类型等）：先做其他遗物，最后再评估；实在做不了就跳过，在报告里写清卡在哪个 C# 方法上。
4. 每移植完一个（或一组紧密相关的）遗物就提交：`feat: port relic <类名>`。

## 验收

```bash
cmake --build build -j
~/.dotnet/dotnet build tools/Sts2Oracle -c Release -v quiet -nologo
SEEDS=$(python3 -c "print(','.join('NW%03d'%i for i in range(100)))")
python3 tests/l4/l4_run.py --seeds $SEEDS --policy-seeds 1,2          # 带 Neow
./build/sts2sim_cli smoke --seeds $SEEDS                               # 统计停止原因
```

- L4 **不许出现 `diverged`**。出现分歧时按 `DIFF` 行的 JSON 路径找模拟器首因，回到 C# 逐行核对；不许改 RNG、补抽随机数或放宽比对。
- 报告里列出每个遗物在 L4 里被选中的次数和结果；blocker 列表里不应再出现这 30 个遗物（跳过的除外）。
- `scripts/check.sh` 全部通过。

## 交付

- 报告 `docs/reports/2026-09-25-neow-relics.md`：遗物结果表、新增的卡牌、对框架的改动、跳过项及原因、最后一次完整对拍的命令和原始摘要输出。结论如实。

## 规则

- 先读 `AGENTS.md`、`docs/design-engine.md`、`docs/audits/pitfalls.md`；遗物样板看 `game/Models/Relics/BurningBlood.h`，事件与选项看 `game/Models/Events/Neow.h`、`game/Models/EventModel.cpp`。
- 不改 `tools/Sts2Oracle/`、`tests/`、`scripts/`。如果 oracle 在某个遗物上崩溃（无头环境缺 UI 节点），停下这个遗物，在报告里给出堆栈，继续下一个。
- 不提交构建产物、游戏文件、反编译源码、本地化文本。
