# 通用 Env 与 CombatSolver 移植：全面进度审计

日期：2026-09-27。审计代码：`a8d0baa`，游戏基线 v0.111.0。审计分支：`docs/generic-env-readiness-audit`。

> 历史审计快照：本文主体记录 `a8d0baa` 时点。后续用户要求已改为冻结 CombatSolver，除用户明确重新要求外不自动恢复。当前分支的原版回退、通用 Preview、整局恢复、内容新增和最新验收见 [Env 开发报告](2026-09-27-env-vanilla-preview.md)。本文旧专用 ABI/测试命令仅作为历史证据，不再是当前可用公共接口。

## 1. 结论与本次决定

**继续开发通用 Env 有意义，应先把 Env 的规则、状态和公共预测合同补齐；CombatSolver 移植现在暂停。**

理由是现有 Env 已有经过正版差分的确定性基础、战士内容、房间流程和 owned 预测能力，可以同时服务搜索、强化学习和随机结果预览。当前阻塞并非单纯“还差几个搜索字段”：全内容、真实档案、完整房间恢复、选择和事实追踪尚不完整。逐项追着原 Beam 填专用投影，会不断被更底层的缺口打断。

用户的新方向可实现：把原版执行、随机性、可继续的选择和结果查询做成通用能力，未来 C# Mod 在公共 API 上实现 Random Foreseer 类提示或 CombatSolver 搜索。评分、剪枝、风险偏好、原 Beam 状态键、UI 和部署留在客户端。

“无需改 Env”必须限定于已验收的游戏版本与支持范围；不能保证游戏更新或任意第三方规则都无需修改。Env 完成以后，CombatSolver 的完整策略适配、产品打包与性能比较仍有工作，不能视为自动完成。

已将主线和恢复门槛写入[通用 Env 完成路线](../plans/2026-09-27-generic-env-completion.md)，在 README、路线图、派生 Mod 说明及两份移植计划中标注暂停。本次没有新增移植实现，没有改动子模块或游戏安装。

## 2. 进度应使用什么口径

不能给整个项目一个可信的“完成百分比”。当前至少有四种不同事实：

1. 游戏内容表存在模型与数值。
2. C++ 存在模型头文件或已登记默认行为。
3. 静态 `IsUsable` 未发现 generator 识别的覆写 stub。
4. 某组正版差分证明某些方法、条件分支与后继正确。

这些层次不等价。`IsUsable` 不能证明模型函数内部没有条件式 `throw Unported`；头文件存在也不能证明完整行为。旧设计和阶段报告的数字具有各自历史口径，以下以当前 `data/content.json`、生成 ID、registry 和实际代码重新核对。

## 3. C++ 内容盘点

### 3.1 总体规模

基线采用生成的正式 ID，排除命名 Mock/`MOCK_*`，包括事件/辅助正式模型；普通运行池可能更小。头文件与 no-stub 计数是**静态库存**，并非已完成比例。

| 类型 | 当前正式基线 | C++ 模型头文件 | 静态 IsUsable/no-stub | 判断 |
|---|---:|---:|---:|---|
| 卡牌 | 596 | 247 | 245 | 战士最完整，其余角色缺大量内容 |
| 遗物 | 299 | 181 | 179 | 共享池已大量接入；先古/角色及条件路径仍缺 |
| 能力 Power | 265 | 139 | 146 | 含无覆写默认可用模型；不可据此说已覆盖所有能力 |
| 药水 | 65 | 55 | 56 | 共享及战士/事件药水较完整，其他内容仍缺 |
| 怪物 | 120 | 110 | 110 | 数字不证明每个招式/条件/生命周期完成 |
| 正式遭遇 | 90 | 90 | 87 | 头文件齐全；三种 Battleworn Dummy 的静态门禁仍为 false |
| 普通事件 | 59 | 49 | 35 | 完整事件、选择和后续路径仍明显不足 |
| 先古事件 | 9 | 1 | 1 | 仅 Neow 已接入；不能算全先古完成 |
| 附魔 Enchantment | 23 | 2 | 18 | 部分效果在基类实现；不能按 no-stub 认定全部效果正确 |
| 苦难 Affliction | 7 | 5 | 7 | 同样存在基类/默认行为，需按 ID 和分支验收 |
| 充能球 Orb | 5 | 5 | 5 | 五种有实现/专项值查询证据，组合和生命周期仍需覆盖 |
| 修饰器 Modifier | 17 | 0 | 6 | 缺具体模型头文件；静态默认可用不是规则完成证明 |

no-stub 可以大于头文件数：部分模型没有待移植的可分发覆写，注册使用默认行为。Power 多出的七个模型只覆写静态/元数据 getter，药水多出的一个是 DeprecatedPotion。确切支持还必须检查实际条件分支、依赖模型、附属状态和对拍证据。

三个 Battleworn Dummy 的 GenerateMonsters 已实现，但 SaveCustomState/LoadCustomState 的字典签名尚未生成，因此静态门禁为 false；有限战斗对拍通过不能替代存取状态验收。附魔具体头文件仅 Glam/Slither；基类有部分 ID 的通用实现及中性默认分支，必须逐项对照原版，不能按静态名单全算完成。

本次新增可重复盘点工具 [`tools/coverage/env_inventory.py`](../../tools/coverage/env_inventory.py)，读取当前内容与生成分发，不运行生成器、不修改内容。文本和 `--json` 模式均可用。它只做 E0 的静态库存，条件路径、真正注册语义和正版覆盖清单仍待建立。

怪物原始导出条目 126 含 6 个 mock；正式模型 120，其中 ModelDb 常规池成员 107。遭遇正式 ID 90，常规 `AllEncounters` 池成员 85，另外 5 个在事件等场景。旧文档的 `109/126` 与当前正式 `110/120` 不能直接算新增完成比例。

另一个生成表边界：原始 ModelDb 的 AllPowers/AllPotions/AllPotionPools 含 mock 引用，而 `gen_content.py` 会跳过包含未生成 ID 的整张列表，当前三张表未生成，尚未发现运行代码调用它们。后续通用选池必须审计这点，不能把“导出了全部数值”理解为每张运行时列表已完整可用。

### 3.2 卡池与通用内容

| 卡池 | 正式卡数 | 头文件 | no-stub |
|---|---:|---:|---:|
| Ironclad | 90 | 90 | 90 |
| Silent | 91 | 21 | 21 |
| Defect | 91 | 31 | 31 |
| Regent | 91 | 8 | 8 |
| Necrobinder | 91 | 10 | 10 |
| Colorless | 65 | 52 | 52 |
| Curse | 18 | 9 | 9 |
| Status | 12 | 9 | 9 |
| Event | 28 | 7 | 7 |
| Quest | 4 | 4 | 2 |
| Token | 14 | 6 | 6 |
| Deprecated | 1 | 0 | 0 |

- 共享遗物池实际 118，头文件 118、no-stub 114；战士专属遗物 8/8/no-stub 8。旧报告 `115/115` 与当前池不一致。
- 共享药水 45/45/no-stub 45；战士专属 3/3/3，事件药水 3/3/3。
- 无色 65 包括多人专属内容；旧报告定义单人范围 53，当前 52 个头文件。剩余 `Splash` 的跨职业生成依赖其余角色，不可改成战士池或过滤掉来宣称原版完成。
- 五角色开局/地图通过，不等于五角色战斗内容完整。球和 Osty 的基础存在，不等于 Defect/Necrobinder 全职业规则完成。
- 附魔、苦难、修饰器及其 Hook/实例字段属于完成范围；上述库存不能代替行为和条件路径的差分。

具体剩余路径包括 FakeMerchant 的选项生成桩、FoulPotion 的假商人分支、WeakPower 的 Paper Krane 分支、SpoilsMap 的幕二移除以及 CardCreationOptions 非零 filter。后四类可以在已有头文件/无标注 stub 的函数体中直接触发 Unported，说明完成门禁必须检查方法内部条件路径。当前行为源与生成表不能通过过滤掉这些原版可达内容来“补齐”。

### 3.3 哪些内容有较强实证

战士 90 张卡有[历史全卡矩阵](2026-09-25-m2-progress.md)：3 种第一幕遭遇 × 90 卡 × A0/A10，共 **540/540 格、8640/8640 场、147638/147638 状态**通过。它是有限配置的逐决策差分，不证明全部卡×遗物×能力组合。

[全遭遇阶段报告](2026-09-26-all-encounters-comparison-mod.md)记录累计 **288/288 场、5297/5297 状态**及后续若干专项。多个 Boss、召唤、复活、假升级等已做 A0/A10 差分。这些是历史证据，本次未重跑全部批次；90 个头文件不等于 90 个遭遇全部有大规模随机验收，The Architect 等仍有验证边界。

## 4. 通用 Env 架构与 API 的实际状态

| 能力 | 已有 | 关键剩余工作 |
|---|---|---|
| 确定性基础 | RNG、Decimal、.NET 排序/集合、生成表与 golden | 版本漂移与所有新路径保持同样纪律 |
| 游戏执行 | Cmd/Hook/回合/FSM，地图、事件、商店、休息、宝箱、奖励流程 | 全内容/条件、真实档案、后两幕端到端、所有嵌套选择 |
| 状态与分支 | POD GameState、RNG 在状态内、逻辑 Hash、owned Clone/Predict | 身份/对象寿命和容量合同；预测追踪复制成本尚需量化 |
| 环境入口 | Reset、LegalActions、Step、Predict、512 动作 mask | 更广局外预览上下文、结构化结果、明确错误/停止边界 |
| 选择 | 静止根 + 动作 + 答案前缀，typed choice 请求 | 所有房间/回合起始/自动效果的选择和恢复合同 |
| 快照 | 完整逻辑字段导出、schema/hash/引用检查，战斗与部分选择导入 | 地图/事件/奖励/商店/休息/宝箱导入；跨进程稳定模型/FSM 协议 |
| 隐藏信息 | Observe、Determinize，特权快照分开 | 全局外隐含结果的可见性合同；预览客户端的权限/展示由客户端处理 |
| SDK | C ABI + Python ordinary Env；C# owned sessions | 独立通用 SDK、Python 预测/查询封装、脱离 CombatSolver 命名和私有策略依赖 |
| 批处理/性能 | VectorEnv 独立状态，支持分支隔离 | 当前是逐个循环，未证明真正并行吞吐；M5 Clone/Step/ABI 性能目标未验收 |

### 快照导入比导出窄

`env/env.h` 和 `env/snapshot_import.cpp` 明确：普通导入仅支持当前 runtime 的活跃战斗、玩家决策及部分战斗选择；pending 操作限定 PlayCard/EndTurn/UsePotion。非战斗根会得到 `snapshot: combat root is not active`。

因此 Env 从 `Reset` 创建的状态能运行并预测局外流程，但**不能现在就把任意真实局外房间捕获后恢复**。runtime ID 绑定当前加载进程的注册身份，此协议不是跨进程通用存档。恢复应扩到定义好的静止边界，不承诺复制任意执行中的 async 栈。

### 真实档案仍只是存下了事实

Run/Player 的真实运行次数、纪元、已见遭遇等字段已进入 POD，但 `engine/run_setup.cpp` 仍只接受全解锁。`Env::Step/Run/LegalActions` 对不符合 full-veteran 的档案抛出 `live profile actions require epoch, encounter and first-run rules`。

这能防止静默假装正确，却不能支持用户普通存档。需要把这些原版规则做进 Env，不能靠改用户档案或把捕获值覆写成全解锁解决。

### 当前预测与事实查询值得保留

`CombatPredictionSession` 提供独立 owned Clone/Predict/Hash/Restore，并在安全胜负点截停，保留查询所需战斗对象，避免完整房间结算。这是可复用的执行边界，应抽象成通用停止选项。

已有卡牌/牌堆/目标/费用/意图 getter，13 个审计模型的计算变量及原始操作数，五种球的队列/数值，能力事实及挂载/移除事件。原 Beam 的卡筛选、风险和终局保留计算已经移回 C#，这个职责方向正确。

但 `combat_prediction_boundary.h` 只有十类主要战斗事件、部分能力事件和六计数。缺完整父子因果、所有方法/Hook/来源、局外结果及通用 RNG 抽取事实。GameState 是 POD；prediction metadata 有 vector/string，Clone 并不等于“所有分支只做一次 memcpy”。

### 性能设计目标尚有明显未验收项

本机 GCC、C++20、当前头文件直接测量 `sizeof(sts2::GameState)` 为 **145936 字节，约 142.5 KiB**；最初设计目标约 8 KiB。这是结构大小测量，不是运行时间测量，不能据此推断快慢。当前每分支 POD、选择基线和可增长的 trace 都会占空间；需用真实分支数、分配/复制/Hash/ABI 成本做 profile，再决定容量布局与追踪存储改进。原目标没有自动放宽，也没有 Clone <1µs/微秒 Step 达标证据。

## 5. CombatSolver Env 后端进度

固定 upstream：`external/CombatSolver` 的 `d231e9e51a0e58d6bfa1373c6265cce13ffd9a45`。派生 Mod 编译其原 Beam 源码并通过生成锚点增加诊断，子模块未修改。

| 项目 | 判断 |
|---|---|
| 开发版程序集、加载、持久化 Native/Env/Both 设置 | 已有；设置存在不代表 Env 执行可用 |
| seed-free live 组件捕获和 blank 模板组装 | 已有有限支持，包括真实 RNG、卡/怪物/FSM、档案事实及完成的 Neow |
| owned 战斗转移、终局、选择前缀及纯查询 | 有实证，但仅声明的内容和场景 |
| 牌/球/能力/牌堆/药水/循环形状键等 C# 投影 | 有部分逐位对齐；完整原状态键仍 false |
| 原 Native Replay 后追加 Env 重放核对 | 已有；Native 仍执行原搜索所用转移 |
| 全 live 状态/对象闭包/历史/选择导入 | 未完成；动态对象、特殊字段、异步边界、真实档案等仍可拒绝 |
| 原 Beam 全历史、政策、威胁、评分与所有后继改用 Env | 未完成 |
| worker 不再调用原生游戏模拟器 | 未达成 |
| 同算法同预算性能及最终双模式 mods 产品 | 未达成 |

直接证据：

- `tools/CombatSolverEnv/Runtime/EnvBackendSettings.cs`：`EnvExecutionGate` 始终返回 `live_root_and_same_policy_adapter_incomplete`。选择 Env 仍回退 Native。
- `tools/CombatSolverEnv/Search/EnvSearchBranch.cs::RequireExecutionReady`：历史、原状态键、政策和威胁完整度逐项拦截。
- `prepare_sources.py` 与 `Search/EnvBranchVerification.cs`：原 Replay 产生 snapshot 后才调用 `VerifyEnvReplay`。
- `Sts2EnvCompare` 的参考 C++ Beam 使用另一套简单值函数/宽深预算，不能用于证明原 CombatSolver 核心完整替代，也不能宣称同条件提速。

既有投入并非全部浪费：捕获字段、owned 分支、纯 getter、原始事实、正版夹具可成为通用 SDK 与 Env 回归。现在暂停进一步追原评分/指纹小片段，避免把搜索特例不断塞进 Env。

## 6. Random Foreseer 对通用 Env 的要求

本次读取[公开源码](https://github.com/hotwords123/StS2.RandomForeseer/tree/c4f14db684d831f9786d1c6af15b172b89d04939)，固定 commit `c4f14db684d831f9786d1c6af15b172b89d04939`，发布说明 v0.13.17、游戏基线 v0.111.0。与本地旧版本或 CombatSolver 内嵌镜像不要混用。

| RF 单人能力 | Env 当前基础 | 必须补齐的通用能力 |
|---|---|---|
| 抽牌、重洗、Frozen Eye 顺序 | 牌堆/抽牌/洗牌和 owned 分支 | 完整 Hook、来源/实例顺序、可见性与查询结果 |
| 随机生成卡/药水、随机选择/目标 | 部分生成、药水动作、选择前缀 | 真实完整池、全部原版条件、选择继续与明确停止状态 |
| 自动打牌、球目标/数值、结束回合伤害明细 | 自动效果/球基础、部分 trace | 完整执行和多层因果、所有来源/每笔损伤、停止边界 |
| 牌组变牌、遗物拾取后结果 | C++ 内部已有相关命令与部分效果 | 公开带来源和上下文的预览操作，候选/结果/连续奖励 |
| 事件选项、Crystal Sphere 位置、下一幕先古/Boss | 事件框架、预生成未来队列 | 事件/先古全内容、任意对应房间恢复、结构化结果 |
| Courier 补货、Driftwood 重掷、PaelsWing 献祭、休息随机结果 | 正常购买/休息及 Courier 部分路径 | 获取顺序、奖励偏移、库存/黑名单、合法预览与结果协议 |

源码 `OutOfCombat/RunPredictionContext.cs` 克隆 Rewards/Shops/Niche、遗物袋、牌组和概率；`MerchantRestockPrediction.cs` 处理候选排除、价格以及购买遗物后对 RNG 和 Hook 的影响。`Common/PredictionTrace.cs` 提供父子调用来源。由此可见，仅暴露战斗 `Predict` 和若干 getters 不足以替代 RF 的模拟能力。

正确实现是在通用 Env 执行同一份原版规则，返回可继续分支和事实；RF 类客户端构建悬停提示和公平展示。它的 approximate mirror/inference 风险、Godot UI 和多人规则不应直接复制进 Env。

## 7. 本次重新运行的验收

原始日志保存在本机 `/tmp`，不提交大体积原始产物。

### 7.1 当前全量门禁

命令：`./scripts/check.sh > /tmp/mcc-env-audit-full-20260927.log 2>&1`，**退出 0**。

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

Oracle 构建有一个既存 CS8604 警告，未阻止运行。

### 7.2 预测/真实对象/所有权夹具

依次运行，全部退出 0，均设置 `-p:CopyModOnBuild=false`，未安装新 Mod：

```text
~/.dotnet/dotnet run --project tools/EnvCombatPredictionChecks -c Release -p:CopyModOnBuild=false -- --out /tmp/mcc-env-audit-beam-20260927
~/.dotnet/dotnet run --project tools/EnvLiveCardChecks -c Release -p:CopyModOnBuild=false
~/.dotnet/dotnet run --project tools/EnvSessionChecks -c Release -p:CopyModOnBuild=false
```

- 日志 `/tmp/mcc-env-audit-beam-20260927.log`：**16 个场景通过**，含跨回合、药水、终局、额外回合、中毒、能力退休/重新获得；输出明确 `full_state_comparison=false; full_history_payload_comparison=false; TestMode=false; installed_game_acceptance=false`。
- 日志 `/tmp/mcc-env-audit-live-20260927.log`：**6 个 whole-live-root 有限夹具通过**（Strike、Rampage、Thrash、Thrash+BodySlam、BodySlam、Barrage），真实游戏对象组装、恢复、出牌及两个完整后继回合逐字段一致，均明确 `installed_game_acceptance=false`。部分组件夹具仍写明 `full_root=false`。
- 日志 `/tmp/mcc-env-audit-session-20260927.log`：64 个并行 ordinary 预测、16 个专用分支、源隔离、失败恢复、Dispose、完整快照/schema/runtime/ref/hash 拒绝通过。
- 真实对象的无头夹具有 creature animation 节点缺失日志；上述断言和退出码通过。这些工具结果不能解释成安装游戏的任意战斗 Env 接受。

### 7.3 绿灯到底证明什么

- L2 是 20 seeds × 5 角色 × A0/A10 的开局/地图。
- L3 默认是战士首层战斗，不是全部角色/全遭遇。
- L4 默认是战士第一幕路径。`OracleRun.cs::StopBeforeBoss` 默认 true，走到第一幕 Boss 前即结束；部分路径更早战死。**不是打完第一幕 Boss，更不是完整三幕。**
- `tests/l4/l4_run.py` 默认只要求零分歧；只有 `--require-complete` 才要求没有 Unported。当前这 80 条确实没有 Unported，但脚本本身不是全内容完成门禁；smoke 本次仍有 6 条 Unported。
- L3/L4 常规协议主要比较输出的决策字段与 RNG 计数等，不等于每步完整 POD/全部隐藏字段证明。
- 旧无头 oracle 全局启用 TestMode，再对改变逻辑的已审计方法恢复生产分支，并替换必要选择屏幕；这与 AGENTS 的纯生产目标存在已登记边界。现有专项 TestMode=false 夹具有价值；完整验收仍需审计所有新增 TestMode/表现层路径，而不能把补丁存在本身当作所有结果无效。
- 本次没有重新运行历史 8640 卡矩阵、全部遭遇批次、真实安装游戏任意根，也没有测 Clone <1µs 或原 Beam 等预算速度。

### 7.4 状态大小测量

临时源文件 `/tmp/mcc-env-audit-size-20260927.cpp` 仅包含 `engine/sim/state.h` 与输出 `sizeof(sts2::GameState)` 的 main。本机运行：

```text
g++ -std=c++20 -I. -O2 /tmp/mcc-env-audit-size-20260927.cpp -o /tmp/mcc-env-audit-size-20260927
/tmp/mcc-env-audit-size-20260927
sizeof(GameState)=145936 bytes
```

源码、可执行文件都仅在 `/tmp`，未加入仓库。结构大小不等于 Clone/Step 时间。

### 7.5 可重复静态库存

`python3 tools/coverage/env_inventory.py` 退出 0，输出第 3 节相同数量。`--json` 输出同一清单及无头文件却在静态名单中的 ID，明确带有不证明完整行为的说明。该脚本复用当前 generator 的类型/别名映射，不把名单当作可达性或真值覆盖；生成分发结构改变时会明确要求重新审计。

## 8. 为什么不继续追移植细节

当前原 Beam 直接使用原模拟器、模拟状态、历史、动态值和大量评分字段。补齐一段键或一个 Getter，不能消除其他部分的原游戏依赖。若 Env 连任意真实档案/局外根/选择/完整生成内容都不支持，就会在搜索扩展或重放更深处再次中断。

先做通用 Env 可以用统一原版执行和接口解决这些共同根因，同时给 RF 预览、其他搜索算法和 Python 提供价值。需要避免另一种返工：不能等全部内容写完才设计 API。应每次选一个原版功能，把执行、恢复、预测结果和正版差分一起完成。

## 9. 接下来按什么顺序做

详细阶段、文件和 gate 见[完成路线](../plans/2026-09-27-generic-env-completion.md)。

1. **E0**：自动内容/方法/路径覆盖清单，分离零分歧与零缺口门禁，显式记录真实终点与 oracle 盲区。
2. **E1/E2**：所有房间与选择恢复；统一 owned 预测、原版操作预览、停止边界、结构化结果和因果 trace。
3. **E3**：先贯通战士三幕，再补其余角色、先古/事件、跨职业、附件/修饰器和实际档案规则；每批通过公共 API 验证。
4. **E4**：从派生 Mod 中迁出独立公共 SDK 与通用游戏捕获桥，补 Python 等价能力。
5. **E5**：RF 单人能力矩阵由外部消费者实现、完整多幕/多角色覆盖、1 万条零分歧零 Unported 原版轨迹及可复现性能。

后续方向已变更：CombatSolver 历史内容冻结，Env 完成也不自动恢复。若用户重新明确要求移植，C# 策略适配、执行、部署及 mods 成品应作为新的独立验收。
