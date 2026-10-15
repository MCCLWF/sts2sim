# 通用 Env、CombatSolver 与 RF：最新全面进度判断

日期：2026-09-27。代码基线 `5339da4`，分支 `feat/generic-env-rf-api`，游戏 v0.111.0。计划：[本轮复核](../plans/2026-09-27-env-priority-review.md)。本轮没有修改游戏规则、恢复旧搜索或部署 Mod。

## 1. 决定

**继续开发通用 Env 有意义，当前集中完成原版单人规则与 Random Foreseer 所需公共 API；CombatSolver 迁移保持冻结。**

已有确定性基础、Cmd/Hook/FSM、独立分支、选择重放和战士内容具有可复用价值。主要阻塞仍是整局主流程、内容、真实档案、结果合同与捕获。这些缺口同时影响随机结果预览与搜索。继续围绕旧求解器状态键、评分和历史投影补片段，不能解决底层缺口。

内容和 API 应同步按功能推进：每批形成“原版操作 → 独立分支 → 选择继续 → 结果查询 → 快照恢复 → 正版差分”。不必等所有卡牌都写完才设计 API；当前暂停的是 CombatSolver 策略移植、双模式产品和性能比较。Env 完成后也不自动恢复历史项目。

## 2. 当前 Env 实际能做什么

| 层 | 已有实现与证据 | 关键缺口 |
|---|---|---|
| 确定性 | 原版 RNG、种子派生、Decimal、.NET 排序/集合；golden、开局/地图差分 | 新路径和版本漂移持续验证 |
| 游戏执行 | Cmd/Hook、回合、怪物 FSM；地图/战斗/奖励/事件/商店/休息/宝箱入口 | 普通跨幕、最终胜利、事件战斗返回父房间、完整内容 |
| Env | Reset、LegalActions、Step、Clone、Hash、Predict、Observe/Determinize、VectorEnv | 全路径支持、容量/寿命合同、性能验收；VectorEnv 当前是独立状态的循环封装 |
| 选择 | 静止根 + 待执行动作 + 答案前缀；实际 card/bundle/reward/alternative/proceed 描述 | 所有可达选择、真实游戏选择的捕获与恢复 |
| 预览与查询 | 普通 owned Preview；战斗 getter/公式/球/能力；局外资源/牌组/房间/选择；可选 before/after 投影 | 悬停所需有类型操作上下文、一般停止边界、完整因果结果 |
| 恢复 | v3 全部已建模字段及模拟器房间/选择恢复；schema/runtime/ref/hash 验证 | 当前绑定 loaded native runtime；独立真实捕获、更多原版字段与跨进程持久协议 |
| 接入 | 普通 C ABI、Python ctypes；29 个 sts2_env 导出 | ABI 版本/能力发现、独立通用 C# SDK、游戏捕获桥、RF 消费者 |
| 档案 | 真实解锁/纪元/运行次数/已见遭遇事实进入状态 | 非全解锁新局及不兼容 live 档案仍拒绝执行；真实规则尚未补齐 |

主要代码：[Env](../../env/env.h)、[Preview](../../env/preview.cpp)、[局外查询](../../env/run_queries.cpp)、[ABI](../../env/c_api.h)、[快照导入](../../env/snapshot_import.cpp)、[档案限制](../../engine/run_setup.cpp)。导出已建模字段不等于原版全部逻辑字段都已建模。

### 主流程是实际缺失

`RunFlow::ProceedFromRewards` 无条件切回 Map；正常开局调用 `SetActInternal(0)`，C++ 没有 `EnterNextAct`、`WinRun` 或 TheArchitect 事件实现。[当前 Proceed](../../game/Runs/RunFlow.cpp:304)、[开局](../../game/Runs/RunFlow.cpp:76)。能查询下一幕预生成先古/Boss，不等于能正常进入下一幕。

原版单人 Boss Proceed 同样走 `ActChangeSynchronizer`，先增加 ActFloor，再由 `RunManager.EnterNextAct/EnterAct` 清理房间、重置地图/odds并触发 AfterActEntered；最后一幕进入 TheArchitect 后才调用 WinRun。双 Boss 首场只返回同幕地图，第二场才进入终点。C++ 已有双 Boss 地图/遭遇选择和最后幕 Boss 空奖励的部分规则，后续流程尚缺。

真值：`Multiplayer/Game/ActChangeSynchronizer.cs:38-94`、`Nodes/Screens/NRewardsScreen.cs:329-349`、`Runs/RunManager.cs:1308-1393/1524-1535/1623-1695`、`Models/Events/TheArchitect.cs:311-317/350-358`。Architect 对话依赖档案胜场和独立事件 RNG；最终胜利也涉及结算，不能用“最后 Boss 死亡后直接 Ended”代替整条流程。

## 3. 内容库存：不能当作完成百分比

本轮重跑 `python3 tools/coverage/env_inventory.py`，正式模型排除 Mock，包含辅助/事件模型：

| 类型 | 正式 ID | 具体头文件 | 静态 whitelist |
|---|---:|---:|---:|
| 卡牌 | 596 | 254 | 252 |
| 能力 | 265 | 140 | 147 |
| 怪物 | 120 | 110 | 110 |
| 遗物 | 299 | 183 | 180 |
| 药水 | 65 | 55 | 56 |
| 附魔 | 23 | 2 | 18 |
| 苦难 | 7 | 5 | 7 |
| 球 | 5 | 5 | 5 |
| 修饰器 | 17 | 0 | 6 |
| 遭遇 | 90 | 90 | 87 |
| 普通事件 | 59 | 49 | 35 |
| 先古 | 9 | 1 | 1 |

五角色卡池具体头文件：**战士 90/90、静默 27/91、机器人 31/91、储君 8/91、亡灵 10/91**。本轮纠正先前静默 28/91：原版 Outmaneuver 属于 EventCardPool，虽然在 Silent 自定义牌组中测试，它不能计入 SilentCardPool。Flechettes 才是本批新增的静默池卡。正式总卡牌头文件 254 不变。证据是 `data/content.json` 的 `CardPoolModel.props.AllCardIds` 与原版 `Models/CardPools/EventCardPool.cs:40`。

头文件、whitelist、方法实现和正版条件路径覆盖是不同事实。能力/附魔部分行为由基类承担，因此名单可大于头文件数；这不代表所有依赖和分支正确。当前具体缺口包括非零 `CardCreationOptions.CardPoolFilter`、FakeMerchant/CrystalSphere 事件选项、SpoilsMap 第二幕移除、奖励 alternative Hook。不能通过过滤原版池或替换随机算法掩盖缺口。

战士 90 卡历史有限矩阵 8640 场/147638 状态对齐有保留价值；最新定向三卡 50 场/702 状态也对齐。它们不证明所有交互或三幕完成，本轮没有重跑这些矩阵。

## 4. CombatSolver 核心替换的实际状态

冻结前已做：设置/UI、有限真实状态捕获、字段与 getter、部分历史/状态键投影、原 Native replay 后的 Env 核对。**完整树扩展和搜索仍未改为 Env；Env 正式执行门禁未开放。**

- `EnvBackendSettings.Initialize` 将 `EnvExecutionGate` 固定为 `live_root_and_same_policy_adapter_incomplete`，选择 Env 会由 Native 继续。[门禁](../../tools/CombatSolverEnv/Runtime/EnvBackendSettings.cs:18)
- `EnvSearchBranch.RequireExecutionReady` 仍要求完整历史、原状态键、政策和威胁投影；这些合同未完成。[要求](../../tools/CombatSolverEnv/Search/EnvSearchBranch.cs:191)
- 部分验证发生在 Native Replay 已生成后继之后，不能解释成 Native 搜索已被替换。
- 任意 live 根、动态对象/选择、策略等价、真实部署和同算法同预算速度对比均未完成。此前双模式 mods 产品目标尚未达成，当前冻结。

隔离已落实：旧 Beam、prediction session、终局旁路与专用测试归档；当前 C ABI 没有 `sts2_combat_prediction_*` 或 `sts2_env_solve_*`；七个旧 managed 项目有构建拒绝 target。`external/CombatSolver` 和历史 C# 适配器仍存在，不能说全部删除或阻止了任意手工上游构建。[档案](../../archive/combatsolver/README.md)、[构建隔离](../../tools/Directory.Build.targets)。本轮未检查或变更游戏中已安装的旧 DLL。

先前为搜索跳过胜利 Hook、回复、清理和奖励的专用分支已撤销；普通 getter/公式/原始事实保留在通用 Env。活动构建路径检索未发现旧 Beam/专用 terminal 标志；这是有限静态核对，不能宣称所有函数已形式证明无残留。

## 5. RF 能力如何成为通用合同

本轮复查本地固定 RF v0.13.17 源码，commit `c4f14db684d831f9786d1c6af15b172b89d04939`。其 README、RunPredictionContext、MerchantRestockPrediction 与 PredictionTrace 说明，需要局内和局外随机结果、连续 Hook、上下文与来源；RF 是消费者需求资料，原版源码/正版 oracle 才是执行真值。

| RF 功能 | 当前基础 | 完成前要补什么 |
|---|---|---|
| 出牌/药水/抽牌/重洗/随机目标 | 普通合法动作预览、选择继续、战斗查询和部分事实 | 全依赖内容、牌堆/实例顺序合同、来源和结果验证 |
| 自动打牌/球/结束回合伤害 | 战斗执行与 partial trace | 每笔结果与因果、全部延迟路径、明确阶段停止边界 |
| 变牌/遗物拾取 | 已支持事件/奖励动作可走原版 Cmd/Hook/Choice，before/after 可读 | 尚不能表达的悬停上下文、连续随机结果及正版结果对拍 |
| Courier 补货 | ShopBuy、库存/价格查询和购买后投影；有限路线对拍 | 更多购买/拾取 Hook 和内容组合，完整验收 |
| Driftwood 重掷/PaelsWing 献祭 | Reroll 路径与描述、Driftwood Hook；有限合同 | 本批尚无正版 REROLL action 结果对拍；献祭 alternative Hook/类型缺失 |
| 事件/CrystalSphere/休息 | 房间查询及部分正常动作 | 所有分支、隐藏牌位、随机后续与具体缺失内容 |
| 下一幕先古/Boss | 显式特权查询可读取预选模型 | 正常跨幕、真实档案与先古行为 |
| 接入真实游戏 | 历史捕获研究与当前快照模板 | 独立 SDK/捕获桥、版本/能力/所有权合同和 RF 消费者 |

完整结果不能只依靠前后字段差：相同终值可能经历不同的生成、移除、伤害或随机抽取。当前 Preview 明确 `trace_complete=false`；pool refs 仅属于对应 owned 状态，尚无完整跨分支身份/退休对象因果合同。

目标调用链是“捕获静止边界 → 校验/恢复 → 纯预测动作或有类型的原版操作 → 按边界停止 → 返回选择/原始结果/可选事实”。Env 负责费用、伤害、生成、随机性和 Hook；C# Mod 负责搜索算法、评分、剪枝、预算、公平展示、缓存及 UI。客户端应能在已验收 v0.111.0 单人范围内只调用公共 API；版本升级或新增第三方规则不属于无需修改 Env 的保证。

## 6. 本轮证据与限制

本轮重新执行：

```text
ctest --test-dir build --output-on-failure
100% tests passed, 0 tests failed out of 11
Total Test time (real) = 6.22 sec
```

库存工具文本与 `--json` 两模式退出 0，数量如上。新增卡池统计保持 `inventory()` 返回形状不变，以正式 CardModel 和 canonical AllCardIds 去重匹配；缺少列表或未知卡 ID 明确报错。`nm -D --defined-only build/libsts2env_c.so` 返回 29 个普通 Env 导出，无旧 prediction/solve ABI。`git diff --check` 通过。

本轮复查最近原始日志，未重跑完整 gate：`/tmp/mcc-env-run-query-full-check-final.log` 有 L2 200 次开局、L3 80 场/1611 状态、L4 80 条/4089 决策零分歧；smoke 一个批次有 6 条 Unported。公共局外查询定向对拍 16 条/2038 决策零分歧、零 Unported；高 HP 的 8 条停在第一幕 Boss 前，另外 8 条战死。[最近批次](2026-09-27-env-run-query-results.md)

默认 L4 的 `StopBeforeBoss` 为 true，而且无 children 时记录 boss；不能将 reason=boss 当作 Boss 胜利。L3 只因 divergence 失败，L4 默认也允许 Unported，check.sh 未启用 require-complete。尚缺方法/条件路径覆盖清单、独立零缺口门禁与实际跨幕/胜利轨迹。headless oracle 仍有 TestMode 及逐方法生产分支恢复；完整纯生产/真机验收仍缺，不否定已限定范围内的差分证据。

历史本机性能基线：GameState 145936 字节，Clone 含销毁约 2.91µs、Hash 10.31µs、EndTurn Predict 109.70µs；本轮未重新测量。尚无 Clone <1µs、公共 ABI/完整 RF、实机或同预算搜索提速证明。[性能边界](2026-09-27-env-api-baseline.md)

## 7. 集中开发顺序和完成门槛

1. **先打通主流程与真值记录**：普通跨幕、最终胜利、事件父房间恢复；让 oracle 真正记录 Boss 后、第二/三幕与胜利。增加零缺口门禁与实际终点统计。
2. **规则与 API 同步验收**：先用抽牌、变牌、遗物拾取、Courier 购买补货贯通公共接口、选择、结果与恢复；补有类型操作、停止/错误合同和完整事实。
3. **补可达内容与档案**：先战士完整三幕，再其他角色、先古/事件、跨职业生成、附件/修饰器和真实档案。简单独立内容继续用 GPT-6 Luna xhigh 批量实现，主代理负责集成和正版验收。
4. **独立接入**：公共 C#/Python SDK、真实游戏捕获桥；用独立 RF 类消费者验证客户端只调用公共 API。
5. **最终验收**：固定版本单人范围、五角色完整三幕和 RF 功能矩阵；至少一万条零分歧/零 Unported 轨迹并强制各幕覆盖；可复现 Clone/Step/ABI/trace 性能。

完整范围以[通用 Env 完成路线](../plans/2026-09-27-generic-env-completion.md)为准。当前整体未完成；不以本轮全绿或库存计数提前标记完成。CombatSolver 保持冻结，恢复相关工作需要用户后续明确要求。
