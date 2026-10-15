# M4 遗物批次 21：战士遗物池收口

计划：[批次 21](../plans/2026-09-26-ironclad-relics-21.md)。按游戏源码接入 Demon Tongue、Paper Phrog、Ruined Helmet。至此数据表的铁甲战士专属遗物池 8/8 个模型均有头文件；这只表示模型覆盖，不等同于所有战斗情境都已穷尽验证。

## 实现

- Demon Tongue：持有者回合首次受未格挡伤害后回复相同生命值；下一持有者回合重置，敌方回合不触发。
- Paper Phrog：持有者攻击易伤目标时，在易伤倍率上加 0.25；通过 Vulnerable Power 的原有倍率计算路径接入。
- Ruined Helmet：每场战斗首次收到正力量时将数量翻倍；后续同场力量不翻倍，战斗结束重置。

没有增加其他角色卡牌或跨职业生成效果。

## 验收

- `python3 tools/gen/gen_dispatch.py`：通过，696 个模型接入分发。
- `cmake --build build -j2`、`test_relics`：通过。测试覆盖每回合首次受伤回复、敌方回合不回复、易伤倍率与非强化攻击、首次力量翻倍及重置。
- 强制持有三件遗物的 Oracle L4：种子 `M4IRON21001`–`004` × 策略 `1,2`，不带 Neow、500 HP，**8/8 局、1573/1573 决策一致**，0 `Unported`、0 分歧。
- `./scripts/check.sh`：通过。ctest 8/8；冒烟无崩溃/卡死（94 death/6 unported，100 death，3 boss/97 death）；L2 **200/200**；L3 两组 **40/40**（784/784、827/827 状态）；L4 两组 **40/40**（1958/1958、2131/2131 决策），均 0 分歧、0 `Unported` 停止。
- Oracle 构建仅有既存 `OracleRun.cs(234,84)` CS8604 可空性警告，0 错误。

## 边界

跨职业取牌仍保持 Unported，其他角色卡牌不在当前范围。M4 其余 run 内容继续进行中。
