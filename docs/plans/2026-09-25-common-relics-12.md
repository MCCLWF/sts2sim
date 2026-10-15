# M4 共享遗物批次 12

## 目标

移植 Membership Card，并为它接通商店价格 Hook。同步接入 The Courier 已有的折扣效果，以保证 Hook 对它也与游戏一致。

## 任务

1. 为商店条目增加固定容量的 `EntryRef` Hook 参数，保留条目种类和索引。
2. 按 v0.111.0 的 Decimal 运算与整数转换移植 Membership Card 50% 折扣和 The Courier 20% 折扣。
3. 在真实商店场景测试标价、购买资格与购买扣款。
4. 运行 `scripts/check.sh`，更新 README、路线图和执行报告。

## 验收标准

- 商店价格 Hook 只在商店房间运行，按 run hook listener 顺序修改 Decimal 价格，再以游戏一致的方式转为整数。
- Membership Card 将本玩家的商品价格乘以 50%；The Courier 乘以 80%。
- 定向购买测试和 `scripts/check.sh` 全部通过；L4 无新增分歧。
- 报告中明确标记 The Courier 的补货 Hook 仍未移植。

## 依据与文件

- 真值：v0.111.0 `Models/Relics/MembershipCard.cs`、`Models/Relics/TheCourier.cs`、`Entities/Merchant/MerchantEntry.cs` 和 `Hooks/Hook.cs`。
- 实现：模型签名、商店 Hook/定价与生成分发、遗物模型和定向测试。
- 进度：`README.md`、`docs/roadmap.md`；结果：`docs/reports/2026-09-25-common-relics-12.md`。
