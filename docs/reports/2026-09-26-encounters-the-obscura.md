# The Obscura Normal encounter 移植报告

日期：2026-09-26  
分支：`feat/all-encounters-comparison-mod`  
游戏基线：v0.111.0；真值为反编译源码与 `Sts2Oracle`。

## 前置与范围

本批增加 The Obscura Normal 遭遇、主怪 The Obscura 和它召出的 Parafright。IllusionPower/MinionPower、动态怪物加入战斗和槽位分配复用模拟器已有实现。

## 结果

已接入 `THE_OBSCURA_NORMAL`。The Obscura 的首招在 `illusion` 槽召唤 Parafright；之后从 Piercing Gaze、Sail、Hardening Strike 三招中随机选择且不连续重复。Sail 同时给存活队友增加 3 点力量，Hardening Strike 先攻击再获得格挡。

Parafright 固定 21 HP、循环 Slam；共享 IllusionPower 增加 MinionPower，并在死亡后进入 `REVIVE_MOVE`，按原版恢复满 HP，再回到最后执行的招式。此处新增 revive 状态是对现有共享能力的适配，L3 覆盖击杀幻象后的复活。

正式遭遇覆盖提升至 **78/90**，怪物模型头文件提升至 **88/126**，尚缺 12 个遭遇和 38 个怪物模型头文件。

## Oracle 对拍与检查

| 配置 | 战斗 / 状态 | 结果 |
|---|---:|---|
| A0，种子 `OBSCR001,OBSCR002`，策略种子 `1,2` | 4/4 · 78/78 | 一致 |
| A10，种子 `OBSCR010`，策略种子 `1,2` | 2/2 · 32/32 | 一致 |

验证命令：

```bash
python3 tools/gen/gen_dispatch.py
cmake --build build -j4 --target sts2sim_cli
DOTNET=/home/wst/.dotnet/dotnet python3 tests/l3/l3_combat.py --seeds OBSCR001,OBSCR002 --characters IRONCLAD --policy-seeds 1,2 --ascension 0 --encounter THE_OBSCURA_NORMAL -v
DOTNET=/home/wst/.dotnet/dotnet python3 tests/l3/l3_combat.py --seeds OBSCR010 --characters IRONCLAD --policy-seeds 1,2 --ascension 10 --encounter THE_OBSCURA_NORMAL -v
scripts/check.sh --quick
```

L3 结果：A0 `4/4`、`78/78`；A10 `2/2`、`32/32`；0 unported、0 diverged。`scripts/check.sh --quick` 通过：构建成功、CTest 8/8、整局 smoke 通过（`death=94 unported=6`、`death=100`、`boss=3 death=97`）。
