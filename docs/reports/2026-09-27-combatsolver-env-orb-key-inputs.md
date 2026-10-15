# CombatSolver Env：原 Beam 充能球键输入

日期：2026-09-27。分支：`feat/combatsolver-env-backend`。前置提交 `73f21c4`，固定 upstream `d231e9e51a0e58d6bfa1373c6265cce13ffd9a45` 无修改。计划：[M5 球键输入](../plans/2026-09-27-m5-orb-key-inputs.md)。

## 结果与限制

五种原版球的只读数值与原队列部分指纹已对齐。三组 TestMode=false 完整 live 根各有 6 个球、容量 8，逐 decimal 位比较正版 getter、实际固定原 OrbMirrors 与 C++ 公开查询；反射调用实际原 `AppendOrbs` 的部分指纹与派生编码相同。原 Beam 12 场有限后继核对也通过新增球指纹检查。

这是完整原状态键中的一个部分。`original_state_key_complete=false`、`OriginalStateKeyReady=false`、历史/政策/威胁完整度和正式 Env 执行门禁保持关闭。没有改变 Beam 候选、评价、剪枝、预算或启用 Env 后端；尚不能交付完整替代 Mod/宣称同算法性能收益。工作区外旧开发包未更新。

## 真值与实现

- 固定原 `CombatBeamSolver.StateEvaluation.cs` 的 AppendOrbs 顺序为 `'O'`、Capacity、Count，再按队列顺序添加每个 Id.Entry、OrbMirrors.GetPassiveValue/GetEvokeValue。没有加入 UID、removed 或原始内部字段；不能用 Env hash 代替编码。
- OrbMirrors 对 Lightning/Frost/Dark/Glass 的部分数值调用 Hook.ModifyOrbValue；Dark evoke 直接读保留内部值，Plasma 不受 Focus 影响，Glass evoke 在修改 passive 后乘 2。当前五种 C++ 原版 getter 与该合同相同，Focus 读取实际 owner 并 clamp 到零。
- 独立 `sts2env.combat_orb_queries` v1 提供队列容量、顺序、local orb ref、模型名、被动/激发 decimal 四字及源 prediction hash。C++ 在 owned 私有 clone 查询，要求所有模型可用并检查完整 GameState hash 纯度；源 hash/历史不变。
- C ABI `sts2_combat_prediction_export_orbs`、公开 `CaptureOrbs()` 返回 owned typed values/read-only 队列与 JSON，检查协议/hash、容量、身份、重复 ref、五种模型与 decimal 形状。`EnvSearchOrbFingerprintProjection.AppendTo` 复用固定原 StateFingerprintBuilder 编码，EnvSearchBranch 缓存分支专有值。诊断节点直接调用实际原 AppendOrbs 核对，未知/分歧仍拒绝该根。
- 真实 OrbCmd.Channel 生成 Lightning 两个、Frost/Dark/Glass/Plasma 各一个。Dark 内部 evoke 为 `-2.5000m`，Glass passive 为 `1.2500m`；真实 Focus 为 2/-4。负 Focus 下 Lightning/Frost/Glass passive clamp 为整数零，Dark evoke 保留负数/scale，Plasma 仍 1/2。不同球数与容量、重复实例、类型与顺序都进入原部分指纹。

## 验收

| 命令 | 实际结果 |
|---|---|
| `cmake --build build -j 4` | `/tmp/mcc-orb-key-build.log`：所有目标构建成功 |
| `scripts/check.sh` | `/tmp/mcc-orb-key-full.log`：`all checks passed`，CTest 10/10；L2 200/0 mismatched；L3 40/40 两组，784/784、827/827 states；L4 各 40 完整路线，1958/1958、2131/2131 decisions，0 Unported/分歧 |
| `python3 tools/EnvQueryChecks/check.py --out /tmp/mcc-orb-key-queries` | `/tmp/mcc-orb-key-queries.log`：三组 `capacity=8; orbs=6; game_original_env_exact_bits=true; original_encoder=true; complete_state_key=false`；公开 managed 与静态 C++ 源/clone 纯度、Unported、原计算变量、22 张卡牌指纹、getter/政策核心与增长降级均通过；工具也重新构建实际派生程序集 |
| `~/.dotnet/dotnet run --project tools/EnvCombatPredictionChecks -c Release -- --out /tmp/mcc-orb-key-beam` | `/tmp/mcc-orb-key-beam.log`：12 场通过，新增实际原球编码核对；仍 `full_state_comparison=false; full_history_payload_comparison=false` |
| `~/.dotnet/dotnet run --project tools/EnvSessionChecks -c Release` | `/tmp/mcc-orb-key-api.log`：64 普通并行预测、16 专用分支、orb query concurrent/owned/source/Dispose、严格恢复/失败恢复通过 |
| `python3 tools/CombatSolverEnv/check_prediction_abi.py --library build/libsts2env_c.so /tmp/mcc-calculated-abi-fixtures` | raw C ABI 通过，包括新查询协议/hash、终局/选择 scope、源隔离和诊断/null 句柄拒绝；复用当前同 schema 的 C++ 合同生成 fixture |
| `python3 tools/CombatSolverEnv/run_smoke.py --mode both --expect-profile-root-rejection --memory-reservation-mib 2048 --out /tmp/mcc-orb-key-game` | `/tmp/mcc-orb-key-game.log`：`derived smoke passed`；game.log 记录 live profile 明确拒绝、env_imported=false/env_used=false、warning shown=true、Native 继续（expanded=100/transitions=335）；隔离实例已清理 |
| `git diff --check`、`python3 tools/gen/gen_state_fields.py --check`、固定子模块状态 | 通过，没有 POD/schema 或 upstream 修改 |

没有测试失败后修改真值/放宽比较。一次 patch 因方法签名锚点不匹配整体拒绝，核对实际文件签名后重新应用；没有修改 upstream 来绕过锚点。

## 未验收项

这三组非空球查询都是 active 根；尚未证明非空球的原 Beam 跨回合/终局完整影子状态。上一批六组 full-live 出牌与两次 EndTurn 的结果仍有效，其中 Barrage 使用三个 FrostOrb；本批没有改变玩法执行。已移除球的历史分配/对象闭包、所有 Hook/风险、完整 combat/战略账本、原状态键和原评分仍待补齐。隔离游戏只证明加载与当前部分档案的拒绝/提示/Native 继续，尚非可见切换、Env 接受或完整替换验收。

继续按同算法适配计划补其余键/政策/评分，再接 Env Fork/Replay/round/choice 路由，最后验收取消/部署和同根等预算性能并生成最终 `mods/CombatSolverEnv`。
