# 通用遗物批次 15：The Courier 商店补货

## 目标

移植 `TheCourier.ShouldRefillMerchantEntry`，使持有者在商店购买卡牌、遗物或药水后补充对应库存槽。

## 范围

- 将 C# `MerchantEntry` 参数映射为既有的 POD `Merchant::EntryRef`。
- 卡牌补货复用原卡池类型/稀有度，取消折扣并按现有流程重新计算价格。
- 遗物补货按新抽出的稀有度，排除当前库存中的遗物；药水补货复用当前商店 RNG。
- 删牌服务走单独购买回调，不受 `ShouldRefillMerchantEntry` 影响。

## 验收

- 测试 The Courier 持有时购买后卡牌、遗物、药水槽仍有库存，卡牌折扣标记被清除。
- 测试无 The Courier 时购买仍清空槽位；删牌服务仍只使用一次。
- 运行生成分发、构建、`ctest`、`git diff --check` 与完整 `scripts/check.sh`。
