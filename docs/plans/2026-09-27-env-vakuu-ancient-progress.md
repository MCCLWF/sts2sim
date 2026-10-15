# 通用 Env：Vakuu 与先古访问历史

日期：2026-09-27。分支：`feat/generic-env-rf-api`。所属阶段：E1/E3，继续扩展铁甲战士三幕真实路线。

## 目标

按 v0.111.0 原版接入 Vakuu 的先古选项生成和 `Visits` 变量，保持所有候选遗物构造顺序、事件 RNG 消耗以及按先古事件和角色区分的存档访问次数。

## 工作

- 用 POD 状态承载每个先古事件、每个角色的访问次数，并纳入新局设置、快照遍历和导入校验。
- 按原版先构造三组共十件遗物，再分别以事件 RNG `UnstableShuffle`，每组选第一项；保留 Sere Talon 的 9 点最大生命值提示。
- 用聚焦测试检查变量、候选构造顺序、三组选项和随机抽取次数；用正版 oracle 的四条三幕路线验证实际后继。
- 当前 C/Python reset 参数还不能传入非零 AncientStats，本批明确记录此边界，不把零历史基线声称为真实档案支持。

## 验收

生成字段与 dispatch 检查、聚焦 CTest、完整 `scripts/check.sh` 通过；四条三幕路线至少越过原 `Vakuu.CalculateVars` 停点，任何新停点或分歧如实记录。

结果记录于[执行报告](../reports/2026-09-27-env-vakuu-ancient-progress.md)。
