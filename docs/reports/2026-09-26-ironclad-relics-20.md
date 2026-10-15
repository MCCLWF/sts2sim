# M4 遗物批次 20：战士战斗遗物

计划：[批次 20](../plans/2026-09-26-ironclad-relics-20.md)。按游戏源码接入 Brimstone、Self-Forming Clay 和 Self-Forming Clay Power。

## 实现

- Brimstone：持有者回合开始时自身获得 2 力量，全部存活对手各获得 1 力量；敌方回合不重复触发。
- Self-Forming Clay：持有者在战斗中受到未格挡伤害后获得 3 层专属能力；每次受伤独立叠加。
- Self-Forming Clay Power：持有者下一次清除格挡时获得累计层数对应的格挡，并移除能力。

三个模型通过现有 Hook 与自动生成的分发接入，没有新增角色卡牌或跨职业生成路径。

## 验收

- `python3 tools/gen/gen_dispatch.py`：通过，693 个模型接入分发。
- `cmake --build build -j2` 和 `test_relics`：通过。行为测试覆盖双方力量、敌方回合不触发、零未格挡伤害不触发、两次受伤叠加及清除格挡时兑现。
- 强制持有两件遗物的 Oracle L4：种子 `M4IRON20001`–`004` × 策略 `1,2`，不带 Neow、500 HP，**8/8 局、1219/1219 决策一致**，0 `Unported`、0 分歧。
- `./scripts/check.sh`：通过。ctest 8/8；冒烟无崩溃/卡死（94 death/6 unported，100 death，3 boss/97 death）；L2 **200/200**；L3 两组 **40/40**（784/784、827/827 状态）；L4 两组 **40/40**（1958/1958、2131/2131 决策），均 0 分歧、0 `Unported` 停止。
- Oracle 构建仅有既存 `OracleRun.cs(234,84)` CS8604 可空性警告，0 错误。

## 边界

此批只扩展战士遗物及其专属能力。其他角色卡牌及战斗内跨职业取牌仍保持 Unported；M4 继续进行中。
