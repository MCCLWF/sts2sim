# CombatSolver 与 sts2sim Env 双求解器对拍报告

日期：2026-09-26  
分支：`feat/all-encounters-comparison-mod`
游戏基线：v0.111.0；CombatSolver 子模块提交：`d231e9e51a0e58d6bfa1373c6265cce13ffd9a45`

## 结论

CombatSolver 与 C++ Env 已在隔离正版游戏进程中同时搜索，并通过 Mod 输出根状态字段、首动作和两边耗时。新加的无搜索根状态预检会在开始 Env 搜索前拒绝不匹配的重建根；通过预检后，检查和搜索复用同一个 Env 实例。

本轮一次真实游戏测试通过：Cultists Normal、铁甲战士、A0、首回合根字段匹配。Env 100 次/深度 12 用时 108.911 ms；CombatSolver 用时 280.522 ms、展开 3 个节点；首动作分别为 Bash 与 Strike。其余 12 个离线同种子样例的核心根字段均匹配。动作一致率随预算和遭遇变化。

这些样例不能证明完整模拟无偏差或存在统一提速。当前结构核对没有覆盖完整卡牌/遗物/怪物/RNG/历史状态；两边搜索算法和预算单位也不同。低预算样例里 Env 用时较短，高预算 Cultists 样例里 CombatSolver 较短。

## 接入内容

### Env 根状态预检

新增 C ABI `sts2_env_inspect_combat_named`：按 seed、角色、飞升和遭遇建立指定战斗，只返回结构化根与状态 hash，不启动搜索。Python Env 包装也暴露 `inspect_combat_named()`；调用后可直接在同一个 Env 上继续搜索。

游戏 Mod 在后台先调用该探针，对比手牌、抽牌/弃牌/消耗堆顺序、遗物、药水、能力、玩家 HP/格挡/能量及敌人身份/HP/格挡/能力。根不匹配时记录字段名并跳过 Env 搜索；匹配时才在同一个 C++ Env 句柄上启动 ISMCTS。搜索期间实机状态变化时，Mod 不输出双求解耗时比。

### 游戏内双搜索 Mod

`tools/Sts2EnvCompare` 监听单人铁甲战士的玩家回合开始，在主线程捕获稳定的 live 结构字段。原生 CombatSolver 在自己的搜索入口运行；Mod 在工作线程中运行 C++ Env。Mod 只读 CombatSolver 完成结果，不部署或改变任何求解器动作。日志包含预检差异字段、搜索耗时、Env rollout 数、原生 expanded node 数、首动作和根检查边界。

### 离线并行 runner

`tools/dual_solver/compare.py` 同时启动 CombatSolver `OfflineSearchHarness` 与 `sts2sim_env_solver`，记录求解时长、进程时长、核心根字段、首动作、rollout 数和 expanded node 数。进程启动开销与 solver 用时分开记录。

## 离线对拍与性能

CombatSolver 固定为 Low 配置，Env 用固定 rollout 次数/深度。两边每个样例都由同一个 seed、角色、飞升和遭遇分别重建开局，并行求解。`root=MATCH` 仅对比双方导出的核心字段：手牌、玩家 HP/能量、敌人 ID/种类/HP/max HP；它不是完整状态相等证明。`native/env` 是 CombatSolver 搜索毫秒数除以 Env 搜索毫秒数，大于 1 表示 Env 这次耗时较少。

### Low：CombatSolver 300 ms 配置，Env 500 次 rollout / 深度 12

| 遭遇 | seed | 核心根字段 | 首动作 | Env ms | CombatSolver ms | native/env |
|---|---|---:|---:|---:|---:|---:|
| CULTISTS_NORMAL | DUAL001 | MATCH | DIFF | 326.837 | 434.564 | 1.330 |
| CULTISTS_NORMAL | DUAL002 | MATCH | DIFF | 339.471 | 409.569 | 1.206 |
| CULTISTS_NORMAL | DUAL003 | MATCH | MATCH | 328.896 | 403.408 | 1.227 |
| GLOBE_HEAD_NORMAL | DUAL101 | MATCH | DIFF | 346.924 | 401.883 | 1.158 |
| GLOBE_HEAD_NORMAL | DUAL102 | MATCH | DIFF | 288.915 | 399.976 | 1.384 |
| GLOBE_HEAD_NORMAL | DUAL103 | MATCH | DIFF | 303.854 | 442.301 | 1.456 |
| AXEBOTS_NORMAL | DUAL201 | MATCH | DIFF | 315.404 | 424.623 | 1.346 |
| AXEBOTS_NORMAL | DUAL202 | MATCH | DIFF | 314.359 | 429.759 | 1.367 |
| AXEBOTS_NORMAL | DUAL203 | MATCH | DIFF | 311.450 | 423.882 | 1.361 |

9/9 核心根字段匹配，首动作 1/9 一致。Env 搜索用时中位数 315.404 ms，CombatSolver 423.882 ms，中位 `native/env=1.346`。即 Env 本组耗时约为 CombatSolver 的 74%；两边的搜索量与路线质量没有对齐，不能据此宣称同质量提速。

### 较高预算：CombatSolver 500 ms 配置，Env 1000 次 rollout / 深度 16

| 遭遇 | seed | 核心根字段 | 首动作 | Env ms | CombatSolver ms | native/env |
|---|---|---:|---:|---:|---:|---:|
| CULTISTS_NORMAL | DUAL004 | MATCH | MATCH | 862.353 | 620.577 | 0.720 |
| CULTISTS_NORMAL | DUAL005 | MATCH | MATCH | 849.358 | 612.800 | 0.721 |
| CULTISTS_NORMAL | DUAL006 | MATCH | DIFF | 836.300 | 650.477 | 0.778 |

3/3 核心根字段匹配，首动作 2/3 一致。Env 搜索用时中位数 849.358 ms，CombatSolver 620.577 ms，中位 `native/env=0.721`；此配置下 CombatSolver 耗时较少。总体结果随遭遇和预算变化，没有稳定的单一胜者。

### 隔离正版游戏内测试

使用 `tools/run-unattended-test.sh` 启动隔离正版游戏；实例加载 CombatSolver、RitsuLib 与更新后的 `Sts2EnvCompare`/`libsts2env_c.so`。场景为 `MCC-DUAL-PREFLIGHT`，Ironclad / Cultists Normal / seed `DUAL001` / A0。无人测试状态 `Passed`，战斗在第 1 回合结束。日志：

```text
[Sts2EnvCompare] ENV turn=1 encounter=CULTISTS_NORMAL structural_root_match=True root_mismatch_fields= live_unchanged=True iterations=100 depth=12 elapsed_ms=108.911 action=PlayCard:BASH:target=1
[Sts2EnvCompare] TWO_SOLVERS turn=1 encounter=CULTISTS_NORMAL env_ms=108.911 native_ms=280.522 native_over_env=2.576 env_rollouts=100 native_expanded=3 env_action=PlayCard:BASH:target=1 native_action=PlayCard:STRIKE_IRONCLAD:target=1 action_agree=False root_check=structural_only budgets=not_equivalent
```

这个测试证明 C# Mod 能加载新增 C ABI，能在原生搜索运行时检查根并完成 Env 搜索。两边首动作不同是策略差异信号；只有执行相同动作并比较后继状态，才能判断动作效果是否存在模拟偏差。

## 复现命令

路径通过本机游戏与 Steam workshop 目录设置，避免将个人绝对路径写入项目：

```bash
GAME_ROOT=/path/to/Slay\ the\ Spire\ 2
RITSU_ROOT=/path/to/steamapps/workshop/content/2868840/3747602295

cmake --build build -j4 --target sts2env_c sts2sim_env_solver
dotnet build tools/Sts2EnvCompare/Sts2EnvCompare.csproj -c Release \
  -p:Sts2Dir="$GAME_ROOT" -p:CopyModOnBuild=true
dotnet build external/CombatSolver/tools/OfflineSearchHarness/OfflineSearchHarness.csproj -c Release \
  -p:Sts2Dir="$GAME_ROOT" -p:Sts2DataDir="$GAME_ROOT/data_sts2_linuxbsd_x86_64" \
  -p:RitsuWorkshopRoot="$RITSU_ROOT"

python3 tools/dual_solver/compare.py \
  --native-harness external/CombatSolver/tools/OfflineSearchHarness/bin/Release/net9.0/OfflineSearchHarness.dll \
  --seeds DUAL001,DUAL002,DUAL003 --encounter CULTISTS_NORMAL \
  --native-budget-ms 300 --env-iterations 500 --env-rollout-depth 12 \
  --out /tmp/dual-solver.json

external/CombatSolver/tools/run-unattended-test.sh \
  --scenario-id MCC-DUAL-PREFLIGHT --character-id IRONCLAD \
  --encounter-id CULTISTS_NORMAL --seed DUAL001 --ascension 0 \
  --act-index-for-test 0 --force-short-search-only \
  --short-search-budget-override-milliseconds 1500 \
  --sts2-game-root "$GAME_ROOT" --ritsu-workshop-root "$RITSU_ROOT" \
  --headless-instance mcc-envcompare-20260926
```

## 验证

- `cmake --build build -j4 --target sts2env_c test_env sts2sim_env_solver`：通过。
- `scripts/check.sh --quick`：8/8 单元测试通过；冒烟为 `death=94 unported=6`、`death=100`、`boss=3 death=97`；退出码 0。
- `STS2SIM_ENV_LIB=$PWD/build/libsts2env_c.so python3 python/test_env.py`：通过；覆盖新根探针和同句柄续接搜索。
- `dotnet build tools/Sts2EnvCompare/Sts2EnvCompare.csproj -c Release --no-restore`：0 警告、0 错误。
- CombatSolver `OfflineSearchHarness` Release 构建：0 警告、0 错误。
- 上述 12 个并行离线样例：核心根字段 12/12 匹配。
- 隔离正版游戏 Mod 测试：`Passed`，并记录到 `ENV` 与 `TWO_SOLVERS` 两行。

## 尚未覆盖

- 根探针重建的是新局的指定遭遇，不是任意真实跑局状态。玩家牌堆的卡牌实例 UID、升级/附魔/费用修改、遗物内部计数、怪物意图/状态机细节、RNG 流状态和战斗历史都未完整导入。进入后续回合时若根字段不匹配，Mod 会跳过 Env 搜索；不能将跳过记录解释为偏差为零。
- 游戏内根核对是结构子集；离线 runner 比较的字段更少。完整性需要状态导入或从战斗开局精确重放，再逐决策点比较状态。
- 首动作只比较卡牌 ID 与目标 combat ID，无法区分同名重复卡牌的实例状态。策略不同导致动作不同，不直接表示模拟语义不一致。
- ISMCTS rollout 与 Beam Search expanded node 不是同一种工作量。固定 rollout 与毫秒预算也不等于相同解质量；上述时间只能作为本机原始观察。
- 这次游戏内验证使用 headless 隔离实例。没有做可见窗口布局验收、任意 live root 导入或动作后继状态差分。
