# CombatSolver 与 sts2sim Env 双求解器对拍

## 目标

把 CombatSolver 的离线原生求解器接入 sts2sim 的 Env 对拍流程：同一 seed、角色、飞升和遭遇战分别在原生游戏 harness 与 C++ Env 中求解，比较根状态、首个动作和可执行路线，并记录两边的搜索耗时与吞吐。

## 任务

- 为 Env 增加直接进入指定遭遇战的调试初始化入口，并通过 C ABI/Python 暴露。
- 增加 Env 侧固定预算参考求解命令，输出规范化根状态、首动作、rollout 数和耗时。
- 增加双求解器 runner，并行启动 CombatSolver OfflineSearchHarness 与 Env solver，统一解析结果。
- 对同一场景做首个动作/根状态偏差报告，记录 native 与 Env 的速度指标。
- 添加单元测试与文档，明确 native 依赖需要 STS2、RitsuLib、CombatSolver 构建产物。

## 验收

- C++/C ABI/Python 构建通过，直接遭遇战入口可得到 Combat 决策。
- 双 runner 能在当前机器上跑至少一个 Ironclad 场景，产生 JSON 报告。
- 报告明确区分根状态偏差、动作偏差和性能差异；不把不同搜索算法的动作差异误报为模拟器状态错误。
- `scripts/check.sh --quick` 通过。

## 进度记录（2026-09-26）

- ✅ C ABI/Python 直达遭遇战、Env 参考搜索和离线并行 runner 已完成。
- ✅ 游戏内比较 Mod 已加载到隔离正版游戏，能在同一战斗首回合同时运行两边搜索并记录根字段、首动作和耗时。
- ✅ 新增不搜索的根状态探针；只有结构字段匹配才运行 Env 搜索，且检查与搜索复用同一个 Env 状态。C ABI/Python 回归与隔离游戏测试通过。
- ✅ 扩展到 `CULTISTS_NORMAL`、`GLOBE_HEAD_NORMAL`、`AXEBOTS_NORMAL` 三种遭遇的离线样例；数据和解释见[执行报告](../reports/2026-09-26-combat-solver-env-bridge.md)。
- ⏳ 任意 live 回合的完整状态导入/精确动作回放和相同后继状态差分尚未完成。当前性能数据只比较不同算法、不同预算的原始时间，不能得出同质量提速结论。
