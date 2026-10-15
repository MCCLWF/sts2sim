# Fake Merchant run 路径与假遗物

日期：2026-09-28。分支：`feat/generic-env-rf-api`。游戏 v0.111.0。此批专注通用 Env 的 run 级内容，CombatSolver 和其它角色任务未修改。

## 实现

- Fake Merchant 的六件库存按原版九件候选用事件 RNG 洗牌后取前六件，并逐件由玩家 Shops RNG 定价。沿用 POD `MerchantState`，将遗物库存容量扩为六。自定义事件无普通选项；Env 通过事件房间的商店决策暴露购买与离开。
- Foul Potion 在 Fake Merchant 房间可以使用；投掷后开启原版事件遭遇，战斗额外奖励包括 Fake Merchant's Rug 和五件未售出遗物。已购买的遗物不再进入战利品。
- 独立移植十件 Fake 遗物及其依赖的 ConfusedPower，保留原版相对于同名真遗物的数值和触发时机。
- 增加原版 `fake-merchant` 库存探针；探针直接调用游戏的 `FakeMerchant.BeginEvent`，读取六件库存、价格与两个 RNG 计数。`tests/l4/fake_merchant_inventory.py` 将其与本地 C++ 状态逐项比较。

## 验收记录

| 命令 | 结果 |
|---|---|
| `./build/test_fake_merchant` | 通过：库存、购买、Foul Potion 转战、地毯及未售遗物奖励、Fake Mango、Fake Lee's Waffle、Fake Venerable Tea Set |
| `DOTNET=/home/wst/.dotnet/dotnet python3 tests/l4/fake_merchant_inventory.py` | 8/8 个独立种子库存 ID、价格和 RNG 计数完全一致 |
| `DOTNET=/home/wst/.dotnet/dotnet python3 tests/l3/l3_combat.py --seeds FAKE01,FAKE02 --characters IRONCLAD --policy-seeds 1,2 --encounter CULTISTS_NORMAL --relics FAKE_ANCHOR,FAKE_BLOOD_VIAL,FAKE_HAPPY_FLOWER,FAKE_LEES_WAFFLE,FAKE_MANGO,FAKE_MERCHANTS_RUG,FAKE_ORICHALCUM,FAKE_SNECKO_EYE,FAKE_STRIKE_DUMMY,FAKE_VENERABLE_TEA_SET --max-actions 120 --verbose` | 4/4 战斗、78/78 状态一致；0 Unported、0 分歧 |
| `python3 tools/coverage/map_source_inventory.py --check docs/audits/map-source-inventory.json` | 通过 |
| `scripts/check.sh` | 通过：21/21 CTest；L2 200 局；L3 80/80 战、1611/1611 状态；默认 L4 80/80 局、4089/4089 决策；全部定向路线零分歧、零 Unported |
| `git diff --check` | 通过，无输出 |

## 仍需验证的范围

Foul Potion 到 Fake Merchant 事件战斗再到额外战利品的整个原版 UI/整局路径尚无直接 oracle 差分；本批仅有源码对照及 C++ 状态转换测试。`NFakeMerchant` 的欢迎/投掷对白、视觉、按键启停，以及未购商品的地图点历史是展示层，Env 尚不输出这些事实。源代码的 `FakeSneckoEye`/`ConfusedPower` TestMode 专用费用覆写不进入普通游戏 Env；普通抽牌费用 RNG 已由四场战斗差分验证。8 个库存种子与 4 场战斗都不能代表全部事件路径覆盖。
