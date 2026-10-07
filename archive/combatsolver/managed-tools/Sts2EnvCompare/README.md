# 战士双求解器比较 Mod（开发版）

> **冻结档案（2026-09-27）**：本项目及依赖其旧 SDK 的工具被 `tools/Directory.Build.targets` 禁止构建，下面的命令和专用 ABI 描述是历史记录，不适用于当前 Env。旧 C++ 预测与 Beam 已归档到 `archive/combatsolver`。当前开发以通用 Env 和 Random Foreseer API 为主线；除用户重新明确要求外不恢复。见 [通用 Env 完成路线](../../docs/plans/2026-09-27-generic-env-completion.md)。


独立派生版构建见 [`../CombatSolverEnv/README.md`](../CombatSolverEnv/README.md)。派生版已包含原生 CombatSolver 和此桥接，不能与这两个独立 Mod 同时启用。

此目录是可由《杀戮尖塔 2》加载的独立 Mod。依赖固定在 `external/CombatSolver` 的 CombatSolver 和本仓库的 C++ `sts2env_c`。只在单人战士的玩家回合运行；游戏仍由 CombatSolver 控制行动，本 Mod 只读取搜索结果和显示对照。

## 构建与安装

需要游戏 v0.111.0、.NET 9 SDK、CMake 和匹配版本的 RitsuLib。示例中 `STS2_GAME_DIR` 指向游戏目录，`STEAM_ROOT` 指向 Steam 数据目录：

```bash
git submodule update --init external/CombatSolver
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target sts2env_c -j 4

dotnet build external/CombatSolver/CombatSolver.csproj -c Release \
  -p:SteamRoot="$STEAM_ROOT" -p:Sts2Dir="$STS2_GAME_DIR" \
  -p:RitsuWorkshopRoot="$STEAM_ROOT/steamapps/workshop/content/2868840/3747602295" \
  -p:RitsuLibDir="$STEAM_ROOT/steamapps/workshop/content/2868840/3747602295/lib/0.111.0" \
  -p:CopyModOnBuild=true

dotnet build tools/Sts2EnvCompare/Sts2EnvCompare.csproj -c Release \
  -p:Sts2Dir="$STS2_GAME_DIR" -p:CopyModOnBuild=true
```

最后一条命令把 `Sts2EnvCompare.dll`、清单和 `libsts2env_c.so` 放在 `mods/Sts2EnvCompare/`。运行时在游戏日志查找 `[Sts2EnvCompare] READY`、`ENV`、`TWO_SOLVERS`。可用 `STS2ENV_COMPARE_ITERATIONS`（默认 100）作为节点预算、`STS2ENV_COMPARE_BEAM_WIDTH`（默认 8）和 `STS2ENV_COMPARE_DEPTH`（默认 12）调整 Env Beam 搜索量；`STS2ENV_COMPARE_MODE` 可设为 `env`、`native` 或 `both`（默认）。`env` 模式会安全暂停原生搜索，保留 Env Beam 计划，逐个动作执行前重新核对 live root，并用持久 Env session 预测下一状态；遇到根不一致、预测失败或游戏动作拒绝时停止计划并恢复原生搜索。暂停/恢复入口不可用时直接回退并显示原因。

## 当前比较边界

目前 C++ Env 通过相同种子、角色、进度和遭遇重建战斗。Mod 先调用无搜索的根状态探针，对照手牌、抽/弃/消耗堆的卡牌 ID 顺序及每张牌的升级等级、局部费用、附魔/苦难 ID 与数量，并比较遗物、药水、能力、生命值、格挡、能量及敌人身份/生命/格挡/能力；字段不匹配时会列出字段名并跳过 Env 搜索。字段匹配后，Mod 在同一个 Env 实例上继续求解，避免检查和搜索使用不同根状态。实机状态在求解结束前变化也会跳过性能/动作比较。

这仍是结构子集核对，不是任意实机状态的完整导入或完整同根证明。当前没有同步卡牌实例 UID、所有费用修饰与动态变量、遗物内部计数、怪物意图/状态机、RNG 状态及战斗历史；战斗推进后按初始 seed 重建的根通常会被预检拒绝，所以后续回合不会得到 Env 同根搜索结果。计划执行只覆盖当前回合的 `PlayCard` 与 `EndTurn`，并在每步检查 live root；其他动作会安全停止。要覆盖任意回合，需要实现这些字段的 live 状态导入或从开战起精确回放动作。

`TWO_SOLVERS` 日志输出两个搜索的原始耗时、节点量及首动作，并把 CombatSolver 实际选出的当前回合 `PlanAction` 路线逐步送入 Env session 做只读转移检查（`native_plan_env_check`）。Env 使用确定性 Beam 展开和固定宽度保留；CombatSolver 仍使用自己的完整 Beam 策略。两边共享 Beam 的高层搜索形状，但评价、候选生成、终局排序和预算定义尚未完全相同，所以不把工作量或耗时当作等价性能。只有结构字段匹配、实机状态在搜索期间保持不变且 CombatSolver 本回合搜索完成时才输出双求解比较。搜索前会保守检查当前 Env 根中的遭遇、牌、附魔、苦难、遗物、药水、怪物、能力和充能球是否完整移植；搜索过程中碰到后来生成的未移植内容也会停。任一处遇到 `Unported` 时记录 `ENV_UNPORTED`，停止 Env 搜索并在游戏内明确显示不使用。计划执行失败会停止剩余动作并恢复原生。首动作比较目前使用卡牌 ID 与目标 combat ID；同名重复牌的实例状态还不能区分。完整 live 根导入、CombatSolver 侧完整策略复用、跨回合 Env 路线部署、等预算基准及复杂状态对拍仍在开发中。

## Env 预测接口

`sts2_env_inspect_current` 只读当前 Env 状态；`sts2_env_validate_combat_root` 在搜索前拒绝当前根里未移植的模型；`sts2_env_try_transition` 从当前根复制一个分支并执行一个合法动作，成功时返回由调用方销毁的后继句柄。源句柄在成功、非法动作和未移植失败时均不改变。返回码：`0` 成功，`1` 到达未移植内容，`-1` 其他错误；失败时后继句柄为空，用源句柄的 `sts2_env_last_error` 获取原因。Python 包装为 `inspect_current()`、`validate_combat_root()` 与 `predict(action)`，未移植失败抛出 `UnportedError`。根检查只涵盖当下已存在的模型，不能证明未来生成的内容均已移植。这是后续把相同 Beam 算法接到 Env 的分支预测入口；它目前不导入任意实机状态，也不替代 CombatSolver 的现有搜索。

Mod 内的 `NativeEnv.OpenCombat()` 还提供持久 session：`Session.InspectJson()` 读取当前分支，`Session.Predict(EnvAction)` 返回新的独立 session，源 session 继续可用；`EnvPredictionUnportedException` 与普通异常分开。每次 Env 搜索完成后，开发版 Mod 会用同一首动作验证一次“源不变、后继可读”的边界。隔离场景 `MCC-DUAL-FUTURE-SESSION-20260926` 已通过。
