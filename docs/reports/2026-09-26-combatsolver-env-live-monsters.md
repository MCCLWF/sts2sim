# CombatSolver Env：真实怪物 RNG 与状态机组件

日期：2026-09-26；分支：`feat/combatsolver-env-backend`；计划：[M5 后端接入](../plans/2026-09-26-m5-combatsolver-env-backend.md)。

## 前置条件与范围

完整 Env 逻辑快照已能严格导出/恢复；真实卡牌组件已开始冻结。生产诊断仍从种子重建根，尚未完整导入 live 字段。本阶段冻结真实怪物组件并增加根级核对，未完成同算法 Env 执行后端或用户要求的成品。

`EnvLiveMonsters` 在主线程 before/after 稳定性边界内捕获每个实例的 RNG counter/四个状态字、performing/spawned 标志、FSM 初始/当前状态、NextMove、首次行动、每个招式运行标志、动态 follow-up、行动历史及已适配扩展字段。组件拥有独立 JSON，不向 worker 传入 live 对象/委托。Cultists 的 `_attackSfxStrength` 只影响音效，按原版调用核实后排除；Toadpole `_isFront` 映射到 extra[0]。其他内部字段、动态变量和未知条件/动态计算器拒绝。

新增 C ABI `sts2_env_export_monster_machines` 只读取当前 Env 的不可变定义，返回状态名、动作回调名、意图、分支、runtime 身份和源 hash；不生成状态机或执行动作。`SIM_MOVE` 的名字保存在注册表，POD 状态结构不变。公开 `InspectMonsterMachines` 返回独立 JSON；注册索引只能在该 runtime 内使用。绑定逐项核对定义与顺序，再按状态名解析当前状态、NextMove、历史和 follow-up。

常量意图/权重采用严格 IL 形状读取原版标量闭包，不调用委托。原版 int -> decimal 使用 `call decimal.op_Implicit(Int32)`；初次工具因此拒绝，检查实际 IL 后补齐精确识别。条件适配仅接受 Toadpole INIT 的两个准确原版回调身份，交换回调也拒绝。动态伤害/重复/权重及 stun 待适配。

## 验证命令与结果

### 正版组件与 owned API

```bash
~/.dotnet/dotnet run --project tools/EnvLiveCardChecks -c Release
~/.dotnet/dotnet run --project tools/EnvSessionChecks -c Release
~/.dotnet/dotnet build tools/CombatSolverEnv/CombatSolverEnv.csproj -c Release
```

均 exit 0；派生 Mod 构建 0 Warning(s)、0 Error(s)。日志分别为 `/tmp/mcc-monster-component-check-final.log`、`/tmp/mcc-monster-session-check-final.log`、`/tmp/mcc-monster-mod-build-final.log`。

组件检查：

```text
live monster component: native FSM transition APIs, complete RNG, runtime name binding, follow-up/flag restore, pure prediction and explicit definition/stun rejection passed; combat_effects_not_compared full_root=false
live monster conditional branches: native front/back Toadpole transitions and multi-attack definition passed; combat_effects_not_compared full_root=false
live monster random branches: vanilla scalar weights, repeat rules and three native RNG/FSM transitions passed; combat_effects_not_compared full_root=false
```

预期来自正版 `OnMovePerformed`/`RollMove`/`OnSideSwitch`，SludgeSpinner 还逐步核对完整 MonsterAi 流。混合夹具明确由种子提供其余字段，只导入怪物组件；修改 follow-up、运行标志与个人 RNG 后恢复，继续 Predict 并核对源 hash 不变。卡牌检查继续通过。工具没有执行原版攻击/能力效果，不能把这些结果称为完整战斗对拍。

managed 检查还在并行源访问中导出定义并核对 runtime/hash；源 Dispose 后返回 JSON 仍可读，状态/回调名完整。

### CreateCreature 负坐标修复

检查正版 `CombatState.CreateCreature` 发现公式是 `CurrentMapCoord?.col/row ?? 0`。C++ 原来按 `< 0` 判断缺失，错误地抹掉存在的负坐标。改为判断坐标容器是否为空，保留已有坐标数值。没有补抽 RNG。

工具从正版创建第一只敌人（DUAL001、A0、combat id 1）采集 null、(0,-1)、(-3,5) 的 counter 与四个状态字；`tests/test_env.cpp` 使用这些原始向量验证 C++ 创建结果。完整日志保留于组件检查文件。这项修复与实机种子模板不匹配分别验证，不将其当作实机根差异的已证明首因。

### C++ 完整回归

```bash
scripts/check.sh
```

最后一次 exit 0；日志 `/tmp/mcc-monster-full-check-final.log`：

```text
100% tests passed, 0 tests failed out of 9
L2 newrun: 200 runs, 0 mismatched
L3 combat: 40/40 combats identical, 0 unported, 0 diverged, 784/784 states matched
L3 combat: 40/40 combats identical, 0 unported, 0 diverged, 827/827 states matched
L4 run: 40 runs: 40 identical, 0 stopped at unported content, 0 diverged; 1958/1958 decisions matched
L4 run: 40 runs: 40 identical, 0 stopped at unported content, 0 diverged; 2131/2131 decisions matched
all checks passed
```

### 隔离正版游戏：接受失败、明确拒绝成功

```bash
python3 tools/CombatSolverEnv/run_smoke.py --mode both --verify-branches \
  --memory-reservation-mib 2048 --out /tmp/mcc-env-livemonsters-game-20260926
```

游戏场景 Passed 并清理实例；**分支接受断言失败**。新检查发现真实个人 RNG 与种子模板不同，已停止这次根的 Env 分支验证：

```text
LIVE_CARD_COMPONENT cards=20 source=live immutable=true full_root=false
LIVE_MONSTER_COMPONENT monsters=2 source=live scope=monster_rng_and_fsm immutable=true full_root=false
BRANCH_ENV_DISABLED turn=1 action_count=0 reason=unavailable:root_monster_runtime_mismatch:1:monster.rng.s0:live=15101400006774947034,env=6180656293325301155 env_used=false scope=structural_and_combat_rng root_source=seed_snapshot_import native_search=continues
ENV_BRANCH_WARNING shown=true text=Env 分支核对已停止；当前由 Native 执行动作。
BRANCH_ENV_SUMMARY native_expanded=100 native_transitions=335 env_checked_transitions_root_cumulative=0 env_disabled=True
```

采样峰值 RSS 1,469,324 KiB，低于 2048 MiB 声明。日志/launcher/memory 保留在上述目录；该实机证据采集于本阶段负坐标修复前的构建，最终修改通过独立原版向量及完整回归验证。

新增单独拒绝 fixture，并只检查已保留材料，不启动第二次游戏：

```bash
python3 tools/CombatSolverEnv/run_smoke.py --verify-only --expect-monster-root-rejection \
  --out /tmp/mcc-env-livemonsters-game-20260926
```

exit 0，确认 UI 提示、拒绝发生在首次 Env 分支转移前且 Native 完成 100 节点搜索。这项通过不替代 `--verify-branches` 接受门禁；接受门禁仍失败。旧结构/九条战斗流子集没有检测到这个差异，不能用旧成功扩大保真结论。

## 未完成条件

- 完整人物/能力/遗物/药水、全 RNG、跑局上下文、历史与注册顺序映射，以及其他怪物内部数据、动态定义和 stun。
- 从实际完整 frozen root 组装生产 Env；不保留未知种子字段，不放宽个人 RNG 核对。
- 所有 native 后继的完整状态核对，包括选择、终局及跨回合；当前怪物核对只在 frozen root。
- Env 接管原 CombatSolver 的全部克隆/转移/准备回合/辅助威胁预测，保持同算法政策；持久设置 UI、公平性能面板、部署及安装包验收。

未修改上游子模块，未合入 main。开发包不属于最终交付；目标保持进行中。
