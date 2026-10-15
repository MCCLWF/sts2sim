# E0 整局采集策略覆盖

日期：2026-09-30。分支 `fix/seed-policy-coverage`，基于 `5266daf`。

## 前置与范围

已读 AGENTS、design、roadmap、deviations、pitfalls。M0/M1 及严格批次工具已有有限验收；E0 原生合法动作集合、执行覆盖和 M5 万局仍未完成。此任务在独立工作区实施，不改正在采集的冻结工作区，也不改通用 Env/ABI/RF 实现。

源码确认旧整局策略跳过 Neutralize/Survivor、没有药水动作、卡牌多选固定最大数量、卡牌奖励只选卡。策略使用独立 `System.Random`，原版玩法 RNG 不作任何改动。

## 工作

1. 默认 `coverage-v2` 移除两张 Silent 起始牌的硬跳过，并修复目标分支缺大括号造成的所有无目标牌遗漏；显式 `legacy-v1` 保留旧策略 RNG 调用及选择行为，供历史固定路线回归。
2. 按原版 NPotionPopup 的所有权/排队/使用类型/自定义可用限制和 PotionModel.IsValidTarget 枚举药水目标。使用 EnqueueManualUse、DiscardPotionGameAction 保留原生动作、历史和 hooks；Automatic 只能丢弃。
3. 多选随机合法数量与有序子集；奖励随机原生 cards/alternatives，包含 Skip/Reroll，强制 Sacrifice 继续优先。
4. L4 文本协议补 `potion slot combat_id` 和 `discard slot`，映射已有 Env 动作。策略版本写入 begin、批次 config/job 身份；采集端拒绝实际版本不符，续跑拒绝改版本，离线旧 trace 继续可回放。
5. 使用新原版输入验证 Silent 两牌、药水 use/discard、多选数量和 reward alternatives，保留失败首因；运行适用工具回归、独立构建和检查。更新 README/roadmap 并报告实际结果。

## 验收

- 最小新 L4 批次保留原始 trace/动作/版本/manifest，CLI 逐决策比较并验证快照后继。
- 明确统计新动作实际发生次数；失败不替换种子，不调整策略或游戏 RNG 以迁就 C++。
- 旧策略显式回归；新策略残余模拟器缺口单独报告，不把本任务当作全覆盖或万局完成。

## 文件

`tools/Sts2Oracle/OracleRun.cs`、`tools/cli/replay_cmd.cpp`、`tests/l4/l4_run.py`、`tools/differential/batch.py`、适用协议回归、工具 README、项目 README/roadmap、计划和报告。
