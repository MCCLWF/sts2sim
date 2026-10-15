# 通用遗物批次 18：Maw Bank 与商店购买钩子报告

计划：[`../plans/2026-09-26-common-relics-18-maw-bank.md`](../plans/2026-09-26-common-relics-18-maw-bank.md)。本批扩展已有 The Courier 商店补货路径，并接入 Maw Bank。

## 实现

- 将 `ShouldRefillMerchantEntry` 和 `AfterItemPurchased` 生成为标准 AbstractModel 成员与 Hook dispatcher。
- MerchantEntry 继续用定长的 `Merchant::EntryRef` 表示。补货 Hook 按 run listener 顺序询问模型，首个 true 生效；The Courier 不再通过遗物 ID 特判。
- 两类购买（普通库存和删牌服务）都在购买及库存处理后调用购买完成 Hook。
- Maw Bank 在每个顶层房间进入时获得动态变量 12 金币；首次实际花费金币的购买会标记已用并禁用遗物；零花费购买不会消耗。

## 验收

- `python3 tools/gen/gen_members.py && python3 tools/gen/gen_hook_header.py && python3 tools/port/refresh_not_ported.py && python3 tools/gen/gen_dispatch.py`：通过；AbstractModel 174 个成员，Hook.h 141 个 dispatcher，6 个等待未支持类型，684 个模型接入分发。
- `cmake --build build -j2`：通过。
- `ctest --test-dir build --output-on-failure`：8/8 通过。Maw Bank 单测验证进商店获得 12 金币、零花费不消耗、普通付费购买后进入 Disabled 状态且之后不再给金币；既有商店测试覆盖 The Courier 卡牌/遗物/药水补货。
- Maw Bank + The Courier 定向 L4：种子 `M4BANK001`–`004` × 策略 `1,2`，不带 Neow、500 HP、强制两件遗物：**8/8 局、1585/1585 决策一致**，0 `Unported`、0 分歧。
- `./scripts/check.sh`：通过。ctest 8/8；冒烟无崩溃/卡死（94 death/6 unported，100 death，3 boss/97 death）；L2 **200/200**；L3 两组 **40/40**（784/784、827/827 状态）；L4 两组 **40/40**（1958/1958、2131/2131 决策），均 0 分歧、0 `Unported` 停止。
- Oracle 构建保留既存 `OracleRun.cs(234,84)` CS8604 可空性警告：0 错误。

## 限制

所有已移植的购买 Hook 可正常组合。游戏中其余尚未移植的 `AfterItemPurchased` 覆写仍由生成 dispatch 受控报为 `Unported`。
