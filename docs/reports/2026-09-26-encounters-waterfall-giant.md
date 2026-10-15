# Waterfall Giant Boss 遭遇移植报告

日期：2026-09-26  
分支：`feat/all-encounters-comparison-mod`  
游戏基线：v0.111.0；真值为反编译源码与 `Sts2Oracle`。

## 前置与范围

本批目标是 Waterfall Giant Boss，覆盖其蒸汽积累、递增的 Pressure Gun 伤害，以及受到致命伤害后恢复并延迟自爆的战斗机制。

## 结果

已接入 `WATERFALL_GIANT_BOSS`。Waterfall Giant 具有 Pressurize → Stomp → Ram → Siphon → Pressure Gun → Pressure Up 循环。压力枪每次加 5 点伤害；其余循环招式按原版增加 Steam Eruption，Stomp 施加 1 层虚弱，Siphon 回复 A0/A10 相应生命值。

SteamEruptionPower 会阻止战斗在巨人首次死亡时结束，并拦截移除：巨人恢复为原版指定的大生命值，进入 About To Blow 蓄爆状态，随后用已缓存的蒸汽层数攻击并再次死亡。音乐、环境音和动画不进入无头模拟状态。

正式遭遇覆盖提升至 **79/90**，怪物模型头文件提升至 **89/126**，尚缺 11 个遭遇和 37 个怪物模型头文件。

## Oracle 对拍与检查

| 配置 | 战斗 / 状态 | 结果 |
|---|---:|---|
| A0，种子 `WFGIANT1,WFGIANT2`，策略种子 `1,2` | 4/4 · 132/132 | 一致 |
| A10，种子 `WFGIANT10`，策略种子 `1,2` | 2/2 · 67/67 | 一致 |

验证命令：

```bash
python3 tools/gen/gen_dispatch.py
cmake --build build -j4 --target sts2sim_cli
DOTNET=/home/wst/.dotnet/dotnet python3 tests/l3/l3_combat.py --seeds WFGIANT1,WFGIANT2 --characters IRONCLAD --policy-seeds 1,2 --ascension 0 --encounter WATERFALL_GIANT_BOSS -v
DOTNET=/home/wst/.dotnet/dotnet python3 tests/l3/l3_combat.py --seeds WFGIANT10 --characters IRONCLAD --policy-seeds 1,2 --ascension 10 --encounter WATERFALL_GIANT_BOSS -v
scripts/check.sh --quick
```

L3 结果：A0 `4/4`、`132/132`；A10 `2/2`、`67/67`；0 unported、0 diverged。`scripts/check.sh --quick` 通过：构建成功、CTest 8/8、整局 smoke 通过（`death=94 unported=6`、`death=100`、`boss=3 death=97`）。
