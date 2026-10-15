# 通用 Env 勘察与开发路线调整

## 目标与用户方向

2026-09-27 用户要求全面勘察 C++ 通用 Env、CombatSolver Env 后端两条线，并将主线调整为：补齐原版单人规则和可供 Random Foreseer 类客户端使用的通用预测 API；暂停 CombatSolver 移植，待 Env 验收完成后再恢复。最终可放入 `mods`、双模式设置与真实性能比较的产品目标保留。

本次交付是代码与验证证据的审计、后续 Env 工作顺序和移植恢复门槛，不宣称本次已补完 Env。

## 前置条件的实际状态

- 基线：`feat/combatsolver-env-backend` 的 `a8d0baa`，游戏 v0.111.0；工作区开始时干净。
- CombatSolver 子模块：`d231e9e51a0e58d6bfa1373c6265cce13ffd9a45`。
- M0/M1/M3 基础已存在；M2 验收是战士卡矩阵和第一幕路线；M4 的全内容、完整 live 导入与 M5 性能仍未完成。
- `EnvBackendSettings.EnvExecutionGate` 恒拒绝正式 Env 执行；Native 搜索之后的 Env 重放验证不能算核心替换完成。
- 旧进度的模型数、文件数、通过的有限用例与全支持混用，需以本次审计注明口径。

## 本次任务

1. 读取游戏基线、`data/content.json`、模型注册、Env/C ABI/Python、快照导入及执行追踪。
2. 只读检查派生 Mod 的 live 捕获、原 Beam 接入、设置与硬门禁。
3. 固定 Random Foreseer 公开源码版本，按功能反推通用快照、命令、查询、结果与追踪需求；其镜像规则不作为游戏真值。
4. 运行现有全量门禁及有限 managed 夹具；明确每个结果的边界。
5. 增加只读 `tools/coverage/env_inventory.py`，使当前静态库存可复核；它不代替条件分支/真值覆盖验收。
6. 在报告中定义 Env 的完成范围、分阶段工作与恢复移植门槛；更新项目规则、README、设计、路线图、D-001 和旧移植计划的暂停状态。

本次不新增搜索投影或后端实现，也不改变游戏安装、上游子模块和 Mod 设置。

## 验收与产物

- `scripts/check.sh` 的原始摘要写入报告；同时说明默认 L4 停在第一幕 Boss 前，以及 Unported 的验收口径。
- `EnvCombatPredictionChecks`、`EnvLiveCardChecks`、`EnvSessionChecks` 的实际运行结果及有限范围写入报告。
- 模型盘点区分：基线数量、头文件、注册、静态未发现 stub、差分证明。禁止用文件比例作为完整实现百分比。
- 报告：`docs/reports/2026-09-27-generic-env-readiness.md`。
- Env 后续总体计划：`docs/plans/2026-09-27-generic-env-completion.md`，每个阶段实施前另写小批次计划。
- 代码主线切换为通用 Env；CombatSolver 移植状态为暂停。最终 Mod 产品验收保留为恢复后的工作。
