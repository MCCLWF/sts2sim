# 通用 Env：Archaic Tooth 起始牌转换

日期：2026-09-27。分支 `feat/generic-env-rf-api`；战士整局主线。真值 v0.111.0 `Models/Relics/ArchaicTooth.cs`、`CardCmd.Transform` 和正式 Oracle。

## 目标

Orobas 选项生成已按原版抽出 Archaic Tooth，但原版二幕批量轨迹 SEED0002/policy2 在取得它时因 `AfterObtained` 缺失停止。实现获得时将牌组中第一张对应起始牌转为 Ancient 卡的原版步骤，继续跑真实轨迹。

## 工作与约束

1. 保留五种原版映射；按 deck 当前顺序查第一张，调用 `RunState.CreateCard` 生成替换牌、继承已升级状态和支持的附魔，再调用通用 `CardCmd.Transform`。不读取选项生成时保存的展示卡作为实际转换对象。
2. 目前附魔 `MutableClone` 的全部 SavedProperty 复制尚无真值支持；遇到时明确 `Unported`，不近似复制。
3. 生成分发、构建、对原版 SEED0002/policy2 二幕轨迹逐决策重放/恢复；记录下一个实际阻塞。完整门禁继续执行。

## 判据

取得遗物后牌组、遗物/RNG、选择和后继状态与原版一致；未选择的分支不产生额外行为；非法/未实现条件明确失败。
