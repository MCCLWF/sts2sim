# 通用 Env：Reflections 与长战斗池回收执行报告

日期：2026-09-27。分支：`feat/generic-env-rf-api`。计划：[Reflections 与长战斗池回收](../plans/2026-09-27-env-reflections-and-combat-pool.md)。依据：v0.111.0 `Reflections.cs`、`CombatState.cs` 与正版 L4 oracle。

## 首因与实现

- Grave of the Forgotten 接入后，另外两条三幕路线首停于 `Events.Reflections.GenerateInitialOptions`，已对齐 2674/3520 决策，零分歧。按源码加入 Touch a Mirror 的两次降级、四次升级与 Shatter 的原牌组顺序复制加 Bad Luck；演出不进入无头 Env。
- 越过 Reflections 后两条路线在 Fabricator 长战斗中遇到 creature pool 23 槽耗尽，首停于决策 857/861，3029/3520 决策已对齐。oracle 显示该场持续召唤小机器人，长达 39/75 回合；已有生物已移出战斗但原池只在离开房间时整池回收。Env 现在只在完成的战斗决策边界扫除不再可达的生物及其能力槽位，保留活跃、逃跑、宠物与能力/卡牌引用，并让分配器重用最低空闲槽。没有改动战斗操作或 RNG。
- 接着另一条路线在决策 1143 遇到 Hook 临时监听器数组 512 项上限。oracle 同时显示约 500 张 Wound 分布于牌堆，数组溢出属于 C++ 有界实现。初版直接扩到 4096 项虽通过该路线，但固定战斗根的 `Predict(EndTurn)` 本机基准达到 589µs；因此改为 512 项内联、超过时才动态扩展的临时列表，维持 C# 列表顺序，且不进入 GameState 的 POD 布局。相同本机基准重测为 175µs，四条路线的结果不变。通用 `StaticVec`/池异常添加容量与模板类型诊断，以便下一次定位真正越界项。

## 验证

| 检查 | 原始结果 |
|---|---|
| `cmake --build build --target sts2sim_cli test_env -j2` | 通过 |
| `./build/test_env && ./build/test_relics` | 两者通过；`test_env` 包含 Queen `_amalgam` 保留与监听器溢出断言，完整门禁又执行通过 |
| `DOTNET=~/.dotnet/dotnet python3 tests/l4/l4_run.py --seeds SEED0003 --policy-seeds 2 --no-neow --hp 10000 --stop victory --verify-snapshots --require-complete` | `1 identical, 0 stopped at unported content, 0 diverged; 1168/1168 decisions matched` |
| `DOTNET=~/.dotnet/dotnet python3 tests/l4/l4_run.py --seeds SEED0002,SEED0003 --policy-seeds 1,2 --no-neow --hp 10000 --stop victory --verify-snapshots --require-complete` | 临时列表优化后：`4 runs: 3 identical, 1 stopped at unported content, 0 diverged; 3517/3520 decisions matched`；剩余 The Architect 入口，命令按预期退出 1 |
| `tools/bench/env_api_bench.cpp` 本机同程序、同根，9×1000 次 | Architect 状态字段加入前，`sizeof(GameState)=343592` 字节；扩大 4096 固定数组的 `Predict(EndTurn)` 平均 589180ns、Preview trace-on 615080ns；512 内联加溢出后的 `Predict` 平均 174774ns、Preview trace-on 200780ns，源 Hash/后继 Hash 保持一致。该次本机对照不是 M5 性能验收 |
| 完整集成后的独立复测，同程序同根，9×1000 次 | `sizeof(GameState)=343600` 字节；Clone 67356ns、Hash 131742ns、Predict 2258704ns、Preview trace-on 2538848ns，源 Hash/后继 Hash 仍保持一致。Clone/Hash 也同步显著变慢，当前数据不足以归因于代码或机器状态；需要受控复测和单独的 M5 性能验收 |
| `scripts/check.sh` | 通过：16/16 CTest、L2 200 局、L3 80 场/1611 状态、默认 L4 80 局/4089 决策、严格二幕 3246 决策、既有胜利 1138 决策、新四路线 3520 决策、Pael's Wing 219 决策 |

## 限制

23 槽耗尽和临时监听器 512 上限仅由这个高生命值压力路线暴露；本批聚焦的逻辑回收不等于所有池都无容量边界。高生命值是可达内容与长战斗压力测试条件，不等于正常血量胜利覆盖。后一处 The Architect 终局事件已另批接入，四条扩展路线最终 3520/3520 决策对齐。完整 Env、真实档案、RF API 与规模性能门槛仍未完成。CombatSolver 保持冻结。
