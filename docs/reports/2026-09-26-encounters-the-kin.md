# The Kin Boss 遭遇移植报告

日期：2026-09-26  
分支：`feat/all-encounters-comparison-mod`  
游戏基线：v0.111.0；真值为反编译源码与 `Sts2Oracle`。

## 前置与范围

本批增加 The Kin Boss 的三名敌人：两名 Kin Follower 和一名 Kin Priest。Follower 的皮肤、说话和音效等表现逻辑不进入模拟状态。

## 结果

已接入 `THE_KIN_BOSS`：slot1 Kin Follower、slot2 Kin Follower、leaderSlot Kin Priest。两只 Follower 都获得 MinionPower，slot1 Follower 开场执行 Power Dance，另一只开场执行 Quick Slash。

Follower 循环为 Quick Slash → 双击 Boomerang → Power Dance；Priest 循环为造成伤害并施加 Frail → 造成伤害并施加 Weak → 三段 Beam → 增加自身力量。按 A0/A10 保留攻击、生命值和力量参数。

正式遭遇覆盖提升至 **80/90**，怪物模型头文件提升至 **91/126**，尚缺 10 个遭遇和 35 个怪物模型头文件。Follower 死亡后的台词和音乐 Hook 仅影响表现，在模拟状态中不保存。

## Oracle 对拍与检查

| 配置 | 战斗 / 状态 | 结果 |
|---|---:|---|
| A0，种子 `THEKIN001,THEKIN002`，策略种子 `1,2` | 4/4 · 77/77 | 一致 |
| A10，种子 `THEKIN010`，策略种子 `1,2` | 2/2 · 35/35 | 一致 |

验证命令：

```bash
python3 tools/gen/gen_dispatch.py
cmake --build build -j4 --target sts2sim_cli
DOTNET=/home/wst/.dotnet/dotnet python3 tests/l3/l3_combat.py --seeds THEKIN001,THEKIN002 --characters IRONCLAD --policy-seeds 1,2 --ascension 0 --encounter THE_KIN_BOSS -v
DOTNET=/home/wst/.dotnet/dotnet python3 tests/l3/l3_combat.py --seeds THEKIN010 --characters IRONCLAD --policy-seeds 1,2 --ascension 10 --encounter THE_KIN_BOSS -v
scripts/check.sh --quick
```

L3 结果：A0 `4/4`、`77/77`；A10 `2/2`、`35/35`；0 unported、0 diverged。`scripts/check.sh --quick` 通过：构建成功、CTest 8/8、整局 smoke 通过（`death=94 unported=6`、`death=100`、`boss=3 death=97`）。
