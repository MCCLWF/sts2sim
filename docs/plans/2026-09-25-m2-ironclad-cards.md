# 计划：铁甲战士卡牌（M2）

执行者：Codex（luna）。分支：`feat/ironclad-cards`（基于 `feat/run-skeleton`）。审阅与合入：Claude。

## 背景

- 铁甲战士卡池 90 张，已移植 3 张基础牌（`STRIKE_IRONCLAD`、`DEFEND_IRONCLAD`、`BASH`）。整局冒烟和 L4 里停下的原因大多是 `Cards.X.OnPlay is not ported`。
- L3 现在支持自定义牌组、额外遗物和战斗内选牌（`docs/reports/2026-09-25-l3-custom-decks.md`）：
  ```bash
  python3 tests/l3/l3_combat.py --seeds $SEEDS --policy-seeds 1,2 --ascension 0 \
      --encounter SLIMES_WEAK --deck ANGER,ANGER+1,ANGER,STRIKE_IRONCLAD,DEFEND_IRONCLAD,BASH
  ```
- 已有的框架：`DamageCmd`（攻击管线）、`CreatureCmd`、`PowerCmd`、`CardPileCmd`（抽牌、加入战斗、洗牌）、`CardCmd`（打出、弃牌、消耗、升级、变牌、苦难、关键字）、`CardSelectCmd`（手牌/牌堆/牌组选牌）、`PlayerCmd`（能量、金币）。样板：`game/Models/Cards/{StrikeIronclad,DefendIronclad,Bash,Slimed}.h`、`game/Models/Powers/*.h`。

## 顺序

1. 普通（20）：`ANGER ARMAMENTS BLOOD_WALL BODY_SLAM BREAKTHROUGH CINDER HAVOC HEADBUTT IRON_WAVE MOLTEN_FIST PERFECTED_STRIKE POMMEL_STRIKE SETUP_STRIKE SHRUG_IT_OFF SWORD_BOOMERANG TAUNT THUNDERCLAP TREMBLE TRUE_GRIT TWIN_STRIKE`
2. 罕见（38）：`ASHEN_STRIKE BATTLE_TRANCE BLAZE BLOODLETTING BLUDGEON BULLY BURNING_PACT COLOSSUS CRUELTY DEMONIC_SHIELD DISMANTLE DRUM_OF_BATTLE EVIL_EYE EXPECT_A_FIGHT FEEL_NO_PAIN FIGHT_ME FLAME_BARRIER FORGOTTEN_RITUAL HEMOKINESIS HOWL_FROM_BEYOND INFERNAL_BLADE INFERNO INFLAME JUGGLING OUTRAGE PILLAGE RAGE RAMPAGE RUPTURE SECOND_WIND SPITE STAMPEDE STOMP STONE_ARMOR UNRELENTING UPPERCUT VICIOUS WHIRLWIND`
3. 稀有（27）与先古（2）：`AGGRESSION BARRICADE BRAND CASCADE CONFLAGRATION CRIMSON_MANTLE DARK_EMBRACE DEMON_FORM DOMINATE FEED FIEND_FIRE HELLRAISER IMPERVIOUS JUGGERNAUT MANGLE MIDNIGHT NOT_YET OFFERING ONE_TWO_PUNCH PACTS_END PRIMAL_FORCE PYRE STOKE TANK TEAR_ASUNDER THRASH UNMOVABLE BREAK CORRUPTION`

## 每张卡的步骤

1. 读 `$STS2_SRC/Models/Cards/<类名>.cs` 及它用到的能力（`Models/Powers/*.cs`）、它生成的卡牌。
2. 逐行移植到 `game/Models/Cards/<类名>.h`（和需要的 `Powers/*.h`）：所有 C# 覆写的成员；`OnUpgrade` 只改表里数值的不用写（生成器自动处理，见 `tools/gen/gen_dispatch.py` 的 delta-only 规则）。动画、音效、VFX 删掉，其余一行不少。
3. `python3 tools/gen/gen_dispatch.py`，构建。
4. 对拍：牌组里放 3～4 张这张卡（含升级版）加少量起始牌，3 个已移植遭遇（例如 `SLIMES_WEAK`、`NIBBITS_WEAK`、`CULTISTS_NORMAL`；需要多个敌人的卡用多敌人遭遇），A0 和 A10 各 8 个种子 × 2 个策略：
   ```bash
   SEEDS=$(python3 -c "print(','.join('CARD%03d'%i for i in range(8)))")
   python3 tests/l3/l3_combat.py --seeds $SEEDS --policy-seeds 1,2 --ascension 0 --encounter <遭遇> --deck <牌组>
   ```
   必须全部 identical。依赖其他卡的效果（例如 `HAVOC` 打出牌堆顶的牌）可以把相关卡一起放进牌组。
5. 每 3～5 张卡提交一次：`feat: port Ironclad cards <类名>…`。

## 规则

- 先读 `AGENTS.md`、`docs/design-engine.md`、`docs/audits/pitfalls.md`。
- 对拍分歧查模拟器，不改参照、不放宽比对、不补抽随机数。
- 缺一个简单的、C# 里有的重载或方法，逐行补上，在报告里单列（文件、方法、C# 位置）。需要大机制（充能球、附魔、召唤友方、新的选择界面）或不确定：跳过这张卡，报告里写明卡在哪个 C# 方法上。
- 不改 `tools/Sts2Oracle/`、`tests/`、`scripts/`。不提交构建产物、游戏文件、反编译源码、本地化文本。

## 交付

- 报告 `docs/reports/2026-09-25-ironclad-cards.md`：卡牌结果表（卡、新增的能力/卡、对拍用的遭遇与牌组、结果）、跳过的卡及原因、框架改动、最后一次完整对拍的命令和原始摘要输出。结论如实。
- `scripts/check.sh` 全部通过。
