# M4 遗物批次 19：战士回复与通用资源规则

计划：[`../plans/2026-09-26-relic-batch-19-run-economy.md`](../plans/2026-09-26-relic-batch-19-run-economy.md)。按游戏源码接入 Black Blood、Darkstone Periapt、Sozu、Ectoplasm、Runic Pyramid。

## 实现

- Black Blood：持有者存活时，战斗胜利后回复动态变量指定的 12 生命。
- Darkstone Periapt：持有者的诅咒进入牌组时增加 6 最大生命；非诅咒不触发。
- Sozu：最大能量增加 1，持有者无法获得药水。
- Ectoplasm：最大能量增加 1，持有者获得的金币变为 0；使用现有 `AfterModifyingGoldGained` 反馈链路。
- Runic Pyramid：持有者回合结束时不清空手牌。

五件遗物均通过已有的模型与 Hook 分发接入，没有新增遗物 ID 特判。跨职业取牌仍保持当前 Unported 边界。

## 验收

- `python3 tools/gen/gen_dispatch.py`：通过；690 个模型接入分发。
- `cmake --build build -j2`：通过。
- `test_relics`：通过。实际命令与 Hook 测试覆盖胜利回复、诅咒入牌组、药水拒绝、金币阻止、最大能量和弃手牌条件。
- 五件遗物强制持有的 Oracle L4：种子 `M4REL19001`–`004` × 策略 `1,2`，不带 Neow、500 HP，**8/8 局、1542/1542 决策一致**，0 `Unported`、0 分歧。
- `./scripts/check.sh`：通过。ctest 8/8；冒烟无崩溃/卡死（94 death/6 unported，100 death，3 boss/97 death）；L2 **200/200**；L3 两组 **40/40**（784/784、827/827 状态）；L4 两组 **40/40**（1958/1958、2131/2131 决策），均 0 分歧、0 `Unported` 停止。
- Oracle 构建保留既存 `OracleRun.cs(234,84)` CS8604 可空性警告：0 错误。

## 边界

本批扩展的是战士和通用遗物的实际行为。其他角色卡牌以及无色 Splash 的跨职业生成路径仍未接入；M4 继续进行中。
