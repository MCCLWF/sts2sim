# 通用遗物批次 15：The Courier 商店补货

## 来源与实现

按游戏 v0.111.0 `Models/Relics/TheCourier.cs`、`Entities/Merchant/MerchantEntry.cs` 及各条目 `RestockAfterPurchase` 移植：

- 商店条目购买成功后调用 `Hook.ShouldRefillMerchantEntry`；单人持有 The Courier 时返回 true。
- 角色卡与无色卡槽重新生成同类型/稀有度的卡，卡牌折扣标记清除，并依游戏顺序重新计算价格、更新卡牌创建结果。
- 遗物槽重新掷稀有度，排除当前商店已有遗物，再从遗物袋后端抽取。
- 药水槽按商店 RNG 重新生成。游戏源码计算的遗物黑名单变量未传给补货，故按源码保留允许重复的行为。
- 删牌服务在独立的 `MerchantCardRemovalEntry` 购买包装器中，不触发补货钩子。

## 验证

- `python3 tools/gen/gen_dispatch.py`：通过，670 个已移植模型。
- `cmake --build build -j2 --target test_relics && ./build/test_relics`：通过，退出码 0。
- 针对性用例验证角色卡、无色卡、遗物、药水购买后的槽位补货，卡牌特价被清除；无 The Courier 时卡牌购买后槽位仍清空；删牌服务仍只使用一次。
- `./scripts/check.sh`：通过。ctest 8/8，冒烟无崩溃/卡死，L2 200/200；L3 两组各 40/40（784/784、827/827 状态）；L4 两组各 40/40（1958/1958、2131/2131 决策）；0 分歧、0 `Unported` 停止。Oracle 构建 0 警告、0 错误。

## 边界

批次只补 The Courier 的商店补货与其已有 20% 折扣。跨职业卡牌池、其他尚未移植的商店/遗物回调仍按 `Unported` 处理。
