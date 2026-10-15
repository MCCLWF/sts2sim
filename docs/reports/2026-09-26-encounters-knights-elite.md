# Knights Elite 遭遇移植报告

日期：2026-09-26  
分支：`feat/all-encounters-comparison-mod`  
游戏基线：v0.111.0；真值为反编译源码与 `Sts2Oracle`。

## 结果

已移植 `KNIGHTS_ELITE` 固定三人组：Flail Knight、Spectral Knight、Magi Knight。新增 Magi/Spectral 的回合招式与 A0/A10 数值，HexPower/Hexed 使玩家牌获得 Ethereal 并在施法者死亡后清除，Dampen 会暂时将已升级战斗牌降到基础等级，并在最后一个施法者死亡后恢复原升级等级。

Dampen 的恢复测试使用升级后的 Rampage。对拍期间，Rampage 在 Dampen 生效时以基础牌面打出；击杀 Magi 后牌恢复为 `RAMPAGE+1`。测试同时覆盖了 Rampage 的成长值经过降级后仍能保留。

此测试还发现并修正一个通用升级错误：导出的升级表已包含 22 个纯表驱动 `OnUpgrade` 覆写的增量，模拟器再次调用这些钩子会重复增加数值。生成分发现已跳过这 22 个纯表驱动覆写；非表驱动的 `OnUpgrade` 仍走模型分发。

正式遭遇覆盖提升至 **72/90**，怪物模型头文件提升至 **80/126**，尚缺 18 个遭遇和 46 个怪物模型头文件。

## Oracle 对拍

默认 Ironclad 牌组：

| 配置 | 战斗 / 状态 | 结果 |
|---|---:|---|
| A0，种子 `KNIGHTA01,KNIGHTA02`，策略种子 `1,2` | 4/4 · 52/52 | 一致 |
| A10，种子 `KNIGHTA10`，策略种子 `1,2` | 2/2 · 30/30 | 一致 |

升级牌和 Dampen 恢复路径：

| 配置 | 战斗 / 状态 | 结果 |
|---|---:|---|
| A0，升级版 Impervious/Bludgeon，策略种子 `1,4,10,14` | 4/4 · 46/46 | 一致；验证 Dampen 生效与施法者死亡后的升级恢复 |
| A0，`IMPERVIOUS+1` 与四张 `RAMPAGE+1`，策略种子 `3,5` | 2/2 · 32/32 | 一致；验证成长中的 Rampage 降级、打出和恢复 |

升级牌对拍使用 `ANCHOR,ORICHALCUM,MANGO,CAPTAINS_WHEEL` 延长战斗，使 Dampen 施加与移除路径都实际发生。

## 验证命令

```bash
cmake --build build -j4 --target test_env sts2env_c sts2sim_env_solver sts2sim_cli
DOTNET=/home/wst/.dotnet/dotnet python3 tests/l3/l3_combat.py --seeds KNIGHTA01,KNIGHTA02 --characters IRONCLAD --policy-seeds 1,2 --ascension 0 --encounter KNIGHTS_ELITE -v
DOTNET=/home/wst/.dotnet/dotnet python3 tests/l3/l3_combat.py --seeds KNIGHTA10 --characters IRONCLAD --policy-seeds 1,2 --ascension 10 --encounter KNIGHTS_ELITE -v
DOTNET=/home/wst/.dotnet/dotnet python3 tests/l3/l3_combat.py --seeds KNIGHTDAMPEN2 --characters IRONCLAD --policy-seeds 1,4,10,14 --ascension 0 --encounter KNIGHTS_ELITE --deck 'IMPERVIOUS+1,IMPERVIOUS+1,IMPERVIOUS+1,IMPERVIOUS+1,IMPERVIOUS+1,IMPERVIOUS+1,IMPERVIOUS+1,IMPERVIOUS+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1,BLUDGEON+1' --relics 'ANCHOR,ORICHALCUM,MANGO,CAPTAINS_WHEEL' -v
DOTNET=/home/wst/.dotnet/dotnet python3 tests/l3/l3_combat.py --seeds KNIGHTSTATEFUL --characters IRONCLAD --policy-seeds 3,5 --ascension 0 --encounter KNIGHTS_ELITE --deck 'IMPERVIOUS+1,RAMPAGE+1,RAMPAGE+1,RAMPAGE+1,RAMPAGE+1' --relics 'ANCHOR,ORICHALCUM,MANGO,CAPTAINS_WHEEL' -v
scripts/check.sh --quick
```

结果：A0 `4/4`, `52/52`；A10 `2/2`, `30/30`；升级牌恢复 `4/4`, `46/46`；Rampage 成长场景 `2/2`, `32/32`。Quick gate：8/8 单元测试通过，三组全局冒烟通过（`death=94 unported=6`、`death=100`、`boss=3 death=97`）。未运行完整 L2/L3/L4 门禁。

## 范围

对拍和本批实现限于 Ironclad 卡牌内容；其他角色牌仍按用户要求暂不扩展。Oracle 场景没有加入 Steady 附魔，降级时该已实现附魔会重新加回 Retain，但本次未单独对拍附魔组合。
