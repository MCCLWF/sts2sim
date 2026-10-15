# 通用 C++ Env、CombatSolver 核心替换与 RF API：开发状态勘察

> 此文是二幕开发前的代码勘察快照。后续 Orobas、二幕严格路线、第三幕入口、Pael's Wing 献祭及 Kaiser 无头真值修复见[最新跨幕进度](2026-09-27-env-act2-third-act-progress.md)。下文按当时状态记录的 Orobas 阻塞、快照失败和 Boss 差分已过时。

日期：2026-09-27，刷新包含当前跨幕批次的实际结果。分支 feat/generic-env-rf-api；已提交基线 8a93c1f，本报告同时检查未提交实现。游戏固定 v0.111.0；Random Foreseer 需求依据本地 v0.13.17、commit c4f14db684d831f9786d1c6af15b172b89d04939。计划：[本轮勘察](../plans/2026-09-27-env-development-status.md)。

**后续进展（同日）**：下文“逐决策恢复6/8失败”与“未重跑完整门禁”是这份勘察当时的基线，现已解决和重跑；详见[快照修复报告](2026-09-27-env-snapshot-stun-shared-page.md)及[第一幕跨幕报告](2026-09-27-env-act-transition.md)。目前第一幕真实轨迹逐决策恢复12/12条、3906/3906决策一致；完整 `scripts/check.sh` 为13/13 CTest、L2 200局、L3 80战/1611状态、L4 80局/4089决策通过。用户随后明确战士整局闭环优先，在此之前暂缓其他角色的新内容；二幕Orobas及最终胜利仍是未完成项。

## 1. 结论与方向

**继续开发通用 Env 有意义。当前集中完成原版单人逻辑、状态恢复和 RF 公共 API，CombatSolver 移植保持冻结。完整搜索核心没有替换成功，完整 Env/RF 也尚未完成。**

已有 RNG、Decimal、Cmd/Hook/FSM、状态分支、选择重放、房间与正版差分工具值得保留。它们共同服务预测、搜索和强化学习。原版费用、伤害、效果、随机数及 Hook 属于 Env；搜索评分、剪枝、历史投影、状态键、风险偏好、预算和 UI 由客户端实现。

**内容与 API 同步补齐。** 每批功能贯通原版操作、纯预测、选择回答、结果查询、恢复和正版差分；不能等全部卡牌写完再检查 RF 接口，也不能只增加接口名称而缺少执行规则。

目标结构：真实游戏捕获桥 → 独立公共 SDK → 通用 Env → 结果/查询 → C# 提示或搜索/UI 客户端。固定游戏版本、完成的原版单人范围内，未来移植应只写捕获映射和客户端逻辑，通过公共 API 使用 Env。版本变化、第三方新增规则需额外支持；现在尚未达到客户端无需修改 Env 的交付状态。

## 2. Env 各层实际状态

| 层 | 已有能力 | 尚缺 |
|---|---|---|
| 确定性基础 | 原版 RNG/种子、Decimal、.NET 排序/集合、内容导出；golden 和差分工具 | 新路径持续对齐、版本漂移验证 |
| 执行引擎 | Cmd/Hook、回合、怪物 FSM、地图和各房间入口；第一幕跨幕有实际差分 | 后续幕先古、最终胜利、父事件恢复、特殊奖励及全部可达内容 |
| 环境 | Reset、Step、LegalActions、Clone、Hash、Predict、Observe/Determinize、VectorEnv | 完整执行合同、容量/寿命、停止边界和性能；VectorEnv 仍是独立状态循环 |
| 选择 | 静止根 + 待执行动作 + 答案前缀重放；卡牌/卡包/奖励/alternative/proceed 描述 | 所有可达嵌套选择、真实输入和继续执行 |
| 查询与预览 | owned 后继、战斗 getter/公式、局外/选择查询、before/after | 有类型悬停操作、逐笔结果/来源/因果、通用停止阶段 |
| 恢复 | snapshot v3、字段/引用/schema/hash 校验、房间及选择夹具 | 实际轨迹恢复存在失败；更多原版字段、live 捕获、跨进程协议 |
| 接口 | 29 个普通 C ABI 导出、Python ctypes | ABI 版本/能力发现、独立 C# SDK、捕获桥、RF 消费者 |
| 档案 | 部分档案事实进入状态 | 非全解锁执行仍明确拒绝；解锁、纪元、初次运行、已见遭遇规则 |

代码依据：[Env](../../env/env.h)、[Preview](../../env/preview.h)、[C ABI](../../env/c_api.h)、[查询](../../env/run_queries.cpp)、[快照校验](../../env/snapshot_validation.cpp)、[档案执行门禁](../../env/env.cpp)。全部已建模字段的导出不证明原版全部逻辑字段覆盖。快照 registry 索引绑定已加载 native runtime，不能视为通用持久存档。

### 第一幕跨幕：有限真实路径已通过

普通 Boss Proceed 已接入 EnterNextAct/EnterAct，退出旧房间、重置当前幕层数/地图/odds并执行跨幕 Hook；首个双 Boss 保留同幕地图。原对拍发现新幕层数未归零和长战斗容量不足，当前实现已修正。

本轮对已有正版轨迹重新执行严格 L4：

- 无 Neow：8 条，2903/2903 决策一致；实际 7 条进入第二幕地图，1 条在第一幕 Boss 战死。
- 有 Neow：4 条，1003/1003 决策一致；实际 4 条进入第二幕地图。
- 合计 12 条、3906 决策，11 个实际跨幕终点；终点比较地图和全部 run/player RNG 的 counter/state0–3。公共查询额外验证全部 12 个终点。

以上均 Ironclad A0、hp10000 定向轨迹，种子/策略见第 6 节。它们尚未证明第二/三幕完整执行、自然生命值全程或完整胜利；默认 gate 仍在第一幕 Boss 前停止。

继续第二幕的已有正版轨迹，C++ 在 244/564 决策停止于 Events.Orobas.GenerateInitialOptions is not ported。第二幕起始先古是具体阻塞项。最终幕 Proceed 已指向 TheArchitect 事件，但 TheArchitect/WinRun、档案对话、事件 RNG 和结算没有实现；当前明确 Unported，不能把空地图或无选项叫作胜利。事件战斗返回父事件和特殊不发奖励路径也未完整贯通。

### 恢复：实际失败需要优先修复

同一批无 Neow 的 8 条轨迹开启 --verify-snapshots，每个决策导出/导入并比较 Hash、合法动作和恢复后后继。本轮重跑：**2 条一致、6 条失败，1015/2903 决策匹配，退出码 1**。

| 失败 | 定位依据 | 当前结论 |
|---|---|---|
| snapshot state: monster state index，4 条 | MonsterModel 使用合成 STUNNED 索引 -2，校验只允许 -1 或 registry 内索引 | 合法眩晕状态被拒绝；需约束地支持编码并保留非法状态拒绝 |
| snapshot state: live event synchronizer state，2 条 | 原版共享事件选择递增 pageIndex；C++ 也递增，快照校验要求 0 | 合法事件进度被拒绝；需支持共享事件页进度并保留结构校验 |

本轮是勘察，没有修复这两个失败。普通重放和有限恢复夹具通过不能覆盖这个结论。不能清空事件状态、放开任意负索引或放宽差分。原版 EventSynchronizer.BeforeExitingRoom 保留 canonical event/page，重置应遵守原版时机。

### 新增静默卡

Expertise、HiddenDaggers、LeadingStrike 已接入生成分发及构建。核对已有定向 L3：3 卡 × 原版/升级 × A0/A10 × 2 种子 × 2 策略，共 **48 场、990 状态、101 次实际目标卡出牌，零分歧、零 Unported**。本轮没有重跑该矩阵。

覆盖限于 Cultists 配置；dump 未直接包含全部 retain 内部标志，满手、NoDraw、所有生成 Hook、致死与真实捕获尚未完整验收。见[三卡报告](2026-09-27-env-silent-retain-shivs.md)。单张生成牌 wrapper 的空结果异常修正已构建，尚无独立非法调用差分验收。

## 3. 内容库存

本轮 python3 tools/coverage/env_inventory.py 实测：

| 类型 | 正式模型 | 具体头文件 | 静态 whitelist |
|---|---:|---:|---:|
| 卡牌 | 596 | 257 | 255 |
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

五角色卡池头文件：战士 90/90、静默 30/91、机器人 31/91、储君 8/91、亡灵 10/91；事件卡池 8/28。Outmaneuver 属事件池。已提交卡牌头文件254，新增3张已接入但未提交。

头文件、注册、方法和正版条件覆盖是不同事实，不能相除给整体完成率。能力/附魔基类行为可使 whitelist 大于头文件数。非零 CardCreationOptions.CardPoolFilter、EventCombatSynchronizer、部分先古和 reward alternative Hook 仍有 Unported；跨职业生成可触及未实现内容。不能过滤真实候选池、替换卡牌或补抽随机数掩盖缺失。

## 4. CombatSolver 核心替换与冻结

冻结前已有设置/UI、有限真实状态字段捕获、部分 Env 分支/查询及 Native 后继核对。**完整搜索核心仍未改为 Env，未完成可放入 mods 使用的双模式替代产品。**

- [EnvBackendSettings](../../tools/CombatSolverEnv/Runtime/EnvBackendSettings.cs) 固定拒绝原因 live_root_and_same_policy_adapter_incomplete，执行门禁关闭。
- [EnvBranchVerification](../../tools/CombatSolverEnv/Search/EnvBranchVerification.cs) 记录 backend=native_with_env_verification，Env 验证 Native 已算出的 replay。
- [EnvSearchBranch](../../tools/CombatSolverEnv/Search/EnvSearchBranch.cs) 定义历史/状态键/policy/threat 完整性要求；静态检索未发现 RequireExecutionReady 的运行路径调用，不能把该定义当成有效运行门禁。
- [CombatSolverEnv.csproj](../../tools/CombatSolverEnv/CombatSolverEnv.csproj) 仍编译 external/CombatSolver/src；比较入口还引用已移除的 sts2_env_solve_beam。任意 live 根、动态选择、完整树扩展、同策略/同预算性能比较均未验收。

旧 C++ prediction/Beam/终局旁路已移到 [archive/combatsolver](../../archive/combatsolver/)，普通 CMake 与公共 ABI 不包含它们。七个旧 managed 项目被 [Directory.Build.targets](../../tools/Directory.Build.targets) 拒绝构建。外部上游与历史适配器保留作记录，门禁不阻止直接构建 external/CombatSolver。

独立静态核对还发现游戏 mods/CombatSolverEnv 中保留 2026-09-26 的旧 DLL/共享库。旧库仍导出 solve_beam，当前库已无此接口；这个安装目录属于历史产物，不能当作当前 Env/RF 的交付版本，也不能直接替换其库宣称兼容。本轮未启动游戏确认加载情况，未改安装目录。

此前搜索跳过战后 Hook、回复、清理和奖励的逻辑已撤销，见 D-005。当前共享库确认29普通接口、0旧prediction/solve导出；活动代码静态核对未发现明显专用搜索行为，但这不是所有方法的形式证明。参考 ISMCTS 单独作为公共 Env 消费者保留。后续只补原版规则和通用合同；Env 完成不自动恢复 CombatSolver，恢复需用户后续明确要求。

## 5. Random Foreseer API 关键缺口

RF 需要局内/局外 RNG、遗物袋/概率、牌组与实例状态、连续 Hook 和结果来源。

现有 PreviewAction 必须是当前合法动作，Answer只能回答已存在的Choice。RF的遗物悬停拾取、变牌选择器、Pael Wing献祭、Crystal Sphere隐藏牌位等还需要精确来源和上下文。真实ShopBuy后的补货与假想悬停补货也需分别表达。依据当前 Preview/API和固定RF的 RelicPickupPrediction、PaelsWingSacrificePrediction、RunPredictionContext、MerchantRestockPrediction；后两者自己复制RNG/遗物袋/概率并重建公式，未来应由Env原版执行和结果合同承载。

| RF 功能组 | 可复用基础 | 关键缺口 |
|---|---|---|
| 抽牌/重洗/印牌/随机目标/药水 | 正常动作预测、选择、战斗查询 | 全内容/Hook、实例顺序、来源和分支验收 |
| 球/自动出牌/回合结束伤害 | 普通执行、部分 trace | 逐笔事件、来源实例、因果/顺序、停止阶段 |
| 变牌/遗物拾取/事件悬停 | 部分事件/奖励、before/after | 有原版来源及上下文的类型化操作、连续随机结果、事件/先古 |
| Courier 补货 | ShopBuy、库存/价格、有限原版路线 | 拾取 Hook、连续购买与补货组合 |
| Driftwood 重掷/PaelsWing 献祭 | 重掷路径/描述、有限合同 | 实际 REROLL 动作验收、献祭 alternative Hook |
| CrystalSphere/休息/下一幕 | 房间查询、部分动作、特权未来查询 | 隐藏位置、真实档案、完整跨幕和先古 |
| 真实游戏接入 | 模拟器快照、历史捕获研究 | 独立 SDK/live 桥、版本/所有权/能力、RF 公共接口消费者 |

需要形成五项公共合同：

1. **Capture/Validate/Restore**：所有房间静止边界、选择前缀、真实档案；稳定模型/FSM身份、版本和对象寿命。
2. **动作和类型化原版操作的纯预测**：表达悬停时尚未发生的变牌、遗物获取等操作，使用同一 Cmd/Hook/RNG。
3. **选择及停止边界**：操作完成、玩家选择、下个战斗决策、敌方阶段结束、安全胜负点、完整房间结算，由调用者明确请求。
4. **结构化逐笔结果**：生成/移除/目标/伤害/资源变化，来源实例、顺序及父子因果；退休对象仍可解释，选择重放不重复累加。
5. **公共 SDK 与支持/失败语义**：版本/能力发现，Exact/NeedsChoice/Unsupported/Truncated/Invalid等语义、失败源不变、可继续选择和容量限制。

Preview 仍明确 trace_complete=false；before/after不足以复原过程。C ABI 有成功、Unported、TraceLimit、Error，池容量失败仍归一般 Error，完整统一结果语义未形成。RNG及未来事实供特权预测/诊断，智能体观测继续遵守信息边界。

C# Mod 负责捕获、调用、缓存、展示、策略/评分及真实动作提交。捕获桥输入真实字段，效果在 Env 执行。RF 的近似推断不成为 Env 规则；RF 功能用作公共接口验收消费者。

## 6. 验证证据

已有正版轨迹被重放，没有重新采集同一语料。无 Neow语料为 SEED0001..SEED0004 × policy1,2；有 Neow为 SEED0002,SEED0003 × policy1,2，均 Ironclad A0、hp10000、stop after-act1。

| 本轮命令 | 结果及原始日志 |
|---|---|
| cmake --build build -j 4 | 退出0；/tmp/mcc-env-status-review-build.log |
| ctest --test-dir build --output-on-failure | 12/12，6.43s；/tmp/mcc-env-status-review-ctest.log |
| dotnet build tools/Sts2Oracle -c Release -v quiet -nologo | 增量构建退出0，0 warning/0 error；/tmp/mcc-env-status-review-oracle-build.log |
| python3 tests/l4/l4_run.py --trace /tmp/mcc-env-act1-endpoint-oracle.jsonl --require-complete --verbose | 8条/2903决策一致；/tmp/mcc-env-status-review-l4.log |
| 同上，trace改为 /tmp/mcc-env-act1-neow-oracle.jsonl | 4条/1003决策一致；/tmp/mcc-env-status-review-neow-l4.log |
| 无Neow重放追加 --verify-snapshots | 退出1，2一致/6失败，1015/2903；/tmp/mcc-env-status-review-restore.log |
| python3 tools/check_run_queries.py --trace ...，分别两份语料 | 合计12条/3906决策/12终点，零分歧零Unported；/tmp/mcc-env-status-review-queries.log、...-neow-queries.log |
| python3 tools/coverage/env_inventory.py | 退出0，库存见第3节 |
| nm -D --defined-only build/libsts2env_c.so，筛选公共导出 | 29普通接口，0旧prediction/solve |
| git diff --check | 退出0 |

三卡已有 /tmp/mcc-env-silent-shivs-final.log 为48场/990状态/101目标出牌；二幕阻塞 /tmp/mcc-env-act2-l4.log 为Orobas Unported。历史完整gate仍为L2 200、L3 80/1611状态、L4 80/4089决策零分歧，一组smoke6 Unported，见[已集成批次](2026-09-27-env-run-query-results.md)。**本轮未重跑完整scripts/check.sh，未合入main。**

默认 L3/L4 gate允许Unported；require-complete拒绝Unported，也不是全部内容覆盖证明。终点是死亡、指定边界还是胜利需另行统计。oracle仍用TestMode加逐方法生产分支恢复，存在登记的生产/真机验证边界。

## 7. 当前性能和容量

原版长战斗累计497张战斗牌，曾暴露160容量限制。当前card pool1024（1023可用）、pile512、状态日志1024，聚合CardList覆盖池。容量仍有限，未来统一截断/容量合同未完成。

本轮单独编译运行 tools/bench/env_api_bench.cpp，固定Ironclad/Cultists A0、DUAL001根，32次预热、9样本×1000次，含结果销毁；源Hash不变，Predict和trace开/关Preview后继一致。

| 测量 | 当前结果 |
|---|---:|
| sizeof(GameState) | 340560字节（约333 KiB） |
| Clone | 均值6.60µs，中位6.59µs |
| Hash | 均值10.46µs |
| EndTurn Predict | 均值200.50µs |
| Preview trace开/关 | 均值216.76/221.42µs |
| 快照JSON输出 | 均值122.78µs |

本机Intel i7-14650HX/GCC15.2、Release库，基准翻译单元O2；未固定频率/亲和性或跨进程重复。日志 /tmp/mcc-env-status-review-api-build.log、...-api-run.log；编译命令同[历史基线](2026-09-27-env-api-baseline.md)，输出改为 /tmp/mcc-env-status-review-api-bench。当前链接库SHA256：env 413bb05ba6e3082a8219b9bf3c637a69555aa687f7d049396a2b8fe418c7e036，game 034e506fc40c32092c21361100399fe9d32ef0943b78bc39f6f8d25984f798f5，engine f85480fc79c3a70b8654bcf5eb737eb3ac7c388996b32a8f6c6b6c63447c550e；content/core同历史基线。

旧145936字节、Clone2.91µs仅代表扩容前产物。当前未达到约8KB/Clone<1µs原目标；Step、ABI、RF使用成本、真机搜索提速未验收，不能用本次单点测量承诺替代游戏搜索后的速度。

## 8. 下一阶段和完成标准

1. **先修实际恢复失败**，按原版语义重新验收逐决策恢复与后继；建立方法/条件覆盖清单和严格终点门禁。
2. **贯通战士三幕**：先古（当前Orobas）、最后事件/胜利、父事件恢复、特殊奖励；经公共API同时验收预测、选择和结果。
3. **补内容与档案**：其余角色、跨职业、先古/事件、附件/修饰器、解锁规则；简单批量内容交GPT-6 Luna xhigh，主线程集成及正版验收。
4. **RF公共合同同步落实**：以抽牌、变牌、遗物拾取、Courier补货四条路径落实操作、停止阶段、结果因果与失败语义，再扩展RF单人矩阵。
5. **独立SDK/live桥与最终验收**：RF类消费者仅通过公共API工作；批准的单人范围全部可达方法/条件有实现和真值证据，真实房间/选择/档案可恢复，至少一万条轨迹零分歧零Unported并统计各幕/胜利，性能可复现。

完整标准按[通用Env路线](../plans/2026-09-27-generic-env-completion.md)执行。本轮完成勘察，Env/RF总目标仍在进行中，CombatSolver保持冻结。
