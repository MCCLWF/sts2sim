# 通用 Env、RF 公共 API 与旧迁移：当前全面复核

日期：2026-09-27。代码基线：`8fb9a73`，分支 `feat/generic-env-rf-api`，原版基线 v0.111.0。本轮只有调查和文档调整，没有新增玩法或搜索适配。计划：[本轮复核](../plans/2026-09-27-env-focus-audit.md)。

## 1. 结论

**继续开发通用 Env 有意义；现在应集中完成原版执行与 RF 所需公共 API，保持 CombatSolver 冻结。**

已投入的确定性基础、Cmd/Hook/FSM、POD 分支、战士内容和房间决策都有可复用价值。补齐这些能力可同时服务预览、搜索和强化学习。现有问题横跨内容、整局流程、档案、捕获和接口，继续围绕旧求解器的评分字段补丁会反复遇到相同底层缺口。

当前可描述为“有验证基础的部分原版环境及第一批通用预览”，不能描述为“完整五角色三幕 Env”“已实现 RF”或“CombatSolver 核心已被 Env 替换”。不能给出一个可信的项目完成百分比：静态内容库存、可分发方法、条件路径和正版验证覆盖是不同层次。

内容与 API 按功能一起推进。固定游戏版本及单人支持范围验收后，客户端通过公共接口完成预测或搜索，不为某个模组增加引擎政策。游戏版本或新增游戏规则改变时仍需维护 Env。

## 2. 通用 Env 的实际进度

| 能力 | 代码中已经存在 | 未完成的关键部分 |
|---|---|---|
| 确定性 | 原版 RNG/种子派生、Decimal、.NET 排序/集合顺序、golden 与差分工具 | 新内容所有方法/条件路径继续遵守同样真值纪律 |
| 执行 | Cmd、Hook、回合、怪物 FSM、各常见房间入口/决策 | 正常跨幕与最终胜利流程、全内容、复杂事件父房间恢复 |
| 状态/分支 | POD GameState、全部 RNG 在状态、owned Clone/Predict、逻辑 Hash | 容量与对象寿命的完整合同、性能目标 |
| 动作/选择 | LegalActions/Step/Answer、静止根加答案前缀重放、C++ typed ChoiceRequest | 常规 C ABI/Python 选择描述查询、所有可达选择及真实游戏选择的捕获/恢复 |
| 普通预览 | PreviewAction/C ABI/Python preview；独立后继、源不变、可关闭 trace、Choice 可继续 | 悬停所需原版操作上下文、更多停止边界、完整结构化结果 |
| 查询/追踪 | 费用/动态变量公式/球/能力查询；部分出牌、抽牌、生成、伤害、Power 事实与父子帧 | 局外结果、来源实例、完整 Hook/RNG/资源/奖励因果；当前明确 `trace_complete=false` |
| 恢复 | v2 全逻辑字段快照及模拟器各房间、战斗/奖励/Smith 选择 round-trip | 独立真实捕获桥、更多边界、跨进程持久协议；当前限定 loaded native runtime |
| 档案 | live 解锁/纪元/运行次数/已见遭遇事实已存入状态 | 部分解锁/初次运行的规则；不兼容档案仍拒绝 Step/LegalActions |
| 消费者 | C ABI、Python ctypes、VectorEnv；普通参考 ISMCTS 在 Env 外部 | 独立通用 C# SDK、游戏捕获桥、RF 功能消费者和验收 |

核心证据：[Env](../../env/env.h)、[普通执行](../../env/env.cpp)、[Preview](../../env/preview.cpp)、[公共 ABI](../../env/c_api.h)、[快照恢复](../../env/snapshot_import.cpp)、[执行事实](../../engine/sim/execution_trace.h)。

“全逻辑字段快照”表示当前 C++ 已建模结构的全字段输运，不证明原版所有逻辑字段都已建模。部分 CardChoices/地图点历史记录仍在移植代码中省略，需按实际读取者审计其影响，不能用模拟器自身 round-trip 证明原版状态完备。

### 2.1 本轮确认的主流程缺口

这不是仅缺后两幕测试。当前 `RunFlow::StartRun` 的正常流程固定 `SetActInternal(0)`；其余外部调用为 `ResetCombat`/调试夹具。`AfterCombatAction` 在死亡时设 Ended，胜利则发奖励；`ProceedFromRewards` 无条件回到 Map，没有进入下一幕或最终胜利的普通后继入口。[RunFlow](../../game/Runs/RunFlow.cpp:304)、[StartRun](../../game/Runs/RunFlow.cpp:76)、[SetActInternal](../../game/Runs/RunManager.cpp:288)。

原版单人也使用 `ActChangeSynchronizer`：ready 动作最终调用 `MoveToNextAct`，递增 ActFloor 并调用 `RunManager.EnterNextAct`；后者调用 `EnterAct(CurrentActIndex + 1)`，末幕后进入 TheArchitect，最后由原版 WinRun 结束。真值位置为 `Multiplayer/Game/ActChangeSynchronizer.cs:80`、`Runs/RunManager.cs:1308/1335/1344`、`Models/Events/TheArchitect.cs:350`。目录名 Multiplayer 不代表这些单人调用可排除。

因此 README/路线图中的“整局骨架”应理解为已有房间和决策骨架，不能理解为普通三幕对局已贯通。跨幕、末幕终点和相关 Hook/房间恢复应优先开发并扩展 oracle 记录。

### 2.2 档案和接口的实质限制

- `engine/run_setup.cpp:14` 拒绝非全解锁新局；`env/env.cpp:103/119/247` 对不兼容 live 档案返回 Unported。存入真实值不等于已按真实值执行规则。
- `snapshot_import.cpp:174/175` 要求 schema 和当前 runtime_id 相同；这是进程内输运/恢复协议，尚不是可跨进程保存的通用存档。
- 普通 Preview 只接受当前合法 Action。Engine 内部存在 CardCmd.Transform、RelicCmd.Obtain 或 Merchant.Restock，不等于公共 API 已能表达所有悬停和假设上下文。
- 非战斗 `inspect_current` 只给 hash/decision；当前常规查询均面向战斗。特权全快照能读到牌组/房间/Choice 字段，但尚无面向 SDK 的完整局外查询、选择选项和结果合同。C ABI 也缺独立 ABI 版本/能力发现接口。
- 本轮实际复现 ABI 输入问题：`action_from_c` 在窄化前不校验 kind/坐标范围。合法 PlayCard 的 kind=2 改成非法 kind=258，Predict 仍接受且后继 hash 相同。应作为 E2 健壮性缺口修复；本轮未改代码。[窄化入口](../../env/c_api.cpp:48)。

## 3. 内容库存和保真证据

正式 ID 排除 Mock，包含不属于普通运行池的辅助/事件等模型。以下为当前静态库存，不是完成比例。`IsUsable` 只说明生成器未识别到待分发的覆写桩，不能证明函数内部条件分支、依赖或组合已实现。

| 类型 | 正式 ID | 具体头文件 | 静态 IsUsable |
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

| 角色卡池 | 头文件/池内正式卡数 |
|---|---:|
| Ironclad | 90/90 |
| Silent | 26/91 |
| Defect | 31/91 |
| Regent | 8/91 |
| Necrobinder | 10/91 |

盘点入口：[env_inventory.py](../../tools/coverage/env_inventory.py)。头文件数可以低于 IsUsable 数，原因包括基类/默认元数据行为；不能据此自动认证完整语义。具体仍有非零 CardPoolFilter、FakeMerchant/CrystalSphere 事件、SpoilsMap 任务等缺口。原版可达内容不得从生成池中过滤掉以制造“完整”。

较强的已有内容证据：战士 90 卡历史有限矩阵 540 格、8640 场、147638 状态对齐；最新五张 Silent 卡 80 场/1170 状态，PaperKrane 主矩阵及补充 24 场/407 状态，另有非致死前缀。这些证明各自有限配置，不能证明所有组合及纯生产/真机路径。详见[战士矩阵](2026-09-25-m2-progress.md)、[Silent 小批](2026-09-27-env-silent-simple-cards.md)、[PaperKrane](2026-09-27-env-paper-krane.md)。

## 4. CombatSolver：核心替换没有完成，当前已冻结

冻结前完成的主要是 UI/设置、部分真实状态捕获、原版 getter/公式输入、若干历史与状态键投影，以及有限操作 replay 核对。完整搜索仍走原 Native 求解器；Env 侧正式执行门禁没有开放。缺少完整树扩展、所有动态对象/选择映射、策略等价、执行部署和同预算性能胜利证据。

当前落实情况：旧 prediction session/终局旁路/专用测试/Beam/solver CLI 移入 `archive/combatsolver`，公共库移除 `sts2_combat_prediction_*` 和 `sts2_env_solve_*`；C ABI 只链接普通 Env，普通 ISMCTS 保留为外部消费者。七个依赖旧 SDK 的 managed 项目有明确构建拒绝 target。[归档](../../archive/combatsolver/README.md)、[构建隔离](../../tools/Directory.Build.targets)、[库依赖](../../CMakeLists.txt:61)。

以前为搜索跳过正常胜利 Hook/清理/奖励的分支已撤销。原版 CalculatedVar 的 base/extra/multiplier、真实 History 计数和只读事实保留为一般能力。搜索评分、剪枝、威胁/风险推断、特定状态键及 UI 应留在客户端。

本轮对 active Env/Engine/Game 构建路径的静态检索没有再发现旧 Beam/专用终局标志；这属于有限静态审计，不是所有函数和条件路径的形式证明。

隔离范围也有边界：历史 C# 适配器仍在 `tools/CombatSolverEnv`；其旧 P/Invoke 已与当前库不兼容。`external/CombatSolver` 保留原上游子模块，直接手工构建它不受 `tools/Directory.Build.targets` 的七项目拦截约束；旧 README 也保留了带冻结头注的历史构建命令。因此当前已隔离活跃 C++ 构建和指定派生工具，不能声称删除了全部旧代码或阻止了一切直接上游构建。本轮没有构建上游、检查/清理游戏中此前安装的 DLL或部署。[历史项目](../../tools/CombatSolverEnv/CombatSolverEnv.csproj)、[历史命令](../../tools/Sts2EnvCompare/README.md)。

## 5. RF 应如何由公共 Env 支持

RF 需求参考固定 v0.13.17 源码 `c4f14db684d831f9786d1c6af15b172b89d04939`；RF 的 mirror/fast-forward 代码是消费者需求资料，原版执行真值仍来自游戏源码与 oracle。

| RF 功能组 | 当前能用的基础 | 还需补齐 |
|---|---|---|
| 出牌、药水、抽牌/重洗、随机目标、自动打牌、球、结束回合 | 合法动作 Preview/Answer、部分战斗查询与事实 | 全部依赖内容/条件、选择上下文、稳定实例与完整因果结果 |
| Frozen Eye/牌堆顺序 | owned 状态和特权快照包含顺序 | 公共牌堆/可见性查询、真实捕获；展示策略由客户端实现 |
| 牌组与战斗变牌 | EventOption/Answer 能走已支持的原版选择流程；CardCmd.Transform 和相关 Hook 已存在 | 牌组/选项/结果查询；合法动作不能表达的悬停上下文才补有类型的原版操作 |
| 遗物拾取和连锁生成 | RelicCmd.Obtain/AfterObtained、奖励/选择基础 | 假设获取操作、取得顺序、遗物袋/牌组/奖励结果和依赖内容 |
| Courier 购买补货 | ShopBuy 与 Merchant 的购买/补货路径 | 库存/价格/来源查询、购买后 Hook 与补货的完整验证、悬停上下文 |
| 事件与隐藏牌位 | EventOption、部分事件及 Choice | 所有事件原版状态/分支，CrystalSphere 等缺失内容，隐藏位置查询 |
| 下一幕先古/Boss | 状态中预生成各幕队列 | 通用 run 查询、档案规则、正常跨幕执行与先古内容 |
| Driftwood 重掷、PaelsWing 献祭、休息随机结果 | 奖励/休息骨架和分支能力；Reward 已有 Skip/Reroll 选择执行 | Driftwood 相关规则、TryModifyCardRewardAlternatives/献祭、连续奖励状态与结构化输出 |

目标接口形态是：**恢复冻结输入 → 克隆分支 → 执行合法动作或有类型的原版操作 → 按明确边界停止 → 返回可继续选择、原始结果及可选因果事实**。操作使用原版 Cmd/Hook/RNG，客户端不需要重复实现随机算法。不能用任意写字段接口替代来源/参数验证，不能为 RF 添加另一套近似预测公式。

优先复用普通动作：Courier 的购买/补货已有真实 ShopBuy 路径，主要缺库存/费用结果查询与差分；重掷已有 Reroll 分支，献祭需要补原版奖励 alternative Hook/类型，而不是 RF 特例动作。[购买补货](../../game/Entities/Merchant/MerchantInventory.cpp:336)、[奖励 alternative 缺口](../../game/Rewards/Reward.cpp:30)、[重掷选择执行](../../game/Rewards/Reward.cpp:242)。

独立 C# SDK 和捕获适配器负责版本、所有权、原版对象到状态的映射；RF/其他 Mod 负责缓存、允许显示的信息、提示和 Godot UI。此边界也适用于未来用户重新要求的搜索移植。

## 6. 验证与性能：本轮事实和已有记录分开

### 本轮重新执行

```text
ctest --test-dir build --output-on-failure
100% tests passed, 0 tests failed out of 10
Total Test time (real) = 5.64 sec
```

`nm -D --defined-only build/libsts2env_c.so | rg ' sts2_'` 返回 27 个普通 `sts2_env_*` 符号，无旧 prediction/solve ABI。库存工具及源码读取不运行生成器。非法 action kind=258 的只读分支复现得到 `accepted=True, same_successor=True, source_unchanged=True`。

### 前一批完整门禁日志已复查，本轮未重跑

`/tmp/mcc-env-full-check-integrated.log` 的实际结果：L2 200 次开局零分歧；L3 80 场、1611 个输出状态零分歧/零 Unported；L4 80 条路线、4089 个决策零分歧/零 Unported；三个 smoke 批次之一仍有 6 条 Unported。

默认 L3 是战士首层；默认 L4 止于第一幕 Boss 前或更早死亡。`reason=boss` 在这套记录中表示到达 Boss 前停止，不是打败 Boss 或通关。对拍覆盖协议输出字段和 RNG 计数，不能解释为每步全部 POD/所有隐藏字段的比较。当前 headless oracle 仍有 TestMode 与登记的生产分支恢复，O-007 死亡结算等边界未完全解决。详见[验证偏离](../audits/deviations.md)、[前批完整报告](2026-09-27-env-vanilla-preview.md)。

此外默认门禁本身允许 Unported：L3 只因 divergence 失败；L4 支持 `--require-complete`，但 `scripts/check.sh` 没有传入。本次复查日志的 L3/L4 恰好零 Unported，不能由此推断以后全绿必然零缺口。应保留有限回归，并新增独立的完整性门禁。[L3 返回条件](../../tests/l3/l3_combat.py:204)、[L4 开关](../../tests/l4/l4_run.py:153)、[门禁调用](../../scripts/check.sh:56)。AGENTS 的“Oracle 不用 TestMode”规则与当前 `Oracle.Run Init(testMode:true)` 实现存在冲突，应在 E0 解决并如实保留验证边界。

性能基线已存在，本轮没有新增测量。固定 Cultists 根平均 Clone 2.91µs、Hash 10.31µs、EndTurn Predict 109.70µs、Preview trace 开/关 121.22/121.92µs、快照 JSON 130.46µs；GameState 145936 字节。当前五个静态库 SHA-256 与[基线报告](2026-09-27-env-api-baseline.md)一致。这是本机有限场景、含销毁的成本，不能证明搜索提速、实机性能或 Clone <1µs 达标。

## 7. 接下来集中做什么

1. **真值与覆盖门禁**：在静态库存之外建立方法/条件/实际测试清单；分开统计零分歧、零 Unported 和轨迹实际终点；扩展 oracle 跨幕与结算记录。
2. **主流程与状态**：补单人正常跨幕、最终胜利、事件战斗返回父房间；补真实档案规则和相关恢复合同，先贯通战士完整三幕。
3. **RF 所需通用操作/结果**：先用抽牌、变牌、遗物拾取、Courier 购买补货贯通公共接口、选择、结果、恢复与正版差分；同步完成输入验证、停止/失败合同和实例追踪。
4. **内容批次**：其他角色、先古/事件、跨职业生成、附件与官方单人修饰器。简单独立模型继续由 GPT-6 Luna xhigh 批量完成，集成和真值验收由主代理负责。
5. **接入和完成验收**：独立 C#/Python SDK 与游戏捕获桥；RF 单人功能矩阵由公共 API 消费者跑通；强制各幕/各角色覆盖加至少 1 万条零分歧、零 Unported 轨迹；可复现性能验收。

E1/E2/E3 按独立功能可以并行，但每批必须形成可使用、可差分的完整链路。以上内容完成前不恢复 CombatSolver；完成也不自动解冻历史项目。完整范围与门槛以[通用 Env 完成路线](../plans/2026-09-27-generic-env-completion.md)为准。
