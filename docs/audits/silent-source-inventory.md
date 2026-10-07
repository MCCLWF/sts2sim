# Silent v0.111.0 源码模型、override 与分支清单

源码根目录：`STS2_SRC`（默认 `../Slay the Spire 2/src/Core`）。池成员/override 来自仓库的 v0.111.0 `data/content.json`；请用 `python3 tools/coverage/silent_source_inventory.py --check` 重生校验。

> 本文是源码审计分母，不把头文件、`IsUsable` 或无明显 stub 当成行为完成。每个模型的所有 override 与 C# 控制流检查点均列出；生成依赖来自源码中的模型类型引用闭包，继承 override 会追到实际声明源。逐项完成状态见 `docs/reports/silent-v0110/final-report.md`。

## 池分母

| 原版池 | 成员数 | 成员 |
|---|---:|---|
| `CARD_POOL.SILENT_CARD_POOL` | 91 | `CARD.ABRASIVE`, `CARD.ACCELERANT`, `CARD.ACCURACY`, `CARD.ACROBATICS`, `CARD.ADRENALINE`, `CARD.AFTERIMAGE`, `CARD.ANTICIPATE`, `CARD.ASSASSINATE`, `CARD.BACKFLIP`, `CARD.BACKSTAB`, `CARD.BLADE_OF_INK`, `CARD.BLADE_DANCE`, `CARD.BLADE_SYMPHONY`, `CARD.BLUR`, `CARD.BOUNCING_FLASK`, `CARD.BUBBLE_BUBBLE`, `CARD.BULLET_TIME`, `CARD.BURST`, `CARD.CALCULATED_GAMBLE`, `CARD.CLOAK_AND_DAGGER`, `CARD.CORROSIVE_WAVE`, `CARD.CONCOCT`, `CARD.DAGGER_SPRAY`, `CARD.DAGGER_THROW`, `CARD.DASH`, `CARD.DEADLY_POISON`, `CARD.DEFEND_SILENT`, `CARD.DEFLECT`, `CARD.DODGE_AND_ROLL`, `CARD.ECHOING_SLASH`, `CARD.ENVENOM`, `CARD.ESCAPE_PLAN`, `CARD.EXPERTISE`, `CARD.EXPOSE`, `CARD.FADE`, `CARD.FAN_OF_KNIVES`, `CARD.FINISHER`, `CARD.FLANKING`, `CARD.FLECHETTES`, `CARD.FLICK_FLACK`, `CARD.SIDESTEP`, `CARD.FOOTWORK`, `CARD.GRAND_FINALE`, `CARD.HAND_TRICK`, `CARD.HAZE`, `CARD.HIDDEN_DAGGERS`, `CARD.INFINITE_BLADES`, `CARD.KNIFE_TRAP`, `CARD.LEADING_STRIKE`, `CARD.LEG_SWEEP`, `CARD.MALAISE`, `CARD.MASTER_PLANNER`, `CARD.MEMENTO_MORI`, `CARD.MIRAGE`, `CARD.MURDER`, `CARD.NEUTRALIZE`, `CARD.NIGHTMARE`, `CARD.NOXIOUS_FUMES`, `CARD.OUTBREAK`, `CARD.PHANTOM_BLADES`, `CARD.PIERCING_WAIL`, `CARD.PINPOINT`, `CARD.POISONED_STAB`, `CARD.POUNCE`, `CARD.PRECISE_CUT`, `CARD.PREDATOR`, `CARD.PREPARED`, `CARD.REFLEX`, `CARD.RICOCHET`, `CARD.SERPENT_FORM`, `CARD.SHADOW_STEP`, `CARD.SHADOWMELD`, `CARD.SKEWER`, `CARD.SLICE`, `CARD.SNAKEBITE`, `CARD.SNEAKY`, `CARD.SPEEDSTER`, `CARD.STORM_OF_STEEL`, `CARD.STRANGLE`, `CARD.STRIKE_SILENT`, `CARD.SUCKER_PUNCH`, `CARD.SUPPRESS`, `CARD.SURVIVOR`, `CARD.TACTICIAN`, `CARD.THE_HUNT`, `CARD.TOOLS_OF_THE_TRADE`, `CARD.TRACKING`, `CARD.UNTOUCHABLE`, `CARD.UP_MY_SLEEVE`, `CARD.WELL_LAID_PLANS`, `CARD.WRAITH_FORM` |
| `RELIC_POOL.SILENT_RELIC_POOL` | 8 | `RELIC.HELICAL_DART`, `RELIC.NINJA_SCROLL`, `RELIC.PAPER_KRANE`, `RELIC.RING_OF_THE_SNAKE`, `RELIC.SNECKO_SKULL`, `RELIC.TINGSHA`, `RELIC.TOUGH_BANDAGES`, `RELIC.TWISTED_FUNNEL` |
| `POTION_POOL.SILENT_POTION_POOL` | 3 | `POTION.POISON_POTION`, `POTION.GHOST_IN_A_JAR`, `POTION.CUNNING_POTION` |

## 模型逐项清单

### CardModel（92）

#### `CARD.ABRASIVE`

- 类别/可达来源：`card pool`；源：`Models/Cards/Abrasive.cs`；C++：`game/Models/Cards/Abrasive.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalKeywords`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`POWER.DEXTERITY_POWER`, `POWER.THORNS_POWER`
- 源码分支/控制流检查点：0

#### `CARD.ACCELERANT`

- 类别/可达来源：`card pool`；源：`Models/Cards/Accelerant.cs`；C++：`game/Models/Cards/Accelerant.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`POWER.ACCELERANT_POWER`, `POWER.POISON_POWER`
- 源码分支/控制流检查点：0

#### `CARD.ACCURACY`

- 类别/可达来源：`card pool`；源：`Models/Cards/Accuracy.cs`；C++：`game/Models/Cards/Accuracy.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`CARD.SHIV`, `POWER.ACCURACY_POWER`
- 源码分支/控制流检查点：0

#### `CARD.ACROBATICS`

- 类别/可达来源：`card pool`；源：`Models/Cards/Acrobatics.cs`；C++：`game/Models/Cards/Acrobatics.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（1）：
  - `Models/Cards/Acrobatics.cs:25` **flow / OnPlay** `if (cardModel != null)`

#### `CARD.ADRENALINE`

- 类别/可达来源：`card pool`；源：`Models/Cards/Adrenaline.cs`；C++：`game/Models/Cards/Adrenaline.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalKeywords`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（1）：
  - `Models/Cards/Adrenaline.cs:31` **flow / OnPlay** `if (LocalContext.IsMe(base.Owner))`

#### `CARD.AFTERIMAGE`

- 类别/可达来源：`card pool`；源：`Models/Cards/Afterimage.cs`；C++：`game/Models/Cards/Afterimage.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`POWER.AFTERIMAGE_POWER`
- 源码分支/控制流检查点：0

#### `CARD.ANTICIPATE`

- 类别/可达来源：`card pool`；源：`Models/Cards/Anticipate.cs`；C++：`game/Models/Cards/Anticipate.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`POWER.ANTICIPATE_POWER`, `POWER.DEXTERITY_POWER`
- 源码分支/控制流检查点：0

#### `CARD.ASSASSINATE`

- 类别/可达来源：`card pool`；源：`Models/Cards/Assassinate.cs`；C++：`game/Models/Cards/Assassinate.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalKeywords`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`POWER.VULNERABLE_POWER`
- 源码分支/控制流检查点：0

#### `CARD.BACKFLIP`

- 类别/可达来源：`card pool`；源：`Models/Cards/Backflip.cs`；C++：`game/Models/Cards/Backflip.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_GainsBlock`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点：0

#### `CARD.BACKSTAB`

- 类别/可达来源：`card pool`；源：`Models/Cards/Backstab.cs`；C++：`game/Models/Cards/Backstab.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalKeywords`
  - `CardModel.get_CanonicalVars`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点：0

#### `CARD.BLADE_DANCE`

- 类别/可达来源：`card pool`；源：`Models/Cards/BladeDance.cs`；C++：`game/Models/Cards/BladeDance.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalKeywords`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`CARD.SHIV`
- 源码分支/控制流检查点（1）：
  - `Models/Cards/BladeDance.cs:27` **flow / OnPlay** `for (int i = 0; i < base.DynamicVars.Cards.IntValue; i++)`

#### `CARD.BLADE_OF_INK`

- 类别/可达来源：`card pool`；源：`Models/Cards/BladeOfInk.cs`；C++：`game/Models/Cards/BladeOfInk.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`CARD.SHIV`, `ENCHANTMENT.INKY`
- 源码分支/控制流检查点（1）：
  - `Models/Cards/BladeOfInk.cs:34` **flow / OnPlay** `foreach (CardModel item in await Shiv.CreateInHand(base.Owner, base.DynamicVars.Cards.IntValue, base.CombatState))`

#### `CARD.BLADE_SYMPHONY`

- 类别/可达来源：`card pool`；源：`Models/Cards/BladeSymphony.cs`；C++：`game/Models/Cards/BladeSymphony.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
  - `CardModel.get_MultiplayerConstraint`
- 生成/模型依赖：`CARD.SHIV`
- 源码分支/控制流检查点（5）：
  - `Models/Cards/BladeSymphony.cs:29` **flow / OnPlay** `if (base.CombatState == null)`
  - `Models/Cards/BladeSymphony.cs:34` **short-circuit / OnPlay** `where c != null && c.IsAlive && c.IsPlayer`
  - `Models/Cards/BladeSymphony.cs:34` **short-circuit / OnPlay** `where c != null && c.IsAlive && c.IsPlayer`
  - `Models/Cards/BladeSymphony.cs:36` **flow / OnPlay** `foreach (Creature teammate in enumerable)`
  - `Models/Cards/BladeSymphony.cs:38` **flow / OnPlay** `for (int i = 0; i < base.DynamicVars.Cards.IntValue; i++)`

#### `CARD.BLUR`

- 类别/可达来源：`card pool`；源：`Models/Cards/Blur.cs`；C++：`game/Models/Cards/Blur.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_GainsBlock`
- 生成/模型依赖：`POWER.BLUR_POWER`
- 源码分支/控制流检查点：0

#### `CARD.BOUNCING_FLASK`

- 类别/可达来源：`card pool`；源：`Models/Cards/BouncingFlask.cs`；C++：`game/Models/Cards/BouncingFlask.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`POTION.POISON_POTION`, `POWER.POISON_POWER`
- 源码分支/控制流检查点（5）：
  - `Models/Cards/BouncingFlask.cs:41` **flow / OnPlay** `for (int i = 0; i < base.DynamicVars.Repeat.IntValue; i++)`
  - `Models/Cards/BouncingFlask.cs:44` **flow / OnPlay** `if (enemy == null)`
  - `Models/Cards/BouncingFlask.cs:48` **flow / OnPlay** `if (TestMode.IsOff)`
  - `Models/Cards/BouncingFlask.cs:50` **flow / OnPlay** `if (i == 0)`
  - `Models/Cards/BouncingFlask.cs:55` **flow / OnPlay** `if (targetNode != null)`

#### `CARD.BUBBLE_BUBBLE`

- 类别/可达来源：`card pool`；源：`Models/Cards/BubbleBubble.cs`；C++：`game/Models/Cards/BubbleBubble.h`（header present; `IsUsable`=yes）
- C# override 清单（6）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
  - `CardModel.get_ExtraRunAssetPaths`
  - `CardModel.get_ShouldGlowGoldInternal`
- 生成/模型依赖：`POWER.POISON_POWER`
- 源码分支/控制流检查点（3）：
  - `Models/Cards/BubbleBubble.cs:22` **short-circuit / ShouldGlowGoldInternal** `protected override bool ShouldGlowGoldInternal => base.CombatState?.HittableEnemies.Any((Creature e) => e.HasPower<PoisonPower>()) ?? false;`
  - `Models/Cards/BubbleBubble.cs:39` **flow / OnPlay** `if (nCreature != null)`
  - `Models/Cards/BubbleBubble.cs:45` **flow / OnPlay** `if (cardPlay.Target.HasPower<PoisonPower>())`

#### `CARD.BULLET_TIME`

- 类别/可达来源：`card pool`；源：`Models/Cards/BulletTime.cs`；C++：`game/Models/Cards/BulletTime.h`（header present; `IsUsable`=yes）
- C# override 清单（2）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
- 生成/模型依赖：`POWER.NO_DRAW_POWER`
- 源码分支/控制流检查点（2）：
  - `Models/Cards/BulletTime.cs:19` **flow / OnPlay** `foreach (CardModel card in PileType.Hand.GetPile(base.Owner).Cards)`
  - `Models/Cards/BulletTime.cs:21` **flow / OnPlay** `if (!card.EnergyCost.CostsX)`

#### `CARD.BURST`

- 类别/可达来源：`card pool`；源：`Models/Cards/Burst.cs`；C++：`game/Models/Cards/Burst.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
- 生成/模型依赖：`POWER.BURST_POWER`
- 源码分支/控制流检查点：0

#### `CARD.CALCULATED_GAMBLE`

- 类别/可达来源：`card pool`；源：`Models/Cards/CalculatedGamble.cs`；C++：`game/Models/Cards/CalculatedGamble.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalKeywords`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点：0

#### `CARD.CLOAK_AND_DAGGER`

- 类别/可达来源：`card pool`；源：`Models/Cards/CloakAndDagger.cs`；C++：`game/Models/Cards/CloakAndDagger.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
  - `CardModel.get_GainsBlock`
- 生成/模型依赖：`CARD.SHIV`
- 源码分支/控制流检查点（1）：
  - `Models/Cards/CloakAndDagger.cs:32` **flow / OnPlay** `for (int i = 0; i < base.DynamicVars.Cards.IntValue; i++)`

#### `CARD.CONCOCT`

- 类别/可达来源：`card pool`；源：`Models/Cards/Concoct.cs`；C++：`game/Models/Cards/Concoct.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
  - `CardModel.get_MultiplayerConstraint`
- 生成/模型依赖：`POWER.CONCOCT_POWER`, `POWER.POISON_POWER`
- 源码分支/控制流检查点：0

#### `CARD.CORROSIVE_WAVE`

- 类别/可达来源：`card pool`；源：`Models/Cards/CorrosiveWave.cs`；C++：`game/Models/Cards/CorrosiveWave.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`POWER.CORROSIVE_WAVE_POWER`, `POWER.POISON_POWER`
- 源码分支/控制流检查点：0

#### `CARD.DAGGER_SPRAY`

- 类别/可达来源：`card pool`；源：`Models/Cards/DaggerSpray.cs`；C++：`game/Models/Cards/DaggerSpray.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（1）：
  - `Models/Cards/DaggerSpray.cs:36` **flow / OnPlay** `foreach (Creature item in hittableEnemies)`

#### `CARD.DAGGER_THROW`

- 类别/可达来源：`card pool`；源：`Models/Cards/DaggerThrow.cs`；C++：`game/Models/Cards/DaggerThrow.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（1）：
  - `Models/Cards/DaggerThrow.cs:35` **flow / OnPlay** `if (cardModel != null)`

#### `CARD.DASH`

- 类别/可达来源：`card pool`；源：`Models/Cards/Dash.cs`；C++：`game/Models/Cards/Dash.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_GainsBlock`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点：0

#### `CARD.DEADLY_POISON`

- 类别/可达来源：`card pool`；源：`Models/Cards/DeadlyPoison.cs`；C++：`game/Models/Cards/DeadlyPoison.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
  - `CardModel.get_ExtraRunAssetPaths`
- 生成/模型依赖：`POWER.POISON_POWER`
- 源码分支/控制流检查点：0

#### `CARD.DEFEND_SILENT`

- 类别/可达来源：`card pool`；源：`Models/Cards/DefendSilent.cs`；C++：`game/Models/Cards/DefendSilent.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalTags`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_GainsBlock`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点：0

#### `CARD.DEFLECT`

- 类别/可达来源：`card pool`；源：`Models/Cards/Deflect.cs`；C++：`game/Models/Cards/Deflect.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_GainsBlock`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点：0

#### `CARD.DODGE_AND_ROLL`

- 类别/可达来源：`card pool`；源：`Models/Cards/DodgeAndRoll.cs`；C++：`game/Models/Cards/DodgeAndRoll.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_GainsBlock`
- 生成/模型依赖：`POWER.BLOCK_NEXT_TURN_POWER`
- 源码分支/控制流检查点：0

#### `CARD.ECHOING_SLASH`

- 类别/可达来源：`card pool`；源：`Models/Cards/EchoingSlash.cs`；C++：`game/Models/Cards/EchoingSlash.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（1）：
  - `Models/Cards/EchoingSlash.cs:27` **flow / OnPlay** `while (attackCount > 0)`

#### `CARD.ENVENOM`

- 类别/可达来源：`card pool`；源：`Models/Cards/Envenom.cs`；C++：`game/Models/Cards/Envenom.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`POWER.ENVENOM_POWER`, `POWER.POISON_POWER`
- 源码分支/控制流检查点：0

#### `CARD.ESCAPE_PLAN`

- 类别/可达来源：`card pool`；源：`Models/Cards/EscapePlan.cs`；C++：`game/Models/Cards/EscapePlan.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_GainsBlock`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（2）：
  - `Models/Cards/EscapePlan.cs:26` **flow / OnPlay** `if (cardModel != null && cardModel.Type == CardType.Skill)`
  - `Models/Cards/EscapePlan.cs:26` **short-circuit / OnPlay** `if (cardModel != null && cardModel.Type == CardType.Skill)`

#### `CARD.EXPERTISE`

- 类别/可达来源：`card pool`；源：`Models/Cards/Expertise.cs`；C++：`game/Models/Cards/Expertise.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（1）：
  - `Models/Cards/Expertise.cs:21` **flow / OnPlay** `foreach (CardModel item in await CardPileCmd.Draw(choiceContext, base.DynamicVars.Cards.IntValue, base.Owner))`

#### `CARD.EXPOSE`

- 类别/可达来源：`card pool`；源：`Models/Cards/Expose.cs`；C++：`game/Models/Cards/Expose.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalKeywords`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`POWER.ARTIFACT_POWER`, `POWER.VULNERABLE_POWER`
- 源码分支/控制流检查点（1）：
  - `Models/Cards/Expose.cs:40` **flow / OnPlay** `if (cardPlay.Target.HasPower<ArtifactPower>())`

#### `CARD.FADE`

- 类别/可达来源：`card pool`；源：`Models/Cards/Fade.cs`；C++：`game/Models/Cards/Fade.h`（header present; `IsUsable`=yes）
- C# override 清单（6）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalKeywords`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
  - `CardModel.get_MultiplayerConstraint`
- 生成/模型依赖：`POWER.DEXTERITY_POWER`, `POWER.FADE_POWER`
- 源码分支/控制流检查点：0

#### `CARD.FAN_OF_KNIVES`

- 类别/可达来源：`card pool`；源：`Models/Cards/FanOfKnives.cs`；C++：`game/Models/Cards/FanOfKnives.h`（header present; `IsUsable`=yes）
- C# override 清单（6）：
  - `CardModel.OnEnqueuePlayVfx`
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
  - `CardModel.get_ExtraRunAssetPaths`
- 生成/模型依赖：`CARD.SHIV`, `POWER.FAN_OF_KNIVES_POWER`
- 源码分支/控制流检查点（1）：
  - `Models/Cards/FanOfKnives.cs:33` **flow / OnPlay** `for (int i = 0; i < base.DynamicVars["Shivs"].IntValue; i++)`

#### `CARD.FINISHER`

- 类别/可达来源：`card pool`；源：`Models/Cards/Finisher.cs`；C++：`game/Models/Cards/Finisher.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（2）：
  - `Models/Cards/Finisher.cs:26` **short-circuit / CanonicalVars** `new CalculatedVar("CalculatedHits").WithMultiplier((CardModel card, Creature? _) => CombatManager.Instance.History.CardPlaysFinished.Count((CardPlayFinishedEntry e) => e.HappenedThisTurn(card.CombatState) && e.CardPlay.Card.Type == CardType.Attack && e.CardPlay.Player == card.Owner))`
  - `Models/Cards/Finisher.cs:26` **short-circuit / CanonicalVars** `new CalculatedVar("CalculatedHits").WithMultiplier((CardModel card, Creature? _) => CombatManager.Instance.History.CardPlaysFinished.Count((CardPlayFinishedEntry e) => e.HappenedThisTurn(card.CombatState) && e.CardPlay.Card.Type == CardType.Attack && e.CardPlay.Player == card.Owner))`

#### `CARD.FLANKING`

- 类别/可达来源：`card pool`；源：`Models/Cards/Flanking.cs`；C++：`game/Models/Cards/Flanking.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_MultiplayerConstraint`
- 生成/模型依赖：`POWER.FLANKING_POWER`
- 源码分支/控制流检查点：0

#### `CARD.FLECHETTES`

- 类别/可达来源：`card pool`；源：`Models/Cards/Flechettes.cs`；C++：`game/Models/Cards/Flechettes.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点：0

#### `CARD.FLICK_FLACK`

- 类别/可达来源：`card pool`；源：`Models/Cards/FlickFlack.cs`；C++：`game/Models/Cards/FlickFlack.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalKeywords`
  - `CardModel.get_CanonicalVars`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点：0

#### `CARD.FOOTWORK`

- 类别/可达来源：`card pool`；源：`Models/Cards/Footwork.cs`；C++：`game/Models/Cards/Footwork.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`POWER.DEXTERITY_POWER`
- 源码分支/控制流检查点：0

#### `CARD.GRAND_FINALE`

- 类别/可达来源：`card pool`；源：`Models/Cards/GrandFinale.cs`；C++：`game/Models/Cards/GrandFinale.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_IsPlayable`
  - `CardModel.get_ShouldGlowGoldInternal`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（1）：
  - `Models/Cards/GrandFinale.cs:30` **flow / OnPlay** `if (nGrandFinaleVfx != null)`

#### `CARD.HAND_TRICK`

- 类别/可达来源：`card pool`；源：`Models/Cards/HandTrick.cs`；C++：`game/Models/Cards/HandTrick.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
  - `CardModel.get_GainsBlock`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（2）：
  - `Models/Cards/HandTrick.cs:30` **short-circuit / OnPlay** `CardModel cardModel = (await CardSelectCmd.FromHand(prefs: new CardSelectorPrefs(base.SelectionScreenPrompt, 1), context: choiceContext, player: base.Owner, filter: (CardModel card) => card.Type == CardType.Skill && !card.IsSlyThisTurn, source: this)).FirstOrDefault();`
  - `Models/Cards/HandTrick.cs:31` **flow / OnPlay** `if (cardModel != null)`

#### `CARD.HAZE`

- 类别/可达来源：`card pool`；源：`Models/Cards/Haze.cs`；C++：`game/Models/Cards/Haze.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`POWER.POISON_POWER`, `POWER.WEAK_POWER`
- 源码分支/控制流检查点（2）：
  - `Models/Cards/Haze.cs:48` **flow / SpawnVfx** `if (node == null)`
  - `Models/Cards/Haze.cs:54` **flow / SpawnVfx** `foreach (Creature hittableEnemy in base.CombatState.HittableEnemies)`

#### `CARD.HIDDEN_DAGGERS`

- 类别/可达来源：`card pool`；源：`Models/Cards/HiddenDaggers.cs`；C++：`game/Models/Cards/HiddenDaggers.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `CardModel.OnPlay`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`CARD.SHIV`
- 源码分支/控制流检查点（2）：
  - `Models/Cards/HiddenDaggers.cs:33` **flow / OnPlay** `if (!base.IsUpgraded)`
  - `Models/Cards/HiddenDaggers.cs:37` **flow / OnPlay** `foreach (CardModel item in enumerable)`

#### `CARD.INFINITE_BLADES`

- 类别/可达来源：`card pool`；源：`Models/Cards/InfiniteBlades.cs`；C++：`game/Models/Cards/InfiniteBlades.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`CARD.SHIV`, `POWER.INFINITE_BLADES_POWER`
- 源码分支/控制流检查点：0

#### `CARD.KNIFE_TRAP`

- 类别/可达来源：`card pool`；源：`Models/Cards/KnifeTrap.cs`；C++：`game/Models/Cards/KnifeTrap.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `CardModel.OnPlay`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`CARD.SHIV`
- 源码分支/控制流检查点（2）：
  - `Models/Cards/KnifeTrap.cs:38` **flow / OnPlay** `foreach (CardModel item in enumerable)`
  - `Models/Cards/KnifeTrap.cs:40` **flow / OnPlay** `if (base.IsUpgraded)`

#### `CARD.LEADING_STRIKE`

- 类别/可达来源：`card pool`；源：`Models/Cards/LeadingStrike.cs`；C++：`game/Models/Cards/LeadingStrike.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalTags`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`CARD.SHIV`
- 源码分支/控制流检查点（1）：
  - `Models/Cards/LeadingStrike.cs:38` **flow / OnPlay** `for (int i = 0; i < base.DynamicVars["Shivs"].IntValue; i++)`

#### `CARD.LEG_SWEEP`

- 类别/可达来源：`card pool`；源：`Models/Cards/LegSweep.cs`；C++：`game/Models/Cards/LegSweep.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
  - `CardModel.get_GainsBlock`
- 生成/模型依赖：`POWER.WEAK_POWER`
- 源码分支/控制流检查点：0

#### `CARD.MALAISE`

- 类别/可达来源：`card pool`；源：`Models/Cards/Malaise.cs`；C++：`game/Models/Cards/Malaise.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `CardModel.OnPlay`
  - `CardModel.get_CanonicalKeywords`
  - `CardModel.get_ExtraHoverTips`
  - `CardModel.get_HasEnergyCostX`
  - `CardModel.get_TargetType`
- 生成/模型依赖：`POWER.STRENGTH_POWER`, `POWER.WEAK_POWER`
- 源码分支/控制流检查点（1）：
  - `Models/Cards/Malaise.cs:35` **flow / OnPlay** `if (base.IsUpgraded)`

#### `CARD.MASTER_PLANNER`

- 类别/可达来源：`card pool`；源：`Models/Cards/MasterPlanner.cs`；C++：`game/Models/Cards/MasterPlanner.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`POWER.MASTER_PLANNER_POWER`
- 源码分支/控制流检查点：0

#### `CARD.MEMENTO_MORI`

- 类别/可达来源：`card pool`；源：`Models/Cards/MementoMori.cs`；C++：`game/Models/Cards/MementoMori.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（1）：
  - `Models/Cards/MementoMori.cs:22` **short-circuit / CanonicalVars** `new CalculatedDamageVar(ValueProp.Move).WithMultiplier((CardModel card, Creature? _) => CombatManager.Instance.History.Entries.OfType<CardDiscardedEntry>().Count((CardDiscardedEntry e) => e.HappenedThisTurn(card.CombatState) && e.Actor == card.Owner.Creature))`

#### `CARD.MIRAGE`

- 类别/可达来源：`card pool`；源：`Models/Cards/Mirage.cs`；C++：`game/Models/Cards/Mirage.h`（header present; `IsUsable`=yes）
- C# override 清单（6）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalKeywords`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
  - `CardModel.get_GainsBlock`
- 生成/模型依赖：`POWER.POISON_POWER`
- 源码分支/控制流检查点（1）：
  - `Models/Cards/Mirage.cs:25` **short-circuit / CanonicalVars** `new CalculatedBlockVar(ValueProp.Move).WithMultiplier((CardModel card, Creature? _) => card.CombatState?.Enemies.Where((Creature c) => c.IsAlive).Sum((Creature c) => c.GetPowerAmount<PoisonPower>()) ?? 0)`

#### `CARD.MURDER`

- 类别/可达来源：`card pool`；源：`Models/Cards/Murder.cs`；C++：`game/Models/Cards/Murder.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点：0

#### `CARD.NEUTRALIZE`

- 类别/可达来源：`card pool`；源：`Models/Cards/Neutralize.cs`；C++：`game/Models/Cards/Neutralize.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`POWER.WEAK_POWER`
- 源码分支/控制流检查点（1）：
  - `Models/Cards/Neutralize.cs:39` **flow / OnPlay** `if (SaveManager.Instance.PrefsSave.FastMode == FastModeType.Normal)`

#### `CARD.NIGHTMARE`

- 类别/可达来源：`card pool`；源：`Models/Cards/Nightmare.cs`；C++：`game/Models/Cards/Nightmare.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanBeGeneratedInCombat`
  - `CardModel.get_CanonicalKeywords`
  - `CardModel.get_ExtraRunAssetPaths`
- 生成/模型依赖：`POWER.NIGHTMARE_POWER`
- 源码分支/控制流检查点（3）：
  - `Models/Cards/Nightmare.cs:35` **flow / OnPlay** `if (TestMode.IsOff && LocalContext.IsMe(base.Owner))`
  - `Models/Cards/Nightmare.cs:35` **short-circuit / OnPlay** `if (TestMode.IsOff && LocalContext.IsMe(base.Owner))`
  - `Models/Cards/Nightmare.cs:44` **flow / OnPlay** `if (selectedCard != null)`

#### `CARD.NOXIOUS_FUMES`

- 类别/可达来源：`card pool`；源：`Models/Cards/NoxiousFumes.cs`；C++：`game/Models/Cards/NoxiousFumes.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`POWER.NOXIOUS_FUMES_POWER`, `POWER.POISON_POWER`
- 源码分支/控制流检查点：0

#### `CARD.OUTBREAK`

- 类别/可达来源：`card pool`；源：`Models/Cards/Outbreak.cs`；C++：`game/Models/Cards/Outbreak.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`POWER.POISON_POWER`
- 源码分支/控制流检查点（3）：
  - `Models/Cards/Outbreak.cs:30` **flow / OnPlay** `foreach (Creature hittableEnemy in base.CombatState.HittableEnemies)`
  - `Models/Cards/Outbreak.cs:36` **flow / OnPlay** `foreach (Creature hittableEnemy2 in base.CombatState.HittableEnemies)`
  - `Models/Cards/Outbreak.cs:39` **flow / OnPlay** `if (power != null)`

#### `CARD.PHANTOM_BLADES`

- 类别/可达来源：`card pool`；源：`Models/Cards/PhantomBlades.cs`；C++：`game/Models/Cards/PhantomBlades.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalTags`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`CARD.SHIV`, `POWER.PHANTOM_BLADES_POWER`
- 源码分支/控制流检查点：0

#### `CARD.PIERCING_WAIL`

- 类别/可达来源：`card pool`；源：`Models/Cards/PiercingWail.cs`；C++：`game/Models/Cards/PiercingWail.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalKeywords`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`POWER.PIERCING_WAIL_POWER`, `POWER.STRENGTH_POWER`
- 源码分支/控制流检查点（1）：
  - `Models/Cards/PiercingWail.cs:31` **flow / OnPlay** `foreach (Creature hittableEnemy in base.CombatState.HittableEnemies)`

#### `CARD.PINPOINT`

- 类别/可达来源：`card pool`；源：`Models/Cards/Pinpoint.cs`；C++：`game/Models/Cards/Pinpoint.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `AbstractModel.AfterCardEnteredCombat`
  - `AbstractModel.AfterCardPlayed`
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（6）：
  - `Models/Cards/Pinpoint.cs:39` **flow / AfterCardEnteredCombat** `if (card != this)`
  - `Models/Cards/Pinpoint.cs:43` **flow / AfterCardEnteredCombat** `if (base.IsClone)`
  - `Models/Cards/Pinpoint.cs:47` **short-circuit / AfterCardEnteredCombat** `int amount = CombatManager.Instance.History.CardPlaysFinished.Count((CardPlayFinishedEntry e) => e.CardPlay.Card.Type == CardType.Skill && e.CardPlay.Player == base.Owner && e.HappenedThisTurn(base.CombatState));`
  - `Models/Cards/Pinpoint.cs:47` **short-circuit / AfterCardEnteredCombat** `int amount = CombatManager.Instance.History.CardPlaysFinished.Count((CardPlayFinishedEntry e) => e.CardPlay.Card.Type == CardType.Skill && e.CardPlay.Player == base.Owner && e.HappenedThisTurn(base.CombatState));`
  - `Models/Cards/Pinpoint.cs:54` **flow / AfterCardPlayed** `if (cardPlay.Card.Owner != base.Owner)`
  - `Models/Cards/Pinpoint.cs:58` **flow / AfterCardPlayed** `if (cardPlay.Card.Type != CardType.Skill)`

#### `CARD.POISONED_STAB`

- 类别/可达来源：`card pool`；源：`Models/Cards/PoisonedStab.cs`；C++：`game/Models/Cards/PoisonedStab.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`POWER.POISON_POWER`
- 源码分支/控制流检查点：0

#### `CARD.POUNCE`

- 类别/可达来源：`card pool`；源：`Models/Cards/Pounce.cs`；C++：`game/Models/Cards/Pounce.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
- 生成/模型依赖：`POWER.FREE_SKILL_POWER`
- 源码分支/控制流检查点：0

#### `CARD.PRECISE_CUT`

- 类别/可达来源：`card pool`；源：`Models/Cards/PreciseCut.cs`；C++：`game/Models/Cards/PreciseCut.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（2）：
  - `Models/Cards/PreciseCut.cs:23` **flow / <type scope>** `if (pile != null && pile.Type == PileType.Hand)`
  - `Models/Cards/PreciseCut.cs:23` **short-circuit / <type scope>** `if (pile != null && pile.Type == PileType.Hand)`

#### `CARD.PREDATOR`

- 类别/可达来源：`card pool`；源：`Models/Cards/Predator.cs`；C++：`game/Models/Cards/Predator.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
- 生成/模型依赖：`POWER.DRAW_CARDS_NEXT_TURN_POWER`
- 源码分支/控制流检查点：0

#### `CARD.PREPARED`

- 类别/可达来源：`card pool`；源：`Models/Cards/Prepared.cs`；C++：`game/Models/Cards/Prepared.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点：0

#### `CARD.REFLEX`

- 类别/可达来源：`card pool`；源：`Models/Cards/Reflex.cs`；C++：`game/Models/Cards/Reflex.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalKeywords`
  - `CardModel.get_CanonicalVars`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点：0

#### `CARD.RICOCHET`

- 类别/可达来源：`card pool`；源：`Models/Cards/Ricochet.cs`；C++：`game/Models/Cards/Ricochet.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalKeywords`
  - `CardModel.get_CanonicalVars`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点：0

#### `CARD.SERPENT_FORM`

- 类别/可达来源：`card pool`；源：`Models/Cards/SerpentForm.cs`；C++：`game/Models/Cards/SerpentForm.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
- 生成/模型依赖：`POWER.SERPENT_FORM_POWER`
- 源码分支/控制流检查点：0

#### `CARD.SHADOWMELD`

- 类别/可达来源：`card pool`；源：`Models/Cards/Shadowmeld.cs`；C++：`game/Models/Cards/Shadowmeld.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`POWER.SHADOWMELD_POWER`
- 源码分支/控制流检查点：0

#### `CARD.SHADOW_STEP`

- 类别/可达来源：`card pool`；源：`Models/Cards/ShadowStep.cs`；C++：`game/Models/Cards/ShadowStep.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
- 生成/模型依赖：`POWER.SHADOW_STEP_POWER`
- 源码分支/控制流检查点：0

#### `CARD.SHIV`

- 类别/可达来源：`generated/reference from CARD.ACCURACY`；源：`Models/Cards/Shiv.cs`；C++：`game/Models/Cards/Shiv.h`（header present; `IsUsable`=yes）
- C# override 清单（6）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalKeywords`
  - `CardModel.get_CanonicalTags`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_TargetType`
- 生成/模型依赖：`CARD.SHIV`, `POWER.FAN_OF_KNIVES_POWER`
- 源码分支/控制流检查点（10）：
  - `Models/Cards/Shiv.cs:27` **flow / TargetType** `if (!HasFanOfKnives)`
  - `Models/Cards/Shiv.cs:45` **flow / HasFanOfKnives** `if (base.IsMutable && base.Owner != null)`
  - `Models/Cards/Shiv.cs:45` **short-circuit / HasFanOfKnives** `if (base.IsMutable && base.Owner != null)`
  - `Models/Cards/Shiv.cs:61` **flow / OnPlay** `if (HasFanOfKnives)`
  - `Models/Cards/Shiv.cs:66` **else / OnPlay** `else`
  - `Models/Cards/Shiv.cs:71` **flow / OnPlay** `if (base.Owner.Character is Silent)`
  - `Models/Cards/Shiv.cs:90` **flow / CreateInHand** `if (count == 0)`
  - `Models/Cards/Shiv.cs:94` **flow / CreateInHand** `if (CombatManager.Instance.IsOverOrEnding)`
  - `Models/Cards/Shiv.cs:99` **flow / CreateInHand** `for (int i = 0; i < count; i++)`
  - `Models/Cards/Shiv.cs:103` **short-circuit / CreateInHand** `await CardPileCmd.AddGeneratedCardsToCombat(shivs, PileType.Hand, creator ?? owner);`

#### `CARD.SIDESTEP`

- 类别/可达来源：`card pool`；源：`Models/Cards/Sidestep.cs`；C++：`game/Models/Cards/Sidestep.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`POWER.ENERGY_NEXT_TURN_POWER`
- 源码分支/控制流检查点：0

#### `CARD.SKEWER`

- 类别/可达来源：`card pool`；源：`Models/Cards/Skewer.cs`；C++：`game/Models/Cards/Skewer.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_HasEnergyCostX`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点：0

#### `CARD.SLICE`

- 类别/可达来源：`card pool`；源：`Models/Cards/Slice.cs`；C++：`game/Models/Cards/Slice.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（1）：
  - `Models/Cards/Slice.cs:31` **flow / OnPlay** `if (SaveManager.Instance.PrefsSave.FastMode == FastModeType.Normal)`

#### `CARD.SNAKEBITE`

- 类别/可达来源：`card pool`；源：`Models/Cards/Snakebite.cs`；C++：`game/Models/Cards/Snakebite.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalKeywords`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`POWER.POISON_POWER`
- 源码分支/控制流检查点：0

#### `CARD.SNEAKY`

- 类别/可达来源：`card pool`；源：`Models/Cards/Sneaky.cs`；C++：`game/Models/Cards/Sneaky.h`（header present; `IsUsable`=yes）
- C# override 清单（6）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalKeywords`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
  - `CardModel.get_MultiplayerConstraint`
- 生成/模型依赖：`POWER.SNEAKY_POWER`
- 源码分支/控制流检查点：0

#### `CARD.SPEEDSTER`

- 类别/可达来源：`card pool`；源：`Models/Cards/Speedster.cs`；C++：`game/Models/Cards/Speedster.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
- 生成/模型依赖：`POWER.SPEEDSTER_POWER`
- 源码分支/控制流检查点：0

#### `CARD.STORM_OF_STEEL`

- 类别/可达来源：`card pool`；源：`Models/Cards/StormOfSteel.cs`；C++：`game/Models/Cards/StormOfSteel.h`（header present; `IsUsable`=yes）
- C# override 清单（2）：
  - `CardModel.OnPlay`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`CARD.SHIV`
- 源码分支/控制流检查点（2）：
  - `Models/Cards/StormOfSteel.cs:27` **flow / OnPlay** `if (!base.IsUpgraded)`
  - `Models/Cards/StormOfSteel.cs:31` **flow / OnPlay** `foreach (CardModel item in enumerable2)`

#### `CARD.STRANGLE`

- 类别/可达来源：`card pool`；源：`Models/Cards/Strangle.cs`；C++：`game/Models/Cards/Strangle.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
- 生成/模型依赖：`POWER.STRANGLE_POWER`
- 源码分支/控制流检查点：0

#### `CARD.STRIKE_SILENT`

- 类别/可达来源：`card pool`；源：`Models/Cards/StrikeSilent.cs`；C++：`game/Models/Cards/StrikeSilent.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalTags`
  - `CardModel.get_CanonicalVars`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点：0

#### `CARD.SUCKER_PUNCH`

- 类别/可达来源：`card pool`；源：`Models/Cards/SuckerPunch.cs`；C++：`game/Models/Cards/SuckerPunch.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`POWER.WEAK_POWER`
- 源码分支/控制流检查点：0

#### `CARD.SUPPRESS`

- 类别/可达来源：`card pool`；源：`Models/Cards/Suppress.cs`；C++：`game/Models/Cards/Suppress.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalKeywords`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`POWER.WEAK_POWER`
- 源码分支/控制流检查点（1）：
  - `Models/Cards/Suppress.cs:41` **flow / OnPlay** `if (SaveManager.Instance.PrefsSave.FastMode == FastModeType.Normal)`

#### `CARD.SURVIVOR`

- 类别/可达来源：`card pool`；源：`Models/Cards/Survivor.cs`；C++：`game/Models/Cards/Survivor.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_GainsBlock`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（1）：
  - `Models/Cards/Survivor.cs:28` **flow / OnPlay** `if (cardModel != null)`

#### `CARD.TACTICIAN`

- 类别/可达来源：`card pool`；源：`Models/Cards/Tactician.cs`；C++：`game/Models/Cards/Tactician.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalKeywords`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点：0

#### `CARD.THE_HUNT`

- 类别/可达来源：`card pool`；源：`Models/Cards/TheHunt.cs`；C++：`game/Models/Cards/TheHunt.h`（header present; `IsUsable`=yes）
- C# override 清单（6）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanBeGeneratedInCombat`
  - `CardModel.get_CanonicalKeywords`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`POWER.THE_HUNT_POWER`
- 源码分支/控制流检查点（3）：
  - `Models/Cards/TheHunt.cs:38` **flow / OnPlay** `if (currentRoom is CombatRoom combatRoom)`
  - `Models/Cards/TheHunt.cs:45` **flow / OnPlay** `if (shouldTriggerFatal && attackCommand.Results.SelectMany((List<DamageResult> r) => r).Any((DamageResult r) => r.WasTargetKilled))`
  - `Models/Cards/TheHunt.cs:45` **short-circuit / OnPlay** `if (shouldTriggerFatal && attackCommand.Results.SelectMany((List<DamageResult> r) => r).Any((DamageResult r) => r.WasTargetKilled))`

#### `CARD.TOOLS_OF_THE_TRADE`

- 类别/可达来源：`card pool`；源：`Models/Cards/ToolsOfTheTrade.cs`；C++：`game/Models/Cards/ToolsOfTheTrade.h`（header present; `IsUsable`=yes）
- C# override 清单（2）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
- 生成/模型依赖：`POWER.TOOLS_OF_THE_TRADE_POWER`
- 源码分支/控制流检查点：0

#### `CARD.TRACKING`

- 类别/可达来源：`card pool`；源：`Models/Cards/Tracking.cs`；C++：`game/Models/Cards/Tracking.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`POWER.TRACKING_POWER`, `POWER.WEAK_POWER`
- 源码分支/控制流检查点：0

#### `CARD.UNTOUCHABLE`

- 类别/可达来源：`card pool`；源：`Models/Cards/Untouchable.cs`；C++：`game/Models/Cards/Untouchable.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalKeywords`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_GainsBlock`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点：0

#### `CARD.UP_MY_SLEEVE`

- 类别/可达来源：`card pool`；源：`Models/Cards/UpMySleeve.cs`；C++：`game/Models/Cards/UpMySleeve.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`CARD.SHIV`
- 源码分支/控制流检查点（1）：
  - `Models/Cards/UpMySleeve.cs:40` **flow / OnPlay** `for (int i = 0; i < base.DynamicVars.Cards.IntValue; i++)`

#### `CARD.WELL_LAID_PLANS`

- 类别/可达来源：`card pool`；源：`Models/Cards/WellLaidPlans.cs`；C++：`game/Models/Cards/WellLaidPlans.h`（header present; `IsUsable`=yes）
- C# override 清单（2）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
- 生成/模型依赖：`POWER.WELL_LAID_PLANS_POWER`
- 源码分支/控制流检查点：0

#### `CARD.WRAITH_FORM`

- 类别/可达来源：`card pool`；源：`Models/Cards/WraithForm.cs`；C++：`game/Models/Cards/WraithForm.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `CardModel.OnPlay`
  - `CardModel.OnUpgrade`
  - `CardModel.get_CanonicalVars`
  - `CardModel.get_ExtraHoverTips`
- 生成/模型依赖：`POWER.DEXTERITY_POWER`, `POWER.INTANGIBLE_POWER`, `POWER.WRAITH_FORM_POWER`
- 源码分支/控制流检查点：0

### CardPoolModel（1）

#### `CARD_POOL.SILENT_CARD_POOL`

- 类别/可达来源：`card pool`；源：`Models/CardPools/SilentCardPool.cs`；C++：`content tables / generic pool queries`（content tables / generic pool queries; `IsUsable`=N/A）
- C# override 清单（8）：
  - `CardPoolModel.FilterThroughEpochs`
  - `CardPoolModel.GenerateAllCards`
  - `CardPoolModel.get_CardFrameMaterialPath`
  - `CardPoolModel.get_DeckEntryCardColor`
  - `CardPoolModel.get_EnergyColorName`
  - `CardPoolModel.get_EnergyOutlineColor`
  - `CardPoolModel.get_IsColorless`
  - `CardPoolModel.get_Title`
- 生成/模型依赖：`CARD.ABRASIVE`, `CARD.ACCELERANT`, `CARD.ACCURACY`, `CARD.ACROBATICS`, `CARD.ADRENALINE`, `CARD.AFTERIMAGE`, `CARD.ANTICIPATE`, `CARD.ASSASSINATE`, `CARD.BACKFLIP`, `CARD.BACKSTAB`, `CARD.BLADE_DANCE`, `CARD.BLADE_OF_INK`, `CARD.BLADE_SYMPHONY`, `CARD.BLUR`, `CARD.BOUNCING_FLASK`, `CARD.BUBBLE_BUBBLE`, `CARD.BULLET_TIME`, `CARD.BURST`, `CARD.CALCULATED_GAMBLE`, `CARD.CLOAK_AND_DAGGER`, `CARD.CONCOCT`, `CARD.CORROSIVE_WAVE`, `CARD.DAGGER_SPRAY`, `CARD.DAGGER_THROW`, `CARD.DASH`, `CARD.DEADLY_POISON`, `CARD.DEFEND_SILENT`, `CARD.DEFLECT`, `CARD.DODGE_AND_ROLL`, `CARD.ECHOING_SLASH`, `CARD.ENVENOM`, `CARD.ESCAPE_PLAN`, `CARD.EXPERTISE`, `CARD.EXPOSE`, `CARD.FADE`, `CARD.FAN_OF_KNIVES`, `CARD.FINISHER`, `CARD.FLANKING`, `CARD.FLECHETTES`, `CARD.FLICK_FLACK`, `CARD.FOOTWORK`, `CARD.GRAND_FINALE`, `CARD.HAND_TRICK`, `CARD.HAZE`, `CARD.HIDDEN_DAGGERS`, `CARD.INFINITE_BLADES`, `CARD.KNIFE_TRAP`, `CARD.LEADING_STRIKE`, `CARD.LEG_SWEEP`, `CARD.MALAISE`, `CARD.MASTER_PLANNER`, `CARD.MEMENTO_MORI`, `CARD.MIRAGE`, `CARD.MURDER`, `CARD.NEUTRALIZE`, `CARD.NIGHTMARE`, `CARD.NOXIOUS_FUMES`, `CARD.OUTBREAK`, `CARD.PHANTOM_BLADES`, `CARD.PIERCING_WAIL`, `CARD.PINPOINT`, `CARD.POISONED_STAB`, `CARD.POUNCE`, `CARD.PRECISE_CUT`, `CARD.PREDATOR`, `CARD.PREPARED`, `CARD.REFLEX`, `CARD.RICOCHET`, `CARD.SERPENT_FORM`, `CARD.SHADOWMELD`, `CARD.SHADOW_STEP`, `CARD.SIDESTEP`, `CARD.SKEWER`, `CARD.SLICE`, `CARD.SNAKEBITE`, `CARD.SNEAKY`, `CARD.SPEEDSTER`, `CARD.STORM_OF_STEEL`, `CARD.STRANGLE`, `CARD.STRIKE_SILENT`, `CARD.SUCKER_PUNCH`, `CARD.SUPPRESS`, `CARD.SURVIVOR`, `CARD.TACTICIAN`, `CARD.THE_HUNT`, `CARD.TOOLS_OF_THE_TRADE`, `CARD.TRACKING`, `CARD.UNTOUCHABLE`, `CARD.UP_MY_SLEEVE`, `CARD.WELL_LAID_PLANS`, `CARD.WRAITH_FORM`
- 源码分支/控制流检查点（3）：
  - `Models/CardPools/SilentCardPool.cs:125` **flow / FilterThroughEpochs** `if (!unlockState.IsEpochRevealed<Silent2Epoch>())`
  - `Models/CardPools/SilentCardPool.cs:129` **flow / FilterThroughEpochs** `if (!unlockState.IsEpochRevealed<Silent5Epoch>())`
  - `Models/CardPools/SilentCardPool.cs:133` **flow / FilterThroughEpochs** `if (!unlockState.IsEpochRevealed<Silent7Epoch>())`

### EnchantmentModel（1）

#### `ENCHANTMENT.INKY`

- 类别/可达来源：`generated/reference from CARD.BLADE_OF_INK`；源：`Models/Enchantments/Inky.cs`；C++：`game/Models/Enchantments/Inky.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `EnchantmentModel.OnPlay`
  - `EnchantmentModel.get_CanonicalVars`
  - `EnchantmentModel.get_ExtraHoverTips`
  - `EnchantmentModel.get_HasExtraCardText`
  - `EnchantmentModel.get_ShowAmount`
- 生成/模型依赖：`POWER.WEAK_POWER`
- 源码分支/控制流检查点（2）：
  - `Models/Enchantments/Inky.cs:26` **flow / OnPlay** `if (base.Card.TargetType != TargetType.AllEnemies)`
  - `Models/Enchantments/Inky.cs:31` **else / OnPlay** `else`

### PotionModel（3）

#### `POTION.CUNNING_POTION`

- 类别/可达来源：`potion pool`；源：`Models/Potions/CunningPotion.cs`；C++：`game/Models/Potions/CunningPotion.h`（header present; `IsUsable`=yes）
- C# override 清单（6）：
  - `PotionModel.OnUse`
  - `PotionModel.get_CanonicalVars`
  - `PotionModel.get_ExtraHoverTips`
  - `PotionModel.get_Rarity`
  - `PotionModel.get_TargetType`
  - `PotionModel.get_Usage`
- 生成/模型依赖：`CARD.SHIV`
- 源码分支/控制流检查点（1）：
  - `Models/Potions/CunningPotion.cs:31` **flow / OnUse** `foreach (CardModel item in await Shiv.CreateInHand(player, base.DynamicVars.Cards.IntValue, target.CombatState, base.Owner))`

#### `POTION.GHOST_IN_A_JAR`

- 类别/可达来源：`potion pool`；源：`Models/Potions/GhostInAJar.cs`；C++：`game/Models/Potions/GhostInAJar.h`（header present; `IsUsable`=yes）
- C# override 清单（6）：
  - `PotionModel.OnUse`
  - `PotionModel.get_CanonicalVars`
  - `PotionModel.get_ExtraHoverTips`
  - `PotionModel.get_Rarity`
  - `PotionModel.get_TargetType`
  - `PotionModel.get_Usage`
- 生成/模型依赖：`POWER.INTANGIBLE_POWER`
- 源码分支/控制流检查点：0

#### `POTION.POISON_POTION`

- 类别/可达来源：`potion pool`；源：`Models/Potions/PoisonPotion.cs`；C++：`game/Models/Potions/PoisonPotion.h`（header present; `IsUsable`=yes）
- C# override 清单（6）：
  - `PotionModel.OnUse`
  - `PotionModel.get_CanonicalVars`
  - `PotionModel.get_ExtraHoverTips`
  - `PotionModel.get_Rarity`
  - `PotionModel.get_TargetType`
  - `PotionModel.get_Usage`
- 生成/模型依赖：`POWER.POISON_POWER`
- 源码分支/控制流检查点（1）：
  - `Models/Potions/PoisonPotion.cs:35` **flow / OnUse** `if (nCreature != null)`

### PotionPoolModel（1）

#### `POTION_POOL.SILENT_POTION_POOL`

- 类别/可达来源：`potion pool`；源：`Models/PotionPools/SilentPotionPool.cs`；C++：`content tables / generic pool queries`（content tables / generic pool queries; `IsUsable`=N/A）
- C# override 清单（4）：
  - `PotionPoolModel.GenerateAllPotions`
  - `PotionPoolModel.GetUnlockedPotions`
  - `PotionPoolModel.get_EnergyColorName`
  - `PotionPoolModel.get_LabOutlineColor`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（1）：
  - `Models/PotionPools/SilentPotionPool.cs:27` **flow / GetUnlockedPotions** `if (!unlockState.IsEpochRevealed<Silent4Epoch>())`

### PowerModel（46）

#### `POWER.ACCELERANT_POWER`

- 类别/可达来源：`generated/reference from CARD.ACCELERANT`；源：`Models/Powers/AccelerantPower.cs`；C++：`game/Models/Powers/AccelerantPower.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `PowerModel.get_ExtraHoverTips`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：`POWER.POISON_POWER`
- 源码分支/控制流检查点：0

#### `POWER.ACCURACY_POWER`

- 类别/可达来源：`generated/reference from CARD.ACCURACY`；源：`Models/Powers/AccuracyPower.cs`；C++：`game/Models/Powers/AccuracyPower.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `AbstractModel.ModifyDamageAdditive`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（4）：
  - `Models/Powers/AccuracyPower.cs:17` **flow / ModifyDamageAdditive** `if (base.Owner != dealer)`
  - `Models/Powers/AccuracyPower.cs:21` **flow / ModifyDamageAdditive** `if (!props.IsPoweredAttack())`
  - `Models/Powers/AccuracyPower.cs:25` **flow / ModifyDamageAdditive** `if (card == null)`
  - `Models/Powers/AccuracyPower.cs:29` **flow / ModifyDamageAdditive** `if (!card.Tags.Contains(CardTag.Shiv))`

#### `POWER.AFTERIMAGE_POWER`

- 类别/可达来源：`generated/reference from CARD.AFTERIMAGE`；源：`Models/Powers/AfterimagePower.cs`；C++：`game/Models/Powers/AfterimagePower.h`（header present; `IsUsable`=yes）
- C# override 清单（6）：
  - `AbstractModel.AfterCardPlayed`
  - `AbstractModel.BeforeCardPlayed`
  - `PowerModel.InitInternalData`
  - `PowerModel.get_ExtraHoverTips`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（4）：
  - `Models/Powers/AfterimagePower.cs:37` **flow / BeforeCardPlayed** `if (cardPlay.Card.Owner.Creature != base.Owner)`
  - `Models/Powers/AfterimagePower.cs:47` **flow / AfterCardPlayed** `if (cardPlay.Card.Owner.Creature == base.Owner && GetInternalData<Data>().amountsForPlayedCards.Remove(cardPlay.Card, out var value) && value > 0)`
  - `Models/Powers/AfterimagePower.cs:47` **short-circuit / AfterCardPlayed** `if (cardPlay.Card.Owner.Creature == base.Owner && GetInternalData<Data>().amountsForPlayedCards.Remove(cardPlay.Card, out var value) && value > 0)`
  - `Models/Powers/AfterimagePower.cs:47` **short-circuit / AfterCardPlayed** `if (cardPlay.Card.Owner.Creature == base.Owner && GetInternalData<Data>().amountsForPlayedCards.Remove(cardPlay.Card, out var value) && value > 0)`

#### `POWER.ANTICIPATE_POWER`

- 类别/可达来源：`generated/reference from CARD.ANTICIPATE`；源：`Models/Powers/AnticipatePower.cs`；C++：`game/Models/Powers/AnticipatePower.h`（header present; `IsUsable`=yes）
- C# override 清单（10）：
  - `AbstractModel.AfterPowerAmountChanged (via TemporaryDexterityPower)`
  - `AbstractModel.AfterSideTurnEnd (via TemporaryDexterityPower)`
  - `PowerModel.BeforeApplied (via TemporaryDexterityPower)`
  - `PowerModel.get_Description (via TemporaryDexterityPower)`
  - `PowerModel.get_ExtraHoverTips (via TemporaryDexterityPower)`
  - `PowerModel.get_SmartDescriptionLocKey (via TemporaryDexterityPower)`
  - `PowerModel.get_StackType (via TemporaryDexterityPower)`
  - `PowerModel.get_Title (via TemporaryDexterityPower)`
  - `PowerModel.get_Type (via TemporaryDexterityPower)`
  - `TemporaryDexterityPower.get_OriginModel`
- 生成/模型依赖：`CARD.ANTICIPATE`
- 源码分支/控制流检查点（14）：
  - `Models/Powers/TemporaryDexterityPower.cs:25` **flow / Type** `if (!IsPositive)`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:67` **flow / Title** `if (!(originModel is CardModel cardModel))`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:69` **flow / Title** `if (!(originModel is PotionModel potionModel))`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:71` **flow / Title** `if (originModel is RelicModel relicModel)`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:83` **ternary / Description** `public override LocString Description => new LocString("powers", IsPositive ? "TEMPORARY_DEXTERITY_POWER.description" : "TEMPORARY_DEXTERITY_DOWN.description");`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:89` **flow / SmartDescriptionLocKey** `if (!IsPositive)`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:105` **flow / ExtraHoverTips** `if (!(originModel is CardModel card))`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:107` **flow / ExtraHoverTips** `if (!(originModel is PotionModel))`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:109` **flow / ExtraHoverTips** `if (!(originModel is RelicModel relic))`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:115` **else / ExtraHoverTips** `else`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:120` **else / ExtraHoverTips** `else`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:137` **flow / AfterPowerAmountChanged** `if (!(amount == (decimal)base.Amount) && power == this)`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:137` **short-circuit / AfterPowerAmountChanged** `if (!(amount == (decimal)base.Amount) && power == this)`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:145` **flow / AfterSideTurnEnd** `if (participants.Contains(base.Owner))`（继承自 `TemporaryDexterityPower`）

#### `POWER.ARTIFACT_POWER`

- 类别/可达来源：`generated/reference from CARD.EXPOSE`；源：`Models/Powers/ArtifactPower.cs`；C++：`game/Models/Powers/ArtifactPower.h`（header present; `IsUsable`=yes）
- C# override 清单（6）：
  - `AbstractModel.AfterModifyingPowerAmountReceived`
  - `AbstractModel.TryModifyPowerAmountReceived`
  - `PowerModel.GetScaledAmountForMultiplayer`
  - `PowerModel.get_ShouldScaleInMultiplayer`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（3）：
  - `Models/Powers/ArtifactPower.cs:19` **flow / TryModifyPowerAmountReceived** `if (target != base.Owner)`
  - `Models/Powers/ArtifactPower.cs:24` **flow / TryModifyPowerAmountReceived** `if (canonicalPower.GetTypeForAmount(amount) != PowerType.Debuff)`
  - `Models/Powers/ArtifactPower.cs:29` **flow / TryModifyPowerAmountReceived** `if (!canonicalPower.IsVisible)`

#### `POWER.BLOCK_NEXT_TURN_POWER`

- 类别/可达来源：`generated/reference from CARD.DODGE_AND_ROLL`；源：`Models/Powers/BlockNextTurnPower.cs`；C++：`game/Models/Powers/BlockNextTurnPower.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `AbstractModel.AfterBlockCleared`
  - `PowerModel.get_ExtraHoverTips`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（1）：
  - `Models/Powers/BlockNextTurnPower.cs:21` **flow / AfterBlockCleared** `if (creature == base.Owner)`

#### `POWER.BLUR_POWER`

- 类别/可达来源：`generated/reference from CARD.BLUR`；源：`Models/Powers/BlurPower.cs`；C++：`game/Models/Powers/BlurPower.h`（header present; `IsUsable`=yes）
- C# override 清单（6）：
  - `AbstractModel.AfterPreventingBlockClear`
  - `AbstractModel.AfterSideTurnStart`
  - `AbstractModel.ShouldClearBlock`
  - `PowerModel.get_ExtraHoverTips`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（3）：
  - `Models/Powers/BlurPower.cs:22` **flow / ShouldClearBlock** `if (base.Owner != creature)`
  - `Models/Powers/BlurPower.cs:31` **flow / AfterPreventingBlockClear** `if (this != preventer)`
  - `Models/Powers/BlurPower.cs:41` **flow / AfterSideTurnStart** `if (participants.Contains(base.Owner))`

#### `POWER.BURST_POWER`

- 类别/可达来源：`generated/reference from CARD.BURST`；源：`Models/Powers/BurstPower.cs`；C++：`game/Models/Powers/BurstPower.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `AbstractModel.AfterModifyingCardPlayCount`
  - `AbstractModel.AfterSideTurnEnd`
  - `AbstractModel.ModifyCardPlayCount`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（3）：
  - `Models/Powers/BurstPower.cs:21` **flow / ModifyCardPlayCount** `if (card.Owner.Creature != base.Owner)`
  - `Models/Powers/BurstPower.cs:25` **flow / ModifyCardPlayCount** `if (card.Type != CardType.Skill)`
  - `Models/Powers/BurstPower.cs:39` **flow / AfterSideTurnEnd** `if (participants.Contains(base.Owner))`

#### `POWER.CONCOCT_POWER`

- 类别/可达来源：`generated/reference from CARD.CONCOCT`；源：`Models/Powers/ConcoctPower.cs`；C++：`game/Models/Powers/ConcoctPower.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `AbstractModel.AfterDamageGiven`
  - `AbstractModel.AfterSideTurnEnd`
  - `PowerModel.get_ExtraHoverTips`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：`POWER.POISON_POWER`
- 源码分支/控制流检查点（4）：
  - `Models/Powers/ConcoctPower.cs:23` **flow / AfterDamageGiven** `if (dealer == base.Owner && props.IsPoweredAttack() && result.UnblockedDamage > 0)`
  - `Models/Powers/ConcoctPower.cs:23` **short-circuit / AfterDamageGiven** `if (dealer == base.Owner && props.IsPoweredAttack() && result.UnblockedDamage > 0)`
  - `Models/Powers/ConcoctPower.cs:23` **short-circuit / AfterDamageGiven** `if (dealer == base.Owner && props.IsPoweredAttack() && result.UnblockedDamage > 0)`
  - `Models/Powers/ConcoctPower.cs:31` **flow / AfterSideTurnEnd** `if (base.Owner.Side != side)`

#### `POWER.CORROSIVE_WAVE_POWER`

- 类别/可达来源：`generated/reference from CARD.CORROSIVE_WAVE`；源：`Models/Powers/CorrosiveWavePower.cs`；C++：`game/Models/Powers/CorrosiveWavePower.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `AbstractModel.AfterCardDrawn`
  - `AbstractModel.AfterSideTurnEnd`
  - `PowerModel.get_ExtraHoverTips`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：`POWER.POISON_POWER`
- 源码分支/控制流检查点（4）：
  - `Models/Powers/CorrosiveWavePower.cs:28` **flow / AfterCardDrawn** `if (card.Owner.Creature != base.Owner)`
  - `Models/Powers/CorrosiveWavePower.cs:33` **flow / AfterCardDrawn** `foreach (Creature hittableEnemy in base.CombatState.HittableEnemies)`
  - `Models/Powers/CorrosiveWavePower.cs:36` **flow / AfterCardDrawn** `if (nCreature != null)`
  - `Models/Powers/CorrosiveWavePower.cs:47` **flow / AfterSideTurnEnd** `if (participants.Contains(base.Owner))`

#### `POWER.CRUELTY_POWER`

- 类别/可达来源：`generated/reference from POWER.VULNERABLE_POWER`；源：`Models/Powers/CrueltyPower.cs`；C++：`game/Models/Powers/CrueltyPower.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `PowerModel.get_ExtraHoverTips`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：`POWER.VULNERABLE_POWER`
- 源码分支/控制流检查点（2）：
  - `Models/Powers/CrueltyPower.cs:19` **flow / ModifyVulnerableMultiplier** `if (target == base.Owner)`
  - `Models/Powers/CrueltyPower.cs:23` **flow / ModifyVulnerableMultiplier** `if (!props.IsPoweredAttack())`

#### `POWER.DEBILITATE_POWER`

- 类别/可达来源：`generated/reference from POWER.VULNERABLE_POWER`；源：`Models/Powers/DebilitatePower.cs`；C++：`game/Models/Powers/DebilitatePower.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `AbstractModel.AfterSideTurnEnd`
  - `PowerModel.get_ExtraHoverTips`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：`POWER.VULNERABLE_POWER`, `POWER.WEAK_POWER`
- 源码分支/控制流检查点（5）：
  - `Models/Powers/DebilitatePower.cs:28` **flow / ModifyVulnerableMultiplier** `if (target != base.Owner)`
  - `Models/Powers/DebilitatePower.cs:32` **flow / ModifyVulnerableMultiplier** `if (!props.IsPoweredAttack())`
  - `Models/Powers/DebilitatePower.cs:41` **flow / ModifyWeakMultiplier** `if (dealer != base.Owner)`
  - `Models/Powers/DebilitatePower.cs:45` **flow / ModifyWeakMultiplier** `if (!props.IsPoweredAttack())`
  - `Models/Powers/DebilitatePower.cs:54` **flow / AfterSideTurnEnd** `if (participants.Contains(base.Owner))`

#### `POWER.DEXTERITY_POWER`

- 类别/可达来源：`generated/reference from CARD.ABRASIVE`；源：`Models/Powers/DexterityPower.cs`；C++：`game/Models/Powers/DexterityPower.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `AbstractModel.ModifyBlockAdditive`
  - `PowerModel.get_AllowNegative`
  - `PowerModel.get_ExtraHoverTips`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（4）：
  - `Models/Powers/DexterityPower.cs:22` **flow / ModifyBlockAdditive** `if (cardSource != null)`
  - `Models/Powers/DexterityPower.cs:24` **flow / ModifyBlockAdditive** `if (cardSource.Owner.Creature != base.Owner)`
  - `Models/Powers/DexterityPower.cs:29` **flow / ModifyBlockAdditive** `if (base.Owner != target)`
  - `Models/Powers/DexterityPower.cs:33` **flow / ModifyBlockAdditive** `if (!props.IsPoweredCardOrMonsterMoveBlock())`

#### `POWER.DOUBLE_DAMAGE_POWER`

- 类别/可达来源：`generated/reference from POWER.SHADOW_STEP_POWER`；源：`Models/Powers/DoubleDamagePower.cs`；C++：`game/Models/Powers/DoubleDamagePower.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `AbstractModel.AfterSideTurnEnd`
  - `AbstractModel.ModifyDamageMultiplicative`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（5）：
  - `Models/Powers/DoubleDamagePower.cs:22` **flow / ModifyDamageMultiplicative** `if (dealer != base.Owner && !base.Owner.Pets.Contains<Creature>(dealer))`
  - `Models/Powers/DoubleDamagePower.cs:22` **short-circuit / ModifyDamageMultiplicative** `if (dealer != base.Owner && !base.Owner.Pets.Contains<Creature>(dealer))`
  - `Models/Powers/DoubleDamagePower.cs:26` **flow / ModifyDamageMultiplicative** `if (!props.IsPoweredAttack())`
  - `Models/Powers/DoubleDamagePower.cs:30` **flow / ModifyDamageMultiplicative** `if (cardSource == null)`
  - `Models/Powers/DoubleDamagePower.cs:39` **flow / AfterSideTurnEnd** `if (participants.Contains(base.Owner))`

#### `POWER.DRAW_CARDS_NEXT_TURN_POWER`

- 类别/可达来源：`generated/reference from CARD.PREDATOR`；源：`Models/Powers/DrawCardsNextTurnPower.cs`；C++：`game/Models/Powers/DrawCardsNextTurnPower.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `AbstractModel.AfterSideTurnStart`
  - `AbstractModel.ModifyHandDraw`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（4）：
  - `Models/Powers/DrawCardsNextTurnPower.cs:24` **flow / ModifyHandDraw** `if (player != base.Owner.Player)`
  - `Models/Powers/DrawCardsNextTurnPower.cs:28` **flow / ModifyHandDraw** `if (base.AmountOnTurnStart == 0)`
  - `Models/Powers/DrawCardsNextTurnPower.cs:37` **flow / AfterSideTurnStart** `if (participants.Contains(base.Owner) && base.AmountOnTurnStart != 0)`
  - `Models/Powers/DrawCardsNextTurnPower.cs:37` **short-circuit / AfterSideTurnStart** `if (participants.Contains(base.Owner) && base.AmountOnTurnStart != 0)`

#### `POWER.ENERGY_NEXT_TURN_POWER`

- 类别/可达来源：`generated/reference from CARD.SIDESTEP`；源：`Models/Powers/EnergyNextTurnPower.cs`；C++：`game/Models/Powers/EnergyNextTurnPower.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `AbstractModel.AfterEnergyReset`
  - `PowerModel.get_ExtraHoverTips`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（1）：
  - `Models/Powers/EnergyNextTurnPower.cs:20` **flow / AfterEnergyReset** `if (player == base.Owner.Player)`

#### `POWER.ENVENOM_POWER`

- 类别/可达来源：`generated/reference from CARD.ENVENOM`；源：`Models/Powers/EnvenomPower.cs`；C++：`game/Models/Powers/EnvenomPower.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `AbstractModel.AfterDamageGiven`
  - `PowerModel.get_ExtraHoverTips`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：`POWER.POISON_POWER`
- 源码分支/控制流检查点（3）：
  - `Models/Powers/EnvenomPower.cs:22` **flow / AfterDamageGiven** `if (dealer == base.Owner && props.IsPoweredAttack() && result.UnblockedDamage > 0)`
  - `Models/Powers/EnvenomPower.cs:22` **short-circuit / AfterDamageGiven** `if (dealer == base.Owner && props.IsPoweredAttack() && result.UnblockedDamage > 0)`
  - `Models/Powers/EnvenomPower.cs:22` **short-circuit / AfterDamageGiven** `if (dealer == base.Owner && props.IsPoweredAttack() && result.UnblockedDamage > 0)`

#### `POWER.FADE_POWER`

- 类别/可达来源：`generated/reference from CARD.FADE`；源：`Models/Powers/FadePower.cs`；C++：`game/Models/Powers/FadePower.h`（header present; `IsUsable`=yes）
- C# override 清单（10）：
  - `AbstractModel.AfterPowerAmountChanged (via TemporaryDexterityPower)`
  - `AbstractModel.AfterSideTurnEnd (via TemporaryDexterityPower)`
  - `PowerModel.BeforeApplied (via TemporaryDexterityPower)`
  - `PowerModel.get_Description (via TemporaryDexterityPower)`
  - `PowerModel.get_ExtraHoverTips (via TemporaryDexterityPower)`
  - `PowerModel.get_SmartDescriptionLocKey (via TemporaryDexterityPower)`
  - `PowerModel.get_StackType (via TemporaryDexterityPower)`
  - `PowerModel.get_Title (via TemporaryDexterityPower)`
  - `PowerModel.get_Type (via TemporaryDexterityPower)`
  - `TemporaryDexterityPower.get_OriginModel`
- 生成/模型依赖：`CARD.FADE`
- 源码分支/控制流检查点（14）：
  - `Models/Powers/TemporaryDexterityPower.cs:25` **flow / Type** `if (!IsPositive)`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:67` **flow / Title** `if (!(originModel is CardModel cardModel))`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:69` **flow / Title** `if (!(originModel is PotionModel potionModel))`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:71` **flow / Title** `if (originModel is RelicModel relicModel)`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:83` **ternary / Description** `public override LocString Description => new LocString("powers", IsPositive ? "TEMPORARY_DEXTERITY_POWER.description" : "TEMPORARY_DEXTERITY_DOWN.description");`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:89` **flow / SmartDescriptionLocKey** `if (!IsPositive)`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:105` **flow / ExtraHoverTips** `if (!(originModel is CardModel card))`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:107` **flow / ExtraHoverTips** `if (!(originModel is PotionModel))`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:109` **flow / ExtraHoverTips** `if (!(originModel is RelicModel relic))`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:115` **else / ExtraHoverTips** `else`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:120` **else / ExtraHoverTips** `else`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:137` **flow / AfterPowerAmountChanged** `if (!(amount == (decimal)base.Amount) && power == this)`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:137` **short-circuit / AfterPowerAmountChanged** `if (!(amount == (decimal)base.Amount) && power == this)`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:145` **flow / AfterSideTurnEnd** `if (participants.Contains(base.Owner))`（继承自 `TemporaryDexterityPower`）

#### `POWER.FAN_OF_KNIVES_POWER`

- 类别/可达来源：`generated/reference from CARD.FAN_OF_KNIVES`；源：`Models/Powers/FanOfKnivesPower.cs`；C++：`game/Models/Powers/FanOfKnivesPower.h`（header present; `IsUsable`=yes）
- C# override 清单（2）：
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点：0

#### `POWER.FLANKING_POWER`

- 类别/可达来源：`generated/reference from CARD.FLANKING`；源：`Models/Powers/FlankingPower.cs`；C++：`game/Models/Powers/FlankingPower.h`（header present; `IsUsable`=yes）
- C# override 清单（7）：
  - `AbstractModel.AfterSideTurnEnd`
  - `AbstractModel.ModifyDamageMultiplicative`
  - `PowerModel.AfterApplied`
  - `PowerModel.get_CanonicalVars`
  - `PowerModel.get_InstanceType`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（4）：
  - `Models/Powers/FlankingPower.cs:37` **flow / ModifyDamageMultiplicative** `if (target != base.Owner)`
  - `Models/Powers/FlankingPower.cs:41` **flow / ModifyDamageMultiplicative** `if (!props.IsPoweredAttack())`
  - `Models/Powers/FlankingPower.cs:45` **flow / ModifyDamageMultiplicative** `if (dealer == base.Applier)`
  - `Models/Powers/FlankingPower.cs:54` **flow / AfterSideTurnEnd** `if (participants.Contains(base.Owner))`

#### `POWER.FREE_SKILL_POWER`

- 类别/可达来源：`generated/reference from CARD.POUNCE`；源：`Models/Powers/FreeSkillPower.cs`；C++：`game/Models/Powers/FreeSkillPower.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `AbstractModel.BeforeCardPlayed`
  - `AbstractModel.TryModifyEnergyCostInCombatLate`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（14）：
  - `Models/Powers/FreeSkillPower.cs:17` **flow / TryModifyEnergyCostInCombatLate** `if (card.Owner.Creature != base.Owner)`
  - `Models/Powers/FreeSkillPower.cs:21` **flow / TryModifyEnergyCostInCombatLate** `if (card.Type != CardType.Skill)`
  - `Models/Powers/FreeSkillPower.cs:26` **flow / TryModifyEnergyCostInCombatLate** `switch (card.Pile?.Type)`
  - `Models/Powers/FreeSkillPower.cs:28` **switch arm / TryModifyEnergyCostInCombatLate** `case PileType.Hand:`
  - `Models/Powers/FreeSkillPower.cs:29` **switch arm / TryModifyEnergyCostInCombatLate** `case PileType.Play:`
  - `Models/Powers/FreeSkillPower.cs:32` **switch arm / TryModifyEnergyCostInCombatLate** `default:`
  - `Models/Powers/FreeSkillPower.cs:36` **flow / TryModifyEnergyCostInCombatLate** `if (!flag)`
  - `Models/Powers/FreeSkillPower.cs:46` **flow / BeforeCardPlayed** `if (cardPlay.Card.Owner.Creature == base.Owner && cardPlay.Card.Type == CardType.Skill)`
  - `Models/Powers/FreeSkillPower.cs:46` **short-circuit / BeforeCardPlayed** `if (cardPlay.Card.Owner.Creature == base.Owner && cardPlay.Card.Type == CardType.Skill)`
  - `Models/Powers/FreeSkillPower.cs:49` **flow / BeforeCardPlayed** `switch (cardPlay.Card.Pile?.Type)`
  - `Models/Powers/FreeSkillPower.cs:51` **switch arm / BeforeCardPlayed** `case PileType.Hand:`
  - `Models/Powers/FreeSkillPower.cs:52` **switch arm / BeforeCardPlayed** `case PileType.Play:`
  - `Models/Powers/FreeSkillPower.cs:55` **switch arm / BeforeCardPlayed** `default:`
  - `Models/Powers/FreeSkillPower.cs:59` **flow / BeforeCardPlayed** `if (flag)`

#### `POWER.HELICAL_DART_POWER`

- 类别/可达来源：`generated/reference from RELIC.HELICAL_DART`；源：`Models/Powers/HelicalDartPower.cs`；C++：`game/Models/Powers/HelicalDartPower.h`（header present; `IsUsable`=yes）
- C# override 清单（10）：
  - `AbstractModel.AfterPowerAmountChanged (via TemporaryDexterityPower)`
  - `AbstractModel.AfterSideTurnEnd (via TemporaryDexterityPower)`
  - `PowerModel.BeforeApplied (via TemporaryDexterityPower)`
  - `PowerModel.get_Description (via TemporaryDexterityPower)`
  - `PowerModel.get_ExtraHoverTips (via TemporaryDexterityPower)`
  - `PowerModel.get_SmartDescriptionLocKey (via TemporaryDexterityPower)`
  - `PowerModel.get_StackType (via TemporaryDexterityPower)`
  - `PowerModel.get_Title (via TemporaryDexterityPower)`
  - `PowerModel.get_Type (via TemporaryDexterityPower)`
  - `TemporaryDexterityPower.get_OriginModel`
- 生成/模型依赖：`RELIC.HELICAL_DART`
- 源码分支/控制流检查点（14）：
  - `Models/Powers/TemporaryDexterityPower.cs:25` **flow / Type** `if (!IsPositive)`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:67` **flow / Title** `if (!(originModel is CardModel cardModel))`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:69` **flow / Title** `if (!(originModel is PotionModel potionModel))`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:71` **flow / Title** `if (originModel is RelicModel relicModel)`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:83` **ternary / Description** `public override LocString Description => new LocString("powers", IsPositive ? "TEMPORARY_DEXTERITY_POWER.description" : "TEMPORARY_DEXTERITY_DOWN.description");`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:89` **flow / SmartDescriptionLocKey** `if (!IsPositive)`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:105` **flow / ExtraHoverTips** `if (!(originModel is CardModel card))`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:107` **flow / ExtraHoverTips** `if (!(originModel is PotionModel))`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:109` **flow / ExtraHoverTips** `if (!(originModel is RelicModel relic))`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:115` **else / ExtraHoverTips** `else`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:120` **else / ExtraHoverTips** `else`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:137` **flow / AfterPowerAmountChanged** `if (!(amount == (decimal)base.Amount) && power == this)`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:137` **short-circuit / AfterPowerAmountChanged** `if (!(amount == (decimal)base.Amount) && power == this)`（继承自 `TemporaryDexterityPower`）
  - `Models/Powers/TemporaryDexterityPower.cs:145` **flow / AfterSideTurnEnd** `if (participants.Contains(base.Owner))`（继承自 `TemporaryDexterityPower`）

#### `POWER.INFINITE_BLADES_POWER`

- 类别/可达来源：`generated/reference from CARD.INFINITE_BLADES`；源：`Models/Powers/InfiniteBladesPower.cs`；C++：`game/Models/Powers/InfiniteBladesPower.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `AbstractModel.BeforeHandDraw`
  - `PowerModel.get_ExtraHoverTips`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：`CARD.SHIV`
- 源码分支/控制流检查点（1）：
  - `Models/Powers/InfiniteBladesPower.cs:22` **flow / BeforeHandDraw** `if (player == base.Owner.Player)`

#### `POWER.INTANGIBLE_POWER`

- 类别/可达来源：`generated/reference from CARD.WRAITH_FORM`；源：`Models/Powers/IntangiblePower.cs`；C++：`game/Models/Powers/IntangiblePower.h`（header present; `IsUsable`=yes）
- C# override 清单（7）：
  - `AbstractModel.AfterModifyingDamageAmount`
  - `AbstractModel.AfterModifyingHpLostAfterOsty`
  - `AbstractModel.AfterSideTurnEnd`
  - `AbstractModel.ModifyDamageCap`
  - `AbstractModel.ModifyHpLostAfterOsty`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（5）：
  - `Models/Powers/IntangiblePower.cs:21` **flow / ModifyHpLostAfterOsty** `if (!CombatManager.Instance.IsInProgress)`
  - `Models/Powers/IntangiblePower.cs:25` **flow / ModifyHpLostAfterOsty** `if (target != base.Owner)`
  - `Models/Powers/IntangiblePower.cs:29` **flow / ModifyHpLostAfterOsty** `if (amount < 1m)`
  - `Models/Powers/IntangiblePower.cs:49` **flow / ModifyDamageCap** `if (target != base.Owner)`
  - `Models/Powers/IntangiblePower.cs:68` **flow / AfterSideTurnEnd** `if (side == CombatSide.Enemy)`

#### `POWER.MASTER_PLANNER_POWER`

- 类别/可达来源：`generated/reference from CARD.MASTER_PLANNER`；源：`Models/Powers/MasterPlannerPower.cs`；C++：`game/Models/Powers/MasterPlannerPower.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `AbstractModel.AfterCardPlayed`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（2）：
  - `Models/Powers/MasterPlannerPower.cs:17` **flow / AfterCardPlayed** `if (cardPlay.Card.Owner != base.Owner.Player)`
  - `Models/Powers/MasterPlannerPower.cs:21` **flow / AfterCardPlayed** `if (cardPlay.Card.Type != CardType.Skill)`

#### `POWER.NIGHTMARE_POWER`

- 类别/可达来源：`generated/reference from CARD.NIGHTMARE`；源：`Models/Powers/NightmarePower.cs`；C++：`game/Models/Powers/NightmarePower.h`（header present; `IsUsable`=yes）
- C# override 清单（6）：
  - `AbstractModel.BeforeHandDraw`
  - `PowerModel.InitInternalData`
  - `PowerModel.get_CanonicalVars`
  - `PowerModel.get_InstanceType`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（2）：
  - `Models/Powers/NightmarePower.cs:43` **flow / BeforeHandDraw** `if (player == base.Owner.Player)`
  - `Models/Powers/NightmarePower.cs:46` **flow / BeforeHandDraw** `for (int i = 0; i < base.Amount; i++)`

#### `POWER.NOXIOUS_FUMES_POWER`

- 类别/可达来源：`generated/reference from CARD.NOXIOUS_FUMES`；源：`Models/Powers/NoxiousFumesPower.cs`；C++：`game/Models/Powers/NoxiousFumesPower.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `AbstractModel.AfterSideTurnStart`
  - `PowerModel.get_ExtraHoverTips`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：`POWER.POISON_POWER`
- 源码分支/控制流检查点（3）：
  - `Models/Powers/NoxiousFumesPower.cs:28` **flow / AfterSideTurnStart** `if (!participants.Contains(base.Owner))`
  - `Models/Powers/NoxiousFumesPower.cs:34` **flow / AfterSideTurnStart** `foreach (Creature hittableEnemy in base.CombatState.HittableEnemies)`
  - `Models/Powers/NoxiousFumesPower.cs:37` **flow / AfterSideTurnStart** `if (nCreature != null)`

#### `POWER.NO_DRAW_POWER`

- 类别/可达来源：`generated/reference from CARD.BULLET_TIME`；源：`Models/Powers/NoDrawPower.cs`；C++：`game/Models/Powers/NoDrawPower.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `AbstractModel.AfterSideTurnEnd`
  - `AbstractModel.ShouldDraw`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（3）：
  - `Models/Powers/NoDrawPower.cs:21` **flow / ShouldDraw** `if (fromHandDraw)`
  - `Models/Powers/NoDrawPower.cs:25` **flow / ShouldDraw** `if (player != base.Owner.Player)`
  - `Models/Powers/NoDrawPower.cs:35` **flow / AfterSideTurnEnd** `if (participants.Contains(base.Owner))`

#### `POWER.PHANTOM_BLADES_POWER`

- 类别/可达来源：`generated/reference from CARD.PHANTOM_BLADES`；源：`Models/Powers/PhantomBladesPower.cs`；C++：`game/Models/Powers/PhantomBladesPower.h`（header present; `IsUsable`=yes）
- C# override 清单（6）：
  - `AbstractModel.AfterCardEnteredCombat`
  - `AbstractModel.ModifyDamageAdditive`
  - `PowerModel.AfterApplied`
  - `PowerModel.get_ExtraHoverTips`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：`CARD.SHIV`
- 源码分支/控制流检查点（10）：
  - `Models/Powers/PhantomBladesPower.cs:30` **flow / AfterCardEnteredCombat** `if (!card.Tags.Contains(CardTag.Shiv))`
  - `Models/Powers/PhantomBladesPower.cs:34` **flow / AfterCardEnteredCombat** `if (card.Owner != base.Owner.Player)`
  - `Models/Powers/PhantomBladesPower.cs:44` **flow / AfterApplied** `foreach (CardModel item in base.Owner.Player.PlayerCombatState.AllCards.Where((CardModel c) => c.Tags.Contains(CardTag.Shiv)))`
  - `Models/Powers/PhantomBladesPower.cs:53` **flow / ModifyDamageAdditive** `if (!props.IsPoweredAttack())`
  - `Models/Powers/PhantomBladesPower.cs:57` **flow / ModifyDamageAdditive** `if (cardSource == null \|\| !cardSource.Tags.Contains(CardTag.Shiv))`
  - `Models/Powers/PhantomBladesPower.cs:57` **short-circuit / ModifyDamageAdditive** `if (cardSource == null \|\| !cardSource.Tags.Contains(CardTag.Shiv))`
  - `Models/Powers/PhantomBladesPower.cs:61` **flow / ModifyDamageAdditive** `if (dealer != base.Owner)`
  - `Models/Powers/PhantomBladesPower.cs:65` **short-circuit / ModifyDamageAdditive** `int num = CombatManager.Instance.History.CardPlaysFinished.Count((CardPlayFinishedEntry e) => e.HappenedThisTurn(base.CombatState) && e.CardPlay.Card.Tags.Contains(CardTag.Shiv) && e.CardPlay.Player == base.Owner.Player);`
  - `Models/Powers/PhantomBladesPower.cs:65` **short-circuit / ModifyDamageAdditive** `int num = CombatManager.Instance.History.CardPlaysFinished.Count((CardPlayFinishedEntry e) => e.HappenedThisTurn(base.CombatState) && e.CardPlay.Card.Tags.Contains(CardTag.Shiv) && e.CardPlay.Player == base.Owner.Player);`
  - `Models/Powers/PhantomBladesPower.cs:66` **flow / ModifyDamageAdditive** `if (num > 0)`

#### `POWER.PIERCING_WAIL_POWER`

- 类别/可达来源：`generated/reference from CARD.PIERCING_WAIL`；源：`Models/Powers/PiercingWailPower.cs`；C++：`game/Models/Powers/PiercingWailPower.h`（header present; `IsUsable`=yes）
- C# override 清单（11）：
  - `AbstractModel.AfterPowerAmountChanged (via TemporaryStrengthPower)`
  - `AbstractModel.AfterSideTurnEnd (via TemporaryStrengthPower)`
  - `PowerModel.BeforeApplied (via TemporaryStrengthPower)`
  - `PowerModel.get_Description (via TemporaryStrengthPower)`
  - `PowerModel.get_ExtraHoverTips (via TemporaryStrengthPower)`
  - `PowerModel.get_SmartDescriptionLocKey (via TemporaryStrengthPower)`
  - `PowerModel.get_StackType (via TemporaryStrengthPower)`
  - `PowerModel.get_Title (via TemporaryStrengthPower)`
  - `PowerModel.get_Type (via TemporaryStrengthPower)`
  - `TemporaryStrengthPower.get_IsPositive`
  - `TemporaryStrengthPower.get_OriginModel`
- 生成/模型依赖：`CARD.PIERCING_WAIL`
- 源码分支/控制流检查点（14）：
  - `Models/Powers/TemporaryStrengthPower.cs:29` **flow / Type** `if (!IsPositive)`（继承自 `TemporaryStrengthPower`）
  - `Models/Powers/TemporaryStrengthPower.cs:71` **flow / Title** `if (!(originModel is CardModel cardModel))`（继承自 `TemporaryStrengthPower`）
  - `Models/Powers/TemporaryStrengthPower.cs:73` **flow / Title** `if (!(originModel is PotionModel potionModel))`（继承自 `TemporaryStrengthPower`）
  - `Models/Powers/TemporaryStrengthPower.cs:75` **flow / Title** `if (originModel is RelicModel relicModel)`（继承自 `TemporaryStrengthPower`）
  - `Models/Powers/TemporaryStrengthPower.cs:87` **ternary / Description** `public override LocString Description => new LocString("powers", IsPositive ? "TEMPORARY_STRENGTH_POWER.description" : "TEMPORARY_STRENGTH_DOWN.description");`（继承自 `TemporaryStrengthPower`）
  - `Models/Powers/TemporaryStrengthPower.cs:93` **flow / SmartDescriptionLocKey** `if (!IsPositive)`（继承自 `TemporaryStrengthPower`）
  - `Models/Powers/TemporaryStrengthPower.cs:109` **flow / ExtraHoverTips** `if (!(originModel is CardModel card))`（继承自 `TemporaryStrengthPower`）
  - `Models/Powers/TemporaryStrengthPower.cs:111` **flow / ExtraHoverTips** `if (!(originModel is PotionModel))`（继承自 `TemporaryStrengthPower`）
  - `Models/Powers/TemporaryStrengthPower.cs:113` **flow / ExtraHoverTips** `if (!(originModel is RelicModel relic))`（继承自 `TemporaryStrengthPower`）
  - `Models/Powers/TemporaryStrengthPower.cs:119` **else / ExtraHoverTips** `else`（继承自 `TemporaryStrengthPower`）
  - `Models/Powers/TemporaryStrengthPower.cs:124` **else / ExtraHoverTips** `else`（继承自 `TemporaryStrengthPower`）
  - `Models/Powers/TemporaryStrengthPower.cs:141` **flow / AfterPowerAmountChanged** `if (!(amount == (decimal)base.Amount) && power == this)`（继承自 `TemporaryStrengthPower`）
  - `Models/Powers/TemporaryStrengthPower.cs:141` **short-circuit / AfterPowerAmountChanged** `if (!(amount == (decimal)base.Amount) && power == this)`（继承自 `TemporaryStrengthPower`）
  - `Models/Powers/TemporaryStrengthPower.cs:149` **flow / AfterSideTurnEnd** `if (participants.Contains(base.Owner))`（继承自 `TemporaryStrengthPower`）

#### `POWER.POISON_POWER`

- 类别/可达来源：`generated/reference from CARD.ACCELERANT`；源：`Models/Powers/PoisonPower.cs`；C++：`game/Models/Powers/PoisonPower.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `AbstractModel.AfterSideTurnStart`
  - `PowerModel.get_AmountLabelColor`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：`POWER.ACCELERANT_POWER`
- 源码分支/控制流检查点（5）：
  - `Models/Powers/PoisonPower.cs:46` **flow / CalculateTotalDamageNextTurn** `for (int i = 0; i < num2; i++)`
  - `Models/Powers/PoisonPower.cs:57` **flow / AfterSideTurnStart** `if (participants.Contains(base.Owner))`
  - `Models/Powers/PoisonPower.cs:69` **flow / Trigger** `for (int i = 0; i < iterations; i++)`
  - `Models/Powers/PoisonPower.cs:72` **flow / Trigger** `if (base.Owner.IsAlive)`
  - `Models/Powers/PoisonPower.cs:76` **else / Trigger** `else`

#### `POWER.SERPENT_FORM_POWER`

- 类别/可达来源：`generated/reference from CARD.SERPENT_FORM`；源：`Models/Powers/SerpentFormPower.cs`；C++：`game/Models/Powers/SerpentFormPower.h`（header present; `IsUsable`=yes）
- C# override 清单（7）：
  - `AbstractModel.AfterCardPlayed`
  - `AbstractModel.BeforeCardPlayed`
  - `PowerModel.AfterApplied`
  - `PowerModel.AfterRemoved`
  - `PowerModel.InitInternalData`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（8）：
  - `Models/Powers/SerpentFormPower.cs:37` **flow / Vfx** `if (_vfx == null)`
  - `Models/Powers/SerpentFormPower.cs:41` **flow / Vfx** `if (!_vfx.IsValid())`
  - `Models/Powers/SerpentFormPower.cs:73` **flow / BeforeCardPlayed** `if (cardPlay.Card.Owner != base.Owner.Player)`
  - `Models/Powers/SerpentFormPower.cs:83` **flow / AfterCardPlayed** `if (cardPlay.Card.Owner == base.Owner.Player && GetInternalData<Data>().amountsForPlayedCards.Remove(cardPlay.Card, out var damage) && damage > 0)`
  - `Models/Powers/SerpentFormPower.cs:83` **short-circuit / AfterCardPlayed** `if (cardPlay.Card.Owner == base.Owner.Player && GetInternalData<Data>().amountsForPlayedCards.Remove(cardPlay.Card, out var damage) && damage > 0)`
  - `Models/Powers/SerpentFormPower.cs:83` **short-circuit / AfterCardPlayed** `if (cardPlay.Card.Owner == base.Owner.Player && GetInternalData<Data>().amountsForPlayedCards.Remove(cardPlay.Card, out var damage) && damage > 0)`
  - `Models/Powers/SerpentFormPower.cs:86` **short-circuit / AfterCardPlayed** `Creature creature = base.Owner.Player.RunState.Rng.CombatTargets.NextItem(base.Owner.CombatState?.HittableEnemies ?? Array.Empty<Creature>());`
  - `Models/Powers/SerpentFormPower.cs:87` **flow / AfterCardPlayed** `if (creature != null)`

#### `POWER.SHADOWMELD_POWER`

- 类别/可达来源：`generated/reference from CARD.SHADOWMELD`；源：`Models/Powers/ShadowmeldPower.cs`；C++：`game/Models/Powers/ShadowmeldPower.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `AbstractModel.AfterSideTurnEnd`
  - `AbstractModel.ModifyBlockMultiplicative`
  - `PowerModel.get_ExtraHoverTips`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（2）：
  - `Models/Powers/ShadowmeldPower.cs:26` **flow / ModifyBlockMultiplicative** `if (base.Owner != target)`
  - `Models/Powers/ShadowmeldPower.cs:35` **flow / AfterSideTurnEnd** `if (participants.Contains(base.Owner))`

#### `POWER.SHADOW_STEP_POWER`

- 类别/可达来源：`generated/reference from CARD.SHADOW_STEP`；源：`Models/Powers/ShadowStepPower.cs`；C++：`game/Models/Powers/ShadowStepPower.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `AbstractModel.AfterSideTurnStart`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：`POWER.DOUBLE_DAMAGE_POWER`
- 源码分支/控制流检查点（1）：
  - `Models/Powers/ShadowStepPower.cs:20` **flow / AfterSideTurnStart** `if (participants.Contains(base.Owner))`

#### `POWER.SNEAKY_POWER`

- 类别/可达来源：`generated/reference from CARD.SNEAKY`；源：`Models/Powers/SneakyPower.cs`；C++：`game/Models/Powers/SneakyPower.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `AbstractModel.AfterCardPlayed`
  - `PowerModel.get_ExtraHoverTips`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（2）：
  - `Models/Powers/SneakyPower.cs:22` **flow / AfterCardPlayed** `if (cardPlay.Card.Owner.Creature != base.Owner && cardPlay.Card.Type == CardType.Attack)`
  - `Models/Powers/SneakyPower.cs:22` **short-circuit / AfterCardPlayed** `if (cardPlay.Card.Owner.Creature != base.Owner && cardPlay.Card.Type == CardType.Attack)`

#### `POWER.SPEEDSTER_POWER`

- 类别/可达来源：`generated/reference from CARD.SPEEDSTER`；源：`Models/Powers/SpeedsterPower.cs`；C++：`game/Models/Powers/SpeedsterPower.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `AbstractModel.AfterCardDrawn`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（3）：
  - `Models/Powers/SpeedsterPower.cs:17` **flow / AfterCardDrawn** `if (!fromHandDraw && card.Owner.Creature == base.Owner && card.Owner.Creature.CombatState.CurrentSide == card.Owner.Creature.Side)`
  - `Models/Powers/SpeedsterPower.cs:17` **short-circuit / AfterCardDrawn** `if (!fromHandDraw && card.Owner.Creature == base.Owner && card.Owner.Creature.CombatState.CurrentSide == card.Owner.Creature.Side)`
  - `Models/Powers/SpeedsterPower.cs:17` **short-circuit / AfterCardDrawn** `if (!fromHandDraw && card.Owner.Creature == base.Owner && card.Owner.Creature.CombatState.CurrentSide == card.Owner.Creature.Side)`

#### `POWER.STRANGLE_POWER`

- 类别/可达来源：`generated/reference from CARD.STRANGLE`；源：`Models/Powers/StranglePower.cs`；C++：`game/Models/Powers/StranglePower.h`（header present; `IsUsable`=yes）
- C# override 清单（7）：
  - `AbstractModel.AfterCardPlayed`
  - `AbstractModel.AfterSideTurnEnd`
  - `AbstractModel.BeforeCardPlayed`
  - `PowerModel.InitInternalData`
  - `PowerModel.get_InstanceType`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（4）：
  - `Models/Powers/StranglePower.cs:38` **flow / BeforeCardPlayed** `if (base.Applier?.Player == null)`
  - `Models/Powers/StranglePower.cs:42` **flow / BeforeCardPlayed** `if (cardPlay.Card.Owner != base.Applier.Player)`
  - `Models/Powers/StranglePower.cs:52` **flow / AfterCardPlayed** `if (GetInternalData<Data>().amountsForPlayedCards.Remove(cardPlay.Card, out var value))`
  - `Models/Powers/StranglePower.cs:61` **flow / AfterSideTurnEnd** `if (participants.Contains(base.Owner))`

#### `POWER.STRENGTH_POWER`

- 类别/可达来源：`generated/reference from CARD.MALAISE`；源：`Models/Powers/StrengthPower.cs`；C++：`game/Models/Powers/StrengthPower.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `AbstractModel.ModifyDamageAdditive`
  - `PowerModel.get_AllowNegative`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（2）：
  - `Models/Powers/StrengthPower.cs:18` **flow / ModifyDamageAdditive** `if (base.Owner != dealer)`
  - `Models/Powers/StrengthPower.cs:22` **flow / ModifyDamageAdditive** `if (!props.IsPoweredAttack())`

#### `POWER.THE_HUNT_POWER`

- 类别/可达来源：`generated/reference from CARD.THE_HUNT`；源：`Models/Powers/TheHuntPower.cs`；C++：`game/Models/Powers/TheHuntPower.h`（header present; `IsUsable`=yes）
- C# override 清单（2）：
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点：0

#### `POWER.THORNS_POWER`

- 类别/可达来源：`generated/reference from CARD.ABRASIVE`；源：`Models/Powers/ThornsPower.cs`；C++：`game/Models/Powers/ThornsPower.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `AbstractModel.BeforeDamageReceived`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（4）：
  - `Models/Powers/ThornsPower.cs:19` **flow / BeforeDamageReceived** `if (target == base.Owner && dealer != null && (props.IsPoweredAttack() \|\| cardSource is Omnislice))`
  - `Models/Powers/ThornsPower.cs:19` **short-circuit / BeforeDamageReceived** `if (target == base.Owner && dealer != null && (props.IsPoweredAttack() \|\| cardSource is Omnislice))`
  - `Models/Powers/ThornsPower.cs:19` **short-circuit / BeforeDamageReceived** `if (target == base.Owner && dealer != null && (props.IsPoweredAttack() \|\| cardSource is Omnislice))`
  - `Models/Powers/ThornsPower.cs:19` **short-circuit / BeforeDamageReceived** `if (target == base.Owner && dealer != null && (props.IsPoweredAttack() \|\| cardSource is Omnislice))`

#### `POWER.TOOLS_OF_THE_TRADE_POWER`

- 类别/可达来源：`generated/reference from CARD.TOOLS_OF_THE_TRADE`；源：`Models/Powers/ToolsOfTheTradePower.cs`；C++：`game/Models/Powers/ToolsOfTheTradePower.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `AbstractModel.AfterPlayerTurnStart`
  - `AbstractModel.ModifyHandDraw`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（3）：
  - `Models/Powers/ToolsOfTheTradePower.cs:20` **flow / ModifyHandDraw** `if (player != base.Owner.Player)`
  - `Models/Powers/ToolsOfTheTradePower.cs:29` **flow / AfterPlayerTurnStart** `if (player == base.Owner.Player)`
  - `Models/Powers/ToolsOfTheTradePower.cs:32` **flow / AfterPlayerTurnStart** `if (list.Count != 0)`

#### `POWER.TRACKING_POWER`

- 类别/可达来源：`generated/reference from CARD.TRACKING`；源：`Models/Powers/TrackingPower.cs`；C++：`game/Models/Powers/TrackingPower.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `AbstractModel.ModifyDamageMultiplicative`
  - `PowerModel.get_ExtraHoverTips`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：`POWER.WEAK_POWER`
- 源码分支/控制流检查点（6）：
  - `Models/Powers/TrackingPower.cs:21` **flow / ModifyDamageMultiplicative** `if (!props.IsPoweredAttack())`
  - `Models/Powers/TrackingPower.cs:25` **flow / ModifyDamageMultiplicative** `if (cardSource == null)`
  - `Models/Powers/TrackingPower.cs:29` **flow / ModifyDamageMultiplicative** `if (dealer != base.Owner && !base.Owner.Pets.Contains<Creature>(dealer))`
  - `Models/Powers/TrackingPower.cs:29` **short-circuit / ModifyDamageMultiplicative** `if (dealer != base.Owner && !base.Owner.Pets.Contains<Creature>(dealer))`
  - `Models/Powers/TrackingPower.cs:33` **flow / ModifyDamageMultiplicative** `if (target == null \|\| !target.HasPower<WeakPower>())`
  - `Models/Powers/TrackingPower.cs:33` **short-circuit / ModifyDamageMultiplicative** `if (target == null \|\| !target.HasPower<WeakPower>())`

#### `POWER.VULNERABLE_POWER`

- 类别/可达来源：`generated/reference from CARD.ASSASSINATE`；源：`Models/Powers/VulnerablePower.cs`；C++：`game/Models/Powers/VulnerablePower.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `AbstractModel.AfterSideTurnEnd`
  - `AbstractModel.ModifyDamageMultiplicative`
  - `PowerModel.get_CanonicalVars`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：`POWER.CRUELTY_POWER`, `POWER.DEBILITATE_POWER`, `RELIC.PAPER_PHROG`
- 源码分支/控制流检查点（8）：
  - `Models/Powers/VulnerablePower.cs:30` **flow / ModifyDamageMultiplicative** `if (target != base.Owner)`
  - `Models/Powers/VulnerablePower.cs:34` **flow / ModifyDamageMultiplicative** `if (!props.IsPoweredAttack())`
  - `Models/Powers/VulnerablePower.cs:39` **flow / ModifyDamageMultiplicative** `if (dealer != null)`
  - `Models/Powers/VulnerablePower.cs:42` **flow / ModifyDamageMultiplicative** `if (paperPhrog != null)`
  - `Models/Powers/VulnerablePower.cs:46` **short-circuit / ModifyDamageMultiplicative** `CrueltyPower crueltyPower = dealer.GetPower<CrueltyPower>() ?? dealer.PetOwner?.Creature.GetPower<CrueltyPower>();`
  - `Models/Powers/VulnerablePower.cs:47` **flow / ModifyDamageMultiplicative** `if (crueltyPower != null)`
  - `Models/Powers/VulnerablePower.cs:53` **flow / ModifyDamageMultiplicative** `if (power != null)`
  - `Models/Powers/VulnerablePower.cs:62` **flow / AfterSideTurnEnd** `if (side == CombatSide.Enemy)`

#### `POWER.WEAK_POWER`

- 类别/可达来源：`generated/reference from CARD.HAZE`；源：`Models/Powers/WeakPower.cs`；C++：`game/Models/Powers/WeakPower.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `AbstractModel.AfterSideTurnEnd`
  - `AbstractModel.ModifyDamageMultiplicative`
  - `PowerModel.get_CanonicalVars`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：`POWER.DEBILITATE_POWER`, `RELIC.PAPER_KRANE`
- 源码分支/控制流检查点（5）：
  - `Models/Powers/WeakPower.cs:27` **flow / ModifyDamageMultiplicative** `if (dealer != base.Owner)`
  - `Models/Powers/WeakPower.cs:31` **flow / ModifyDamageMultiplicative** `if (!props.IsPoweredAttack())`
  - `Models/Powers/WeakPower.cs:37` **flow / ModifyDamageMultiplicative** `if (paperKrane != null)`
  - `Models/Powers/WeakPower.cs:42` **flow / ModifyDamageMultiplicative** `if (power != null)`
  - `Models/Powers/WeakPower.cs:51` **flow / AfterSideTurnEnd** `if (side == CombatSide.Enemy)`

#### `POWER.WELL_LAID_PLANS_POWER`

- 类别/可达来源：`generated/reference from CARD.WELL_LAID_PLANS`；源：`Models/Powers/WellLaidPlansPower.cs`；C++：`game/Models/Powers/WellLaidPlansPower.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `AbstractModel.ShouldFlush`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（1）：
  - `Models/Powers/WellLaidPlansPower.cs:14` **flow / ShouldFlush** `if (player != base.Owner.Player)`

#### `POWER.WRAITH_FORM_POWER`

- 类别/可达来源：`generated/reference from CARD.WRAITH_FORM`；源：`Models/Powers/WraithFormPower.cs`；C++：`game/Models/Powers/WraithFormPower.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `AbstractModel.AfterSideTurnStart`
  - `PowerModel.get_ExtraHoverTips`
  - `PowerModel.get_StackType`
  - `PowerModel.get_Type`
- 生成/模型依赖：`POWER.DEXTERITY_POWER`
- 源码分支/控制流检查点（1）：
  - `Models/Powers/WraithFormPower.cs:23` **flow / AfterSideTurnStart** `if (participants.Contains(base.Owner))`

### RelicModel（9）

#### `RELIC.HELICAL_DART`

- 类别/可达来源：`relic pool`；源：`Models/Relics/HelicalDart.cs`；C++：`game/Models/Relics/HelicalDart.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `AbstractModel.AfterCardPlayed`
  - `RelicModel.get_CanonicalVars`
  - `RelicModel.get_ExtraHoverTips`
  - `RelicModel.get_Rarity`
- 生成/模型依赖：`CARD.SHIV`, `POWER.DEXTERITY_POWER`, `POWER.HELICAL_DART_POWER`
- 源码分支/控制流检查点（2）：
  - `Models/Relics/HelicalDart.cs:25` **flow / AfterCardPlayed** `if (cardPlay.Card.Owner == base.Owner && cardPlay.Card.Tags.Contains(CardTag.Shiv))`
  - `Models/Relics/HelicalDart.cs:25` **short-circuit / AfterCardPlayed** `if (cardPlay.Card.Owner == base.Owner && cardPlay.Card.Tags.Contains(CardTag.Shiv))`

#### `RELIC.NINJA_SCROLL`

- 类别/可达来源：`relic pool`；源：`Models/Relics/NinjaScroll.cs`；C++：`game/Models/Relics/NinjaScroll.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `AbstractModel.BeforeHandDraw`
  - `RelicModel.get_CanonicalVars`
  - `RelicModel.get_ExtraHoverTips`
  - `RelicModel.get_Rarity`
- 生成/模型依赖：`CARD.SHIV`
- 源码分支/控制流检查点（2）：
  - `Models/Relics/NinjaScroll.cs:25` **flow / BeforeHandDraw** `if (player == base.Owner && base.Owner.PlayerCombatState.TurnNumber <= 1)`
  - `Models/Relics/NinjaScroll.cs:25` **short-circuit / BeforeHandDraw** `if (player == base.Owner && base.Owner.PlayerCombatState.TurnNumber <= 1)`

#### `RELIC.PAPER_KRANE`

- 类别/可达来源：`relic pool`；源：`Models/Relics/PaperKrane.cs`；C++：`game/Models/Relics/PaperKrane.h`（header present; `IsUsable`=yes）
- C# override 清单（2）：
  - `RelicModel.get_ExtraHoverTips`
  - `RelicModel.get_Rarity`
- 生成/模型依赖：`POWER.WEAK_POWER`
- 源码分支/控制流检查点（2）：
  - `Models/Relics/PaperKrane.cs:18` **flow / ModifyWeakMultiplier** `if (target != base.Owner.Creature)`
  - `Models/Relics/PaperKrane.cs:22` **flow / ModifyWeakMultiplier** `if (!props.IsPoweredAttack())`

#### `RELIC.PAPER_PHROG`

- 类别/可达来源：`generated/reference from POWER.VULNERABLE_POWER`；源：`Models/Relics/PaperPhrog.cs`；C++：`game/Models/Relics/PaperPhrog.h`（header present; `IsUsable`=yes）
- C# override 清单（2）：
  - `RelicModel.get_ExtraHoverTips`
  - `RelicModel.get_Rarity`
- 生成/模型依赖：`POWER.VULNERABLE_POWER`
- 源码分支/控制流检查点（2）：
  - `Models/Relics/PaperPhrog.cs:18` **flow / ModifyVulnerableMultiplier** `if (target == base.Owner.Creature)`
  - `Models/Relics/PaperPhrog.cs:22` **flow / ModifyVulnerableMultiplier** `if (!props.IsPoweredAttack())`

#### `RELIC.RING_OF_THE_SNAKE`

- 类别/可达来源：`relic pool`；源：`Models/Relics/RingOfTheSnake.cs`；C++：`game/Models/Relics/RingOfTheSnake.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `AbstractModel.ModifyHandDraw`
  - `RelicModel.get_CanonicalVars`
  - `RelicModel.get_Rarity`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（2）：
  - `Models/Relics/RingOfTheSnake.cs:16` **flow / ModifyHandDraw** `if (player != base.Owner)`
  - `Models/Relics/RingOfTheSnake.cs:20` **flow / ModifyHandDraw** `if (base.Owner.PlayerCombatState.TurnNumber > 1)`

#### `RELIC.SNECKO_SKULL`

- 类别/可达来源：`relic pool`；源：`Models/Relics/SneckoSkull.cs`；C++：`game/Models/Relics/SneckoSkull.h`（header present; `IsUsable`=yes）
- C# override 清单（5）：
  - `AbstractModel.AfterModifyingPowerAmountGiven`
  - `AbstractModel.ModifyPowerAmountGivenAdditive`
  - `RelicModel.get_CanonicalVars`
  - `RelicModel.get_ExtraHoverTips`
  - `RelicModel.get_Rarity`
- 生成/模型依赖：`POWER.POISON_POWER`
- 源码分支/控制流检查点（2）：
  - `Models/Relics/SneckoSkull.cs:21` **flow / ModifyPowerAmountGivenAdditive** `if (!(power is PoisonPower))`
  - `Models/Relics/SneckoSkull.cs:25` **flow / ModifyPowerAmountGivenAdditive** `if (giver != base.Owner.Creature)`

#### `RELIC.TINGSHA`

- 类别/可达来源：`relic pool`；源：`Models/Relics/Tingsha.cs`；C++：`game/Models/Relics/Tingsha.h`（header present; `IsUsable`=yes）
- C# override 清单（3）：
  - `AbstractModel.AfterCardDiscarded`
  - `RelicModel.get_CanonicalVars`
  - `RelicModel.get_Rarity`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（3）：
  - `Models/Relics/Tingsha.cs:20` **flow / AfterCardDiscarded** `if (card.Owner == base.Owner && base.Owner.Creature.Side == base.Owner.Creature.CombatState.CurrentSide)`
  - `Models/Relics/Tingsha.cs:20` **short-circuit / AfterCardDiscarded** `if (card.Owner == base.Owner && base.Owner.Creature.Side == base.Owner.Creature.CombatState.CurrentSide)`
  - `Models/Relics/Tingsha.cs:24` **flow / AfterCardDiscarded** `if (creature != null)`

#### `RELIC.TOUGH_BANDAGES`

- 类别/可达来源：`relic pool`；源：`Models/Relics/ToughBandages.cs`；C++：`game/Models/Relics/ToughBandages.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `AbstractModel.AfterCardDiscarded`
  - `RelicModel.get_CanonicalVars`
  - `RelicModel.get_ExtraHoverTips`
  - `RelicModel.get_Rarity`
- 生成/模型依赖：（无已识别的模型类型引用）
- 源码分支/控制流检查点（2）：
  - `Models/Relics/ToughBandages.cs:22` **flow / AfterCardDiscarded** `if (card.Owner == base.Owner && base.Owner.Creature.Side == base.Owner.Creature.CombatState.CurrentSide)`
  - `Models/Relics/ToughBandages.cs:22` **short-circuit / AfterCardDiscarded** `if (card.Owner == base.Owner && base.Owner.Creature.Side == base.Owner.Creature.CombatState.CurrentSide)`

#### `RELIC.TWISTED_FUNNEL`

- 类别/可达来源：`relic pool`；源：`Models/Relics/TwistedFunnel.cs`；C++：`game/Models/Relics/TwistedFunnel.h`（header present; `IsUsable`=yes）
- C# override 清单（4）：
  - `AbstractModel.BeforeSideTurnStart`
  - `RelicModel.get_CanonicalVars`
  - `RelicModel.get_ExtraHoverTips`
  - `RelicModel.get_Rarity`
- 生成/模型依赖：`POWER.POISON_POWER`
- 源码分支/控制流检查点（4）：
  - `Models/Relics/TwistedFunnel.cs:28` **flow / BeforeSideTurnStart** `if (!participants.Contains(base.Owner.Creature) \|\| base.Owner.PlayerCombatState.TurnNumber > 1)`
  - `Models/Relics/TwistedFunnel.cs:28` **short-circuit / BeforeSideTurnStart** `if (!participants.Contains(base.Owner.Creature) \|\| base.Owner.PlayerCombatState.TurnNumber > 1)`
  - `Models/Relics/TwistedFunnel.cs:33` **flow / BeforeSideTurnStart** `foreach (Creature hittableEnemy in base.Owner.Creature.CombatState.HittableEnemies)`
  - `Models/Relics/TwistedFunnel.cs:38` **flow / BeforeSideTurnStart** `foreach (Creature hittableEnemy2 in base.Owner.Creature.CombatState.HittableEnemies)`

### RelicPoolModel（1）

#### `RELIC_POOL.SILENT_RELIC_POOL`

- 类别/可达来源：`relic pool`；源：`Models/RelicPools/SilentRelicPool.cs`；C++：`content tables / generic pool queries`（content tables / generic pool queries; `IsUsable`=N/A）
- C# override 清单（4）：
  - `RelicPoolModel.GenerateAllRelics`
  - `RelicPoolModel.GetUnlockedRelics`
  - `RelicPoolModel.get_EnergyColorName`
  - `RelicPoolModel.get_LabOutlineColor`
- 生成/模型依赖：`RELIC.HELICAL_DART`, `RELIC.NINJA_SCROLL`, `RELIC.PAPER_KRANE`, `RELIC.RING_OF_THE_SNAKE`, `RELIC.SNECKO_SKULL`, `RELIC.TINGSHA`, `RELIC.TOUGH_BANDAGES`, `RELIC.TWISTED_FUNNEL`
- 源码分支/控制流检查点（2）：
  - `Models/RelicPools/SilentRelicPool.cs:35` **flow / GetUnlockedRelics** `if (!unlockState.IsEpochRevealed<Silent3Epoch>())`
  - `Models/RelicPools/SilentRelicPool.cs:39` **flow / GetUnlockedRelics** `if (!unlockState.IsEpochRevealed<Silent6Epoch>())`

## 缺失的 C# 源文件

无。

## 未解析的源码或依赖映射

无。

## 计数与口径

- 闭包模型：154。
- 缺具体 C++ 模型头文件（不含表驱动的池模型）：0。
- 缺 `IsUsable`（不含表驱动的池模型）：0。
- 有明显 Unported stub 标记的现存头文件：0。
- 源码控制流检查点（含继承 override）：314。
- 依赖闭包由已知模型类型的泛型参数与静态 `Create*` 引用构造；源审计还要核查非泛型工厂与运行时 Hook。
- CardPoolModel/RelicPoolModel/PotionPoolModel 在 Env 中无独立具体类；其池成员通过导出的内容表，需逐项核对全解锁路径与池源过滤条件。
