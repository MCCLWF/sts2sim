# M5：Mod 药水库存键

分支 `feat/combatsolver-env-backend`，前置 `a44d4da`。原 Beam 循环键在牌堆形状之外包含原根固定槽位数和分支当前药水库存；通用 Env 的 owned snapshot 已保存槽引用与药水模型 ID。

1. 根打开时在 C# Mod 保存固定药水槽数，Fork/Predict 继承；未知或不一致的槽布局明确拒绝。
2. 从分支 owned snapshot 的槽引用和药水池模型 ID，在 Mod 生成固定原 `BuildPotionInventoryKey` 的编码，C++ Env 不写原搜索政策。
3. TestMode=false 的 16 个原 Beam 场景逐位对拍原私有方法；构建、getter/API 和完整 `scripts/check.sh` 通过，更新报告、README/路线图。
4. 不把药水部分键称为完整循环键；存活敌人位图和搜索边界仍需独立审计。
