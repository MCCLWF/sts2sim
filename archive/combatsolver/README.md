# CombatSolver 冻结档案

本目录保留先前 CombatSolver 接入所用的 C++ 专用预测 session、ABI、终局旁路、Beam 示例与测试。它们不属于当前通用 Env，也不参加默认构建。这里的旧接口与当前源码不兼容，不能直接构建或作为保真验收。

`archive/combatsolver/managed-tools/` 保存 `CombatSolverEnv` 及依赖其旧 SDK 的 managed 工具；它们已移出活动工具目录，不参加当前构建。`external/CombatSolver` 保留历史来源；不以其规则、评分、状态键或历史投影指导通用 Env。

当前主线是普通原版执行、整局状态恢复、owned Preview 和 Random Foreseer 所需公共能力。原版 getter 与事实采集已迁到 `env/game_queries.*` 和 `engine/sim/execution_trace.h`，这些能力不提供搜索策略。恢复 CombatSolver 开发需要用户重新明确要求。
