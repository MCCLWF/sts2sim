# 通用 Env API 基线测量计划

状态：已完成修正口径的本机基线采集；数据与环境记录见[测量报告](../reports/2026-09-27-env-api-baseline.md)。该采集不是性能验收。

## 目标

用仓库当前公共 Env/Preview API 建立一次可复现的本机微基准，记录一个固定战斗根上的状态大小、克隆、Hash、固定 EndTurn 后继、Preview（trace 开启/关闭）和快照 JSON 导出耗时。测量用于后续比较，不作为性能验收或性能目标达成证明。

## 范围与边界

- 固定输入为 Ironclad、A0、`DUAL001`、`CULTISTS_NORMAL` 的静止战斗根；每次操作都从这个未推进的根独立执行。
- 仅使用 `Env::Clone`、`Env::Hash`、`Env::Predict`、`PreviewAction` 和 `Env::ExportSnapshotJson` 等普通公共接口。
- 不运行搜索策略、Beam 或评分逻辑；不做跨设备/真机性能对比，也不据此宣布达到设计性能目标。
- 每项做 9 个样本，每样本 1000 次操作，预热 32 次；计时区间包括 Clone、Predict、Preview 和字符串导出结果的销毁。
- 计时循环只从返回对象读取只读 POD 字段以形成摘要，不在操作计时内调用 Hash、Observe 或额外导出。Clone 使用 `run.total_floor`；Predict/Preview 使用 `run.phase` 与 `player_combat.energy`，Preview 另混 trace event 数。源根与普通/trace 开关一致性通过额外 Hash 校验，但所有校验都在计时区间外。
- 验证根 Hash 在测量前后不变，并验证 trace 开启/关闭的 Preview 后继 Hash 均与普通 Predict 相同。

## 产物

- `tools/bench/env_api_bench.cpp`：独立基准程序。
- `docs/reports/2026-09-27-env-api-baseline.md`：实际命令、环境、日志路径、原始测量值及限制。

## 验收

- 用已有 `build/libsts2*.a` 按仓库当前静态库顺序单独编译；不改 CMake、公共 Env/Preview 接口或核心文件。
- 成功运行后记录每项操作的迭代数、9 个样本的平均及中位单次耗时。
- 编译/运行日志保存在 `/tmp`；如编译或链接失败，报告完整失败原因，不以修改核心代码绕过。
