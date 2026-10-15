# M4 药水批次 22：战士与事件药水

计划：[批次 22](../plans/2026-09-26-ironclad-event-potions-22.md)。按游戏源码接入 Soldiers’ Stew、Ambergris、Glowwater 及 Ambergris Power。

## 实现

- Soldiers’ Stew：目标玩家战斗中所有带 Strike 标签的牌，基础回放次数各加 1。铁甲战士药水池 3/3 个模型现在都有头文件。
- Ambergris：回复目标最大生命的 50%；战斗中额外施加一次额外回合能力。战斗外只回复生命。能力在额外回合被取用后递减。
- Glowwater：先耗尽目标玩家当前手牌，再抽取最多 10 张牌。

## 验收

- `python3 tools/gen/gen_dispatch.py`：通过，700 个模型接入分发。
- `cmake --build build -j2`、`test_potions`：通过。行为测试覆盖药水实际使用、Strike 与非 Strike 牌、Glowwater 的手牌/耗尽牌堆，以及 Ambergris 战斗内外的不同效果和额外回合 Hook。
- `./scripts/check.sh`：通过。ctest 8/8；冒烟无崩溃/卡死（94 death/6 unported，100 death，3 boss/97 death）；L2 **200/200**；L3 两组 **40/40**（784/784、827/827 状态）；L4 两组 **40/40**（1958/1958、2131/2131 决策），均 0 分歧、0 `Unported` 停止。
- Oracle 构建仅有既存 `OracleRun.cs(234,84)` CS8604 可空性警告，0 错误。

L4 现有入口没有指定开局药水的参数，因此这次 L4 是回归门禁；新增药水的直接行为由 `test_potions` 验证，尚未进行强制药水 Oracle 对拍。

## 边界

事件药水 Foul Potion 的商店、假商人事件和战斗效果尚未接入，仍保持 Unported。其他角色卡牌与跨职业取牌继续不实现；M4 继续进行中。
