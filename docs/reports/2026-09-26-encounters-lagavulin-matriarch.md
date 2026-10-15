# Lagavulin Matriarch Boss 遭遇移植报告

日期：2026-09-26  
分支：`feat/all-encounters-comparison-mod`  
游戏基线：v0.111.0；真值为反编译源码与 `Sts2Oracle`。

## 结果

已接入 `LAGAVULIN_MATRIARCH_BOSS` 与 Lagavulin Matriarch。战斗开始时依次施加 12 层 Plating 和 3 层 Asleep。Asleep 按原版在受到未格挡伤害时移除 Plating、设置一次 WakeUp stun 后清除；若未受击，则在拥有者参与的回合结束时递减，并于最后一层前移除 Plating、计时归零后唤醒。

唤醒后招式为 Slash → Disembowel（2 段）→ Slash2（攻击并获得格挡）→ Soul Siphon（玩家力量/敏捷各 -2，Matriarch 力量 +2）→ Slash。初始睡眠分支持续检查 AsleepPower。

受伤动画状态和死亡时停止睡眠特效是纯表现 Hook，在无头模拟中为空实现。

正式遭遇覆盖提升至 **75/90**，怪物模型头文件提升至 **83/126**，尚缺 15 个遭遇和 43 个怪物模型头文件。

## Oracle 对拍

| 配置 | 战斗 / 状态 | 结果 |
|---|---:|---|
| A0，种子 `LAGAMAT01,LAGAMAT02`，策略种子 `1,2` | 4/4 · 115/115 | 一致 |
| A10，种子 `LAGAMAT10`，策略种子 `1,2` | 2/2 · 53/53 | 一致 |
| A0，20 张 Defend、延长生存遗物，种子 `LAGASLEEP`，策略种子 `1,2` | 2/2 · 79/79 | 一致；验证三回合计时唤醒 |

默认牌组覆盖受击唤醒和 WakeUp stun；纯防御牌组不造成伤害，覆盖 Asleep 倒数、末层 Plating 移除和计时唤醒。

## 验证命令

```bash
python3 tools/gen/gen_dispatch.py
cmake --build build -j4 --target sts2sim_cli
DOTNET=/home/wst/.dotnet/dotnet python3 tests/l3/l3_combat.py --seeds LAGAMAT01,LAGAMAT02 --characters IRONCLAD --policy-seeds 1,2 --ascension 0 --encounter LAGAVULIN_MATRIARCH_BOSS -v
DOTNET=/home/wst/.dotnet/dotnet python3 tests/l3/l3_combat.py --seeds LAGAMAT10 --characters IRONCLAD --policy-seeds 1,2 --ascension 10 --encounter LAGAVULIN_MATRIARCH_BOSS -v
DOTNET=/home/wst/.dotnet/dotnet python3 tests/l3/l3_combat.py --seeds LAGASLEEP --characters IRONCLAD --policy-seeds 1,2 --ascension 0 --encounter LAGAVULIN_MATRIARCH_BOSS --deck 'DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD,DEFEND_IRONCLAD' --relics 'ANCHOR,ORICHALCUM,MANGO,CAPTAINS_WHEEL' -v
scripts/check.sh --quick
```

L3 结果：A0 `4/4`、`115/115`；A10 `2/2`、`53/53`；计时唤醒 `2/2`、`79/79`；0 unported、0 diverged。`scripts/check.sh --quick` 通过：8/8 CTest 和三组整局冒烟通过（`death=94 unported=6`、`death=100`、`boss=3 death=97`）。完整 L2/L3/L4 门禁未运行。
