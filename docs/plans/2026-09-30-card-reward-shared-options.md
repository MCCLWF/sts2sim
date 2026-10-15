# 卡牌奖励创建选项的共享修改

基底：`5266daf`；分支：`fix/card-reward-shared-options`。属于 E3 新种子首分歧修复，通用 Env/API 主线保持未完成。

## 首因与目标

深幕注入批 `M9AA996BBF59348 / REGENT / A10 / policy 1552014862 / HP 10000` 第 485 决策的第四张卡：原版 `FURNACE`，模拟器 `BLACK_HOLE`，此前 485 个决策一致。原版 `CardCreationOptions` 是可修改的引用对象；Dingy Rug 更新卡池后，Lasting Candy 必须看到同一创建选项。当前 C++ 在单卡 helper 中按值复制，使后续奖励 Hook 丢失该更新。

## 工作范围

- `game/Factories/CardFactory.h/.cpp`：在整批生成及后续修改 Hook 中保留创建选项修改，允许持有奖励的调用者保存这些修改。
- `game/Rewards/Reward.cpp`：保留普通 CardReward.Options/RerollOptions 的原版共享对象效果。
- `game/Models/Relics/LastingCandy.h`：新建附加卡的选项，遵循原版 flags、source、rarity 及池/筛选语义。
- 必要的差分证据、报告、移植坑点及现状链接；不修改批次工具、Oracle 或其它工作区。

## 验收

1. 以冻结的原始 Oracle 轨迹重放上述完整对局，明确记录完整终点与所有决策；先验证该首分歧已消失，后续新分歧如实保留。
2. 对卡池变更保存和 reroll 进行必要的原版探针或有限合同验证。
3. 独立构建、相关 CTest；提交前保留源码及 CLI 指纹。main 合入仍由集成方跑完整 `scripts/check.sh`。

## 容量限制

另两条新桶只读取证。Noisebot/Supermassive 属于主代理既有修复；1025 回合 `StateLog` 的定长容量异常涉及状态/快照合同，留给协调后的共享修复，不通过增大容量掩盖。
