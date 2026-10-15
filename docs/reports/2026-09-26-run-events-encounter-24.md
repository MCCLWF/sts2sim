# M4 run 内容批次 24：蘑菇事件与 Construct Menagerie

计划：[批次 24](../plans/2026-09-26-run-events-encounter-24.md)。本批接入 Hungry for Mushrooms、Big Mushroom、Fragrant Mushroom，以及 Construct Menagerie Normal。

## 实现

- Hungry for Mushrooms 事件现在可生成两项遗物选择；选择后正确结束事件。
- Big Mushroom 在获得时增加 20 最大生命，第一回合的抽牌 Hook 减少 2 张。
- Fragrant Mushroom 在获得时造成 15 点无视格挡且无力量伤害，并按源码的稳定洗牌顺序升级最多 2 张可升级牌。
- Construct Menagerie Normal 生成一只 Punch Construct 和两只 Cubex Construct，接入现有遭遇生成分发。

## 验收

- `python3 tools/gen/gen_dispatch.py`：通过，705 个模型接入分发。
- `cmake --build build -j2`、`test_relics`：通过。测试覆盖两件遗物的获得效果、第一回合抽牌修正、事件两项选择和遭遇组成。
- 定向 Oracle L3：种子 `M4EVENT24001`–`002` × 策略 `1,2`，**4/4 场战斗、56/56 状态一致**，0 `Unported`、0 分歧。
- `./scripts/check.sh`：通过；ctest 8/8，smoke 94/6、100/0、boss 3/97，L2 200/200，L3 两组 40/40（784/784、827/827），L4 两组 40/40（1958/1958、2131/2131）。Oracle 仅保留既有 `OracleRun.cs(234,84) CS8604` 警告，0 errors。

## 边界

本批没有实现其他角色卡牌。后续仍需把更多事件和第三幕/Underdocks 遭遇接入整局路径。
