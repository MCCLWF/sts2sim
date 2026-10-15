# 通用 Env：移除活动 CombatSolver 桥接接口

日期：2026-09-28。分支：`feat/generic-env-rf-api`。

## 目标

让活动 Env 只保留通用原版执行、快照恢复、查询和 RF 所需的公共基础。旧 CombatSolver 的 live mapper 模板和 monster machine 注册导出移出活动 ABI；历史代码继续留在归档目录供审计，不参与构建。

## 范围

- 删除 `blank_template` 和 `monster_machines` 的 C++/C ABI 出口及其测试夹具。
- 保留普通逻辑快照、选择恢复、owned 预测、查询和未移植内容的明确错误。
- 不修改 CombatSolver 子模块，不恢复旧 Beam、评分、状态键或搜索策略。

## 验收

- 活动 C ABI 不再导出两个 live mapper 接口。
- CTest、Python Env、`scripts/check.sh --quick` 通过。
- 源码搜索只在 `archive/` 和历史报告中发现这两个旧协议名称。
