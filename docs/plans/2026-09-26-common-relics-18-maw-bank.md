# 通用遗物批次 18：Maw Bank 与商店购买钩子

## 目标

实现 Maw Bank 的进房金币与首次付费购买后停用，并用通用 run hook 分发商店补货和购买完成回调。

## 工作项

- 生成 `ShouldRefillMerchantEntry` 与 `AfterItemPurchased` 的 POD MerchantEntry 签名。
- 按源码 listener 顺序分发购买回调；补货判定由每个监听模型返回 bool，首个 true 生效。
- 将 The Courier 接入通用 `ShouldRefillMerchantEntry` dispatch，移除特定遗物 ID 分支。
- 实现 Maw Bank：顶层房间进入时获得动态金币；首次消费金币的购买后标记已用并禁用；零花费不触发。
- 为房间触发、付费购买、状态变化与无遗物商店行为加测试，更新路线图与 README。

## 验收

- 生成脚本与构建通过，`ctest` 全绿。
- 强制 Maw Bank/The Courier 的商店 Oracle L4 逐状态一致。
- 完整 `scripts/check.sh` 通过。

## 涉及文件

- `tools/gen/gen_members.py`、`tools/gen/gen_hook_header.py`
- `game/Models/AbstractModel.def`、`game/Hooks/Hook.h`、`game/Hooks/Hook.cpp`
- `game/Models/Relics/MawBank.h`、`game/Models/Relics/TheCourier.h`
- `game/Entities/Merchant/MerchantInventory.cpp`
- `tests/test_relics.cpp`
- 生成 dispatch、路线图、README 与报告
