# 无色牌批次 9：四张战斗牌

## 目标

继续补齐单人可用的无色牌，移植 Gold Axe、Rend、Seeker Strike 和 Volley，并复用或补足其需要的战斗历史、临时 Power 分类、抽牌堆选择和 X 值计算。

## 工作项

- Gold Axe：以本场已完成出牌数计算伤害，升级获得保留。
- Rend：按目标身上非临时减益数量计算伤害，并实现升级后的两项数值变化。
- Seeker Strike：造成伤害后，从抽牌堆中按源码 RNG 顺序随机展示最多三张，再选择一张加入手牌。
- Volley：按解析后的 X 值随机攻击，升级增加每击伤害；让 `ResolveEnergyXValue` 统一应用 X 值 Hook，并修正 Whirlwind 对 X 值 Hook 的遗漏。
- 更新覆盖率文档，添加针对各行为的真实战斗测试，并运行完整 `scripts/check.sh`。

## 验收标准

- 四张牌接入生成 dispatch，单人无色牌模型达到 44/65。
- 新增测试覆盖伤害计算、Rend 的临时 Power 排除、Seeker Strike 的选择与牌堆移动、Volley 的 X 次数。
- `scripts/check.sh` 全部通过，L2/L3/L4 与 oracle 无分歧。

## 涉及文件

- `game/Models/Cards/GoldAxe.h`
- `game/Models/Cards/Rend.h`
- `game/Models/Cards/SeekerStrike.h`
- `game/Models/Cards/Volley.h`
- `game/Models/PowerModel.h`
- `game/Combat/History/CombatHistory.h`
- `engine/sim/state.h`
- `game/Models/CardModel.h`、`game/Models/CardModel.cpp`
- `game/Commands/CardSelectCmd.h`、`game/Commands/CardSelectCmd.cpp`
- `game/Models/Cards/Whirlwind.h`
- `tests/test_colorless.cpp`
- generated dispatch, README, roadmap, and execution report
