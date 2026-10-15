# Vantom Boss 遭遇移植报告

日期：2026-09-26  
分支：`feat/all-encounters-comparison-mod`  
游戏基线：v0.111.0；真值为反编译源码与 `Sts2Oracle`。

## 结果

已接入 `VANTOM_BOSS` 与单体 Vantom。保留 A0/A10 生命值、ToughEnemies 对应 8/9 层 Slippery、DeadlyEnemies 对应的攻击值，以及 `ShouldDisappearFromDoom = false`。

招式循环按原版为 Ink Blot → Inky Lance（2 段）→ Dismember（攻击并加入 3 张 Wound 到弃牌堆）→ Prepare（获得 2 力量）→ Ink Blot。为覆盖 Dismember，补入通用不可打出状态牌 Wound。

Vantom 死亡后更新 Boss 音乐参数的 Hook 仅影响表现，因此在无头模拟中为空实现；动画、音效同样不参与战斗状态。

正式遭遇覆盖提升至 **74/90**，怪物模型头文件提升至 **82/126**，尚缺 16 个遭遇和 44 个怪物模型头文件。

## Oracle 对拍

| 配置 | 战斗 / 状态 | 结果 |
|---|---:|---|
| A0，种子 `VANTOMA01,VANTOMA02`，策略种子 `1,2` | 4/4 · 114/114 | 一致 |
| A10，种子 `VANTOMA10`，策略种子 `1,2` | 2/2 · 44/44 | 一致 |

## 验证命令

```bash
python3 tools/gen/gen_dispatch.py
cmake --build build -j4 --target sts2sim_cli
DOTNET=/home/wst/.dotnet/dotnet python3 tests/l3/l3_combat.py --seeds VANTOMA01,VANTOMA02 --characters IRONCLAD --policy-seeds 1,2 --ascension 0 --encounter VANTOM_BOSS -v
DOTNET=/home/wst/.dotnet/dotnet python3 tests/l3/l3_combat.py --seeds VANTOMA10 --characters IRONCLAD --policy-seeds 1,2 --ascension 10 --encounter VANTOM_BOSS -v
scripts/check.sh --quick
```

L3 结果：A0 `4/4`、`114/114`；A10 `2/2`、`44/44`；0 unported、0 diverged。`scripts/check.sh --quick` 通过：8/8 CTest 和三组整局冒烟通过（`death=94 unported=6`、`death=100`、`boss=3 death=97`）。完整 L2/L3/L4 门禁未运行。
