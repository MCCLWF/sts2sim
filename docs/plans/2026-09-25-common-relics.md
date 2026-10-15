# 通用遗物直接钩子批次

分支：`feat/m2-completion`。药水批次已通过门禁，本批继续处理通用遗物中已有完整 run 钩子、无需新增休息点选项的内容。

## 范围

`DragonFruit.AfterGoldGained`、`BowlerHat.ModifyGoldGained/AfterModifyingGoldGained`、`Planisphere.AfterRoomEntered`。

Girya 和 Shovel 的休息点自定义选项不在本批实现，继续显式保留未实现。

## 验收

`tests/test_relics.cpp` 在真实 RunState 中验证金币倍率、获得金币后的最大生命和首次未知房间回血；生成分发、完整构建、单测、烟测和 L2/L3/L4 门禁通过。
