# 通用 Env 主线：原版回退、预览、恢复与开发进度

日期：2026-09-27。分支：`feat/generic-env-rf-api`，开始于 `84d59b6`。游戏基线 v0.111.0。计划：[原版预览入口与冻结](../plans/2026-09-27-env-vanilla-preview.md)。

## 1. 当前判断

**继续开发 Env 有意义；当前应集中补齐通用 Env 与 Random Foreseer 所需 API。CombatSolver 历史内容冻结，不因 Env 完成自动恢复。**

RNG、Decimal、容器顺序、Cmd/Hook/FSM、战士内容和整局骨架已有实际差分证据，属于可用基础。当前的主要缺口是其余角色内容、真实档案、全部条件分支、后两幕端到端、局外操作/结果、完整追踪和真实游戏捕获。这些缺口会同时阻塞预览和搜索；追逐旧 Beam 字段不能解决它们。

正确方向是内容与公共 API 同步按功能补齐。每批贯通“原版操作 → 独立分支 → 选择/结果 → 恢复 → 差分”，以 RF 功能矩阵验收。客户端使用通用 API 构建展示或搜索，费用/伤害/随机性等原版规则归 Env；评分、剪枝、风险偏好、公平展示和 UI 归客户端。已验收版本和单人范围内，未来移植不应为某个算法更改 Env。

## 2. 本批已落实的架构调整

### 冻结与归档

- 旧 `CombatPredictionSession`、`sts2_combat_prediction_*`、专用终局旁路头文件与测试移入 `archive/combatsolver`。
- 旧参考 Beam/solver CLI 移出当前构建；`sts2_env_solve_*` 全部移出普通 ABI，Python 删除相应绑定。普通 M3 ISMCTS 保留在 `search/`，作为 Env 外部消费者。
- `tools/Directory.Build.targets` 禁止派生 Mod 和依赖其旧 SDK 的七个 managed 项目构建。旧内容留作历史研究资料，不能影响当前门禁。
- 这是明确的 ABI 变更。冻结消费者不与当前库兼容；新的独立通用 C# SDK 尚未建立。

### 回退不当规则分支

- `CheckWinCondition` 恢复普通胜负处理；移除预测跳过战后 Hook、回复、清理和奖励的条件。
- `EndEnemyTurn` 删除专用 terminal 提前返回。
- `RunFlow::AfterCombatAction` 恢复普通奖励/结束局流程。
- `DamageReceived` 保持原版 `IsInProgress && !IsEnding` 历史记录条件，不因观察追踪而放宽。
- 普通药水路径按原版 ActionExecutor 执行 `AfterPlayerAction` 安全胜负检查；此前此检查只在预测 scope 下调用，是普通路径的缺口。

核对源为 `CombatManager.cs`（CheckWinCondition / EndEnemyTurn）、`ActionExecutor.cs`、`CreatureCmd.cs`、`CombatRoom.cs` 与 `NCombatUi.cs`；没有以 CombatSolver 规则替代原版真值。

### 通用 API

- `PreviewAction` / `sts2_env_preview` / Python `preview`：普通 owned Env 后继，结果与正常执行一致，可关闭追踪，原根不变。Choice 后继可继续 Answer；重放结果是整段待处理操作前缀，客户端替换先前部分追踪。
- `ExecutionTrace` 只观察事实，不保存搜索终局、风险推断或六个 Beam 计数；添加帧父子关系、全序 sequence 和容量错误。
- `DamageResolved` 独立保存实际伤害结果，致死命中也可读取；没有将它写入原版 History 来影响游戏计数。
- 原版查询迁为普通 Env 上的四种 getter 接口。保留 CalculatedVar 原版 base/extra/multiplier 公式操作数；这些是游戏数值语义，不是搜索评分。
- v2 logical snapshot 从战斗限定扩展到已定义的整局决策和选择前缀；保留 schema/runtime/RNG 顺序、字段/引用、hash 及重放检查。活跃战斗中的选择也进行保守模型覆盖检查。
- API 用法与限制：[公共预览文档](../env-preview-api.md)。

## 3. 内容开发的真实进度

本批内容由 GPT-6 Luna xhigh 承担：Prepared、EscapePlan、Adrenaline、Sidestep、Expose，及 Sidestep 所需 EnergyNextTurnPower；PaperKrane 与 WeakPower 原版倍率调用。有限定向差分：五卡 80/80 场、1170/1170 状态；PaperKrane 主矩阵与补充合计 24/24 场、407/407 状态，另有 16 个非致死前缀、48/48 状态。均零分歧、零 Unported，保留实际出牌/能力/条件覆盖轨迹。纯生产及真机未验收，具体边界以各批报告为准：[静默小批](2026-09-27-env-silent-simple-cards.md)、[PaperKrane](2026-09-27-env-paper-krane.md)。

静态库存由 `python3 tools/coverage/env_inventory.py` 读取正式模型，排除 Mock；具体最终数量在第 5 节。头文件与 IsUsable 不能证明条件分支或完整行为：PaperKrane 此前就在可用名单中，但 WeakPower 到该条件时仍会 Unported。本批补齐该路径，静态遗物 no-stub 数量不会因此增加。

角色卡池的静态头文件范围：Ironclad 90/90、Silent 26/91、Defect 31/91、Regent 8/91、Necrobinder 10/91。铁甲战士最完整，但历史全卡矩阵与当前第一幕回归都不能代替所有组合和完整三幕验收。

## 4. CombatSolver 的真实进度与冻结理由

冻结前的成果是设置/UI、部分捕获映射、原版 getter/公式、部分状态键/历史投影以及有限 replay 验证。正式 Env 执行门禁关闭；实际完整搜索仍由原 Native 求解器执行，再把部分结果送到 Env 核对。完整搜索扩展、原状态/历史键、所有动态对象/选择和执行部署未完成，也没有等预算性能胜利的证据。

因此不能说“已经把核心完全换成 Env”。有价值的普通 getter/状态/事实能力已迁回通用接口；旧求解器的策略资料冻结。重新启动该移植需要用户后续明确要求。

## 5. 验收

- 含 EnergyNextTurnPower 的最终 Release 构建和完整 gate 已通过。`scripts/check.sh > /tmp/mcc-env-full-check-integrated.log 2>&1` 退出 0。
- CTest 当前 10/10，包括新 `env_preview`、完整字段 snapshot、Python 与外部 ISMCTS 消费者。
- 新预览夹具检查源隔离、普通/追踪/禁追踪后继一致、容量失败、实际致死事实、BurningBlood 胜利回复和奖励、药水胜利、Discovery 前缀、战后卡牌奖励选择、Smith 选择、各房间与自然死亡结束恢复。它们是公共合同回归，不自称正版差分。
- 公共文档中的 Python Preview/restore 示例实际运行通过；旧 build 目录里的 solver/test_combat_prediction 遗留可执行文件已清除。
- `nm -D --defined-only build/libsts2env_c.so` 的 C ABI 名单不再有 `sts2_combat_prediction_*` 或 `sts2_env_solve_*`；静态 Env archive 仅含 env/snapshot/preview/game_queries。
- 冻结项目构建验证：`dotnet build tools/CombatSolverEnv -p:CopyModOnBuild=false -c Release` 退出 1，错误来自明确的冻结 target；未部署到游戏。

### 最终全量回归原始摘要

```text
100% tests passed, 0 tests failed out of 10
smoke: death=94 unported=6
smoke: death=100
smoke: boss=3 death=97
L2 newrun: 200 runs, 0 mismatched
L3 combat: 40/40 combats identical, 0 unported, 0 diverged, 784/784 states matched
L3 combat: 40/40 combats identical, 0 unported, 0 diverged, 827/827 states matched
L4 run: 40 runs: 40 identical, 0 stopped at unported content, 0 diverged; 1958/1958 decisions matched
L4 run: 40 runs: 40 identical, 0 stopped at unported content, 0 diverged; 2131/2131 decisions matched
all checks passed
```

L2 是五角色开局；默认 L3 是战士首层；默认 L4 止于第一幕 Boss 前，部分局更早战死。协议对拍覆盖输出决策字段/RNG 计数，不是每步完整 POD/所有隐藏字段证明。smoke 有未实现中止，完整 Env 门禁未通过。headless Oracle 仍使用 TestMode 与已登记生产分支/表现层适配，不能称为所有路径的纯生产或真机验收。

### 最终静态库存

| 模型 | 正式 ID | 具体头文件 | IsUsable 静态名单 |
|---|---:|---:|---:|
| Card | 596 | 252 | 250 |
| Power | 265 | 140 | 147 |
| Monster | 120 | 110 | 110 |
| Relic | 299 | 182 | 179 |
| Potion | 65 | 55 | 56 |
| Enchantment | 23 | 2 | 18 |
| Affliction | 7 | 5 | 7 |
| Orb | 5 | 5 | 5 |
| Modifier | 17 | 0 | 6 |
| Encounter | 90 | 90 | 87 |
| Event | 59 | 49 | 35 |
| Ancient | 9 | 1 | 1 |

角色卡池 no-stub 也按 generator 的 `id_enum_name` 命名映射核对，分别 90、26、31、8、10；名称缩写不能直接去掉 `CARD.` 来计数。这些均为静态名单，不是行为完成百分比。

### 性能与日志

新增纯 API 基准 `tools/bench/env_api_bench.cpp`，测固定根的 Clone、Hash、EndTurn Predict、追踪开启/关闭 Preview 和快照。每项包含结果销毁；计时内部只读少量 POD 字段防止优化，不额外调用 Hash。独立一致性校验在计时外。本机平均值（含销毁，固定 Cultists 根）：Clone **2.91µs**，Hash **10.31µs**，EndTurn Predict **109.70µs**，Preview trace 开 **121.22µs**、关 **121.92µs**，完整快照输出 **130.46µs**。Trace 开关差异在本次采样条件下不能作因果结论。数字与环境以 [基线报告](2026-09-27-env-api-baseline.md) 为准；这不是搜索或真机性能比较，Clone <1µs 目标未达成。

原始日志在本机 `/tmp`，不提交大体积 trace。内容差分的最终场数/条件覆盖以各批报告为准。

## 6. 当前仍未完成的目标

1. **覆盖与真值**：方法/条件路径清单、逐项 TestMode/表现层审计、全角色/全部房间/强制后两幕完整轨迹；不能只依赖允许 Unported 的回归脚本。
2. **内容与档案**：其他角色、跨职业生成、先古/事件、附件/修饰器和真实部分解锁/初次运行规则。
3. **RF 操作合同**：变牌、遗物拾取、Courier 补货、Driftwood 重掷、PaelsWing 献祭、休息与事件隐藏位置的明确预览操作；当前普通合法动作并不能表达所有悬停预览上下文。
4. **结果和停止**：当前只停于普通决策/选择/结束；其他停止边界、来源实例、完整 Hook/RNG/局外因果与结果差分仍待补。当前 JSON 明确 `trace_complete=false`。
5. **真实捕获与 SDK**：当前 round-trip 验证的是模拟器自身的已支持边界；任意真实游戏房间捕获、独立 C# SDK 与跨进程持久协议未完成。
6. **规模与性能**：GameState 的 145936 字节显著大于设计早期目标；局部基准不等于实机吞吐、1 万条完整性或搜索速度验收。

下一批先完善 E0 覆盖和可复现基准，E1/E2 按 RF 的抽牌、变牌、遗物拾取、商店补货逐条贯通，E3 内容每批随同验证。完整阶段门槛见 [通用 Env 完成路线](../plans/2026-09-27-generic-env-completion.md)。本批没有将完整 Env / RF 目标标为完成。
