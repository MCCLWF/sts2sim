# 派生 Mod 的后端边界

上游基线为固定子模块；此文件维护派生项目新增的职责。上游架构见 `external/CombatSolver/docs/ARCHITECTURE.md`。

## 所有权

- `Runtime/EnvRootCapture.cs` 在上游主线程 Capture 的 before/after 稳定性检查内冻结种子、遭遇、进阶、幕与真实卡牌组件，创建根级 `EnvBranchVerificationContext`。变量/费用预热在 before stamp 前进行。Attach 只发布冻结值，不再在 after stamp 后读取 live 对象。后台不接收 live `CombatState`。UI provider 只在主线程读取诊断值并通过 `SolverText` 本地化。
- 派生 companion 的 `EnvLiveSnapshotProvider` 另在主线程冻结七个组件并组装 owned JSON，worker 仅导入该文档。失败标记 `root_source=live_capture env_imported=false`，不执行种子近似；成功的 reference Beam 诊断和 Native 路线重放各自拥有独立 session。此捕获还未绑定原 coordinator 的同一请求 root/token，不是正式同根/同政策执行路径。
- `Runtime/EnvLiveCards.cs` 将注册牌、历史牌及递归可达的克隆原件/牌组原件转为独占 `JsonElement`，保存 pool、deck 与五个牌堆。费用修改顺序/组合到期标志、临时星数费用、本地关键词、标量变量的精确 decimal 位、历史回合及引用按值冻结。未初始化的星数费用投影 canonical 值并保留 false lazy flag，不调用 getter；C++ 执行首次读取时按正版写入 cost 和 true flag，只读合法动作查询在副本上进行。未知类型、子类内部字段、特殊变量、变量属性变化、附魔/污染、Dampen 历史及容量超限拒绝组件；这不是完整根导入。
- `Runtime/EnvLiveMonsters.cs` 在同一稳定性边界冻结各实例 RNG、当前/初始状态、NextMove、行动历史、首次行动和每个招式的运行标志、动态 follow-up 及已适配扩展字段。冻结值仅含 JSON，工作线程按状态名绑定当前 Env 注册表。常量闭包读取经过严格 IL 检查的标量，不执行委托；动态伤害/重复/权重和未知条件回调拒绝。当前条件适配仅 Toadpole 的前后排分支，并核对两个原版回调的准确身份，交换也拒绝。
- `Runtime/EnvLiveRunContext.cs` 冻结实际 15 条 RNG、精确 float odds、坐标、地图/幕/遗物袋及人物运行标量。`EnvLiveCollections` 读取 CLR 槽位/移除链以保留未来枚举和插入语义，未知 comparer 拒绝。实际 run/player 档案独立保留 epoch/seen/runs；`RequireSupportedProfile` 对不满足目前全解锁假设的源显式拒绝，不改写档案或冒充支持。
- `Runtime/EnvLiveCreatures.cs` 以和卡牌相同的顺序冻结人物/敌人标量、能力顺序、owner/applier/target 及受审计的内部计数/精确变量。怪物绑定同时验证 CombatId 和模型 ID。HpDisplay 作为玩法字段精确冻结；未知内部对象/字段、无法保留 decimal 位的整数槽及外部引用拒绝。HardenedShell/Hellraiser 与额外回合 Juggling 语义已有独立原生验证，但不代表全部历史闭包完成。
- `Runtime/EnvLiveInventory.cs` 冻结活跃遗物、药水空槽、充能球与队列；遗物变量保存精确 decimal，药水可变变量必须仍等于 Env 固定定义，Dark/Glass 保存实例值。未知子类字段、排队药水和多叠遗物拒绝。活跃池本地编号尚未证明历史分配游标或所有已移除对象闭包。
- `Runtime/EnvLiveCombatContext.cs` 捕获管理器/队列的实际静止边界、遭遇 RNG/生成列表、资源与目前 C++ 使用的历史聚合；额外回合按原版 HappenedThisTurn 的人物回合号计算。history sidecar 保存这些查询的依据，未保存原版完整历史载荷。执行中效果、暂停选择、已排队结束回合和未知遭遇字段拒绝。
- `Runtime/EnvLiveRoomContext.cs` 只接受单层普通 CombatRoom 的静止边界；它逐项冻结奖励、RelicObtained 订阅、Reward/Event/RestSite/Treasure synchronizer 的空 bookkeeping。普通房间可保留已完成且已清理的 Neow：事件 RNG、描述键、疗愈量、完成页和同步器事件/投票/页码会进入通用状态；未完成事件、未知事件类型、共享页、待处理任务和事件战斗状态仍拒绝。CardReward、未知订阅、额外奖励、嵌套事件房间和任一残留队列都拒绝，presentation-only 订阅只记录类型与数量。
- `Runtime/EnvLiveRootAssembler.cs` 从 C++ seed-free blank template 开始合并已拥有的 live JSON。模板的 `GameState{}` 和 `monster_default` 不带新局种子或 RNG draw；未映射的奖励、事件、休息点、宝箱、商店字段只有在 room contract 证明为空且模板默认仍为空时才保留。Run/Player/Combat/PlayerCombat 的每个字段必须有唯一 live 写入者；重复捕获只核对相等性，缺项、冲突和新 schema 字段拒绝。仅五个室外默认对象可在完整形状与空状态证明后保留，GameState 顶层也有完整字段门禁。任何缺项通过异常停用该根，不能降级为种子近似。
- 内容导出器刻意省略引用/自动属性字段；`EnvModelCatalog.RequireSubclassFields` 另查实际子类实例字段，避免将表中缺项当作无状态证明。变量预热扩展到能力与库存，仍在 before stamp 前执行。
- `sts2_env_export_monster_machines` 只读取当前分支的不可变注册元数据，提供状态、动作回调名、意图与分支定义及 runtime/hash。`SIM_MOVE` 的名字属于注册表，不进入 POD 状态。公开 `InspectMonsterMachines` 返回独占 JSON；索引只在当前 runtime 内有效。
- `prepare_card_catalog.py` 从固定 content/生成 IDs 构建内嵌卡牌表，检查非 mock ID 集合，保留变量升级基线和待映射子类字段。运行时核对正版游戏程序集 hash 与活跃卡牌类型/entry；游戏网络 ID hash 包含已安装 Mod 的注册类型，不用于判断 Env 私有 ID 表。表格身份不等于内容移植覆盖；未知玩法 Hook/补丁仍须在完整根边界审计。
- `Search/EnvBranchVerification.cs` 对 Replay/ReplayAction 的 native 结果建立对应 Env 分支。普通根与完成回合准备后的根分别校验；准备前的空辅助 Replay 被延后，挂起准备选择明确拒绝。已观察快照不会重复预测。节点句柄现在使用独立 `EnvSearchBranch`，内部持有 `EnvCombatPredictionSession`，根、兄弟、终局和失败路径都有独立释放。
- 每个 `SimulationSnapshot` 独立拥有一个 Env session；原生 `ReleaseSimulator` 同时释放 session。未转交的临时后继总在 finally 中释放。根 context 不持有 native 句柄。
- 原生并行兄弟分支会读取同一个 Env 源。`NativeEnv.Session` 对 inspect、clone、predict 和 dispose 使用同一把锁，保护 C ABI 的诊断缓冲区和句柄生命周期；后继之间独立运行。
- 验证失败只停用该根的 Env 核对；不跳过 native 候选、不更改评分或部署结果。首次原因用原子操作登记。总计日志标明 root 累计口径及 native 耗时包含验证，不能与单次 solver 工作量直接等同。
- `Api/EnvPredictionApi.cs` 是公开的纯分支接口，只暴露动作、状态检查、所有权和失败分类。没有 native Search 内部类型或 live 场景对象。可建立明确命名的种子战斗或恢复完整显式快照；live mapper 与真实游戏接受验证是独立前置条件。
- `CombatPredictionSession` / `Api/EnvCombatPredictionSession.cs` 是原搜索专用的独立执行 scope。正常 run Env 的战后 Hook/奖励仍照常执行；scope 只在原安全胜负点记录实际玩家回合/胜负，保存评分所需的战前清理状态。临时 boundary 指针由 RAII 保护，每个新 Sim 从 null 开始，不进入 POD GameState。终局无合法动作，继续预测拒绝。
- 预测外层 `sts2env.combat_prediction` v7 保存普通快照、terminal、choice boundary baseline 和独立 hash 域。原始历史为 20 列执行事实，包含每次进入 OnPlay 的 `CardEffectStarted`，C++ 不记录搜索 Risk。派生 C# Mod 投影回原 21 列历史，在固定默认 inference 下恢复 Rampage/VoidForm 的风险分派、Self 策略目标、六个计数和 deferred draw/generated 的原始历史索引；非默认 inference 明确拒绝。16 个原 Beam fixture 已通过有限历史/语义对拍，但 `history_projection_complete=false`，其他方法/Hook/限制风险与完整历史仍未覆盖。Clone 复制 owned 分支及基线；Answer 同时恢复 GameState 与整个 boundary，然后重新执行答案前缀。导入 choice 必须能在相同 scope 重现；普通游戏快照中的待选择状态因没有历史基线而被拒绝。旧外层版本拒绝。终局允许非 Play/非玩家侧，普通快照恢复仍只接受 active 决策。快照是受信特权诊断输运，hash 不证明已经清除的历史 PendingLoss。
- `env/combat_prediction_c_api.cpp` 采用与普通 Env 不同的 opaque 句柄类型；所有失败的后继为 null。raw C ABI 由调用者串行访问同一 handle，输入可借用该 handle 的 result 缓冲，函数先复制再清缓冲。`NativeCombatPrediction.Session` 用同一把锁保护 native 调用、读缓冲及 Dispose，独立后继并发运行。native 库配置按程序集加锁且幂等，失败初始化会释放库。
- `env/snapshot.cpp` 使用生成的命名字段遍历导出全部 Env 逻辑状态；`Api/EnvStateSnapshot.cs` 拥有独立 JSON。浮点/decimal 按原始位编码，RNG 保留完整 64 位值；池槽/空闲链保留未来分配语义。版本 2 带当前 native runtime 身份。
- `Env::ExportBlankTemplateJson`/`sts2_env_export_blank_template` 提供不改变 handle 状态的 seed-free mapper 模板；外层是 `sts2env.blank_template` v1，内嵌协议 v2 快照的 decision/hash/pending/choice 均为空，并附独立 `monster_default`。模板不是可导入战斗根，供 live assembler 做严格起点。
- `env/snapshot_import.cpp` 在新的 Env 中解码完整文档。`snapshot_json.h` 限定输入大小、复杂度和严格整数 JSON；命名 visitor 对缺失/未知字段失败。容器解码检查容量/槽号/游标与空闲链，`snapshot_validation.cpp` 检查战斗引用及状态机注册归属；choice 通过重放证明当前状态/请求一致。任何错误销毁临时 Env。C ABI 拷贝输入后才重置诊断缓冲区，允许输入引用自身 last_result。
- `RestoreCombatSnapshot` 与 Python `restore_combat_snapshot` 返回独立 owned 分支；runtime/schema 不匹配明确拒绝。内部完整 fresh combat 文档可把 hash 设为 null，由 native 计算；已有 hash 必须吻合。部分选择禁止省略 hash/replay base。
- 原生 Replay 根会走一次种子快照导出/导入，以在正版 Mod 中验收 ABI/所有权路径；日志标明 `root_source=seed_snapshot_import`。这不是 live mapper，不能把种子源的恢复包装成实际游戏状态捕获。
- `Search/EnvRngVerification.cs` 在结构核对成功后读取完整 Env 快照，并将九条战斗流与原生 branch `*State` getter 的冻结值核对；不物化原生 lazy RNG。纯比较与逐字段原因位于 `Api/EnvRngComparison.cs`，同样供独立 managed 检查使用。
- `UI/English.additions.json` 与上游英文目录在构建时合并为同一内嵌资源；重名拒绝构建，防止覆盖上游译文。

## 生成入口

`prepare_sources.py` 检查并替换根预热/稳定性检查/构建/返回、回合准备 Snapshot、Replay 结果、ReplayAction 的两个返回、快照释放和 Solve 结束。生成文件放在 obj 下，csproj 精确排除对应上游原文件。其余搜索源码直接链接；这些入口不修改算法政策。锚点变化需要人工审查。

## 当前验证合同

投影原生 shadow 的四个牌堆与部分卡牌实例字段、遗物熔化状态、药水槽、能力数量和人物/敌人标量，并核对九条战斗 RNG 的 counter 和四个状态字。核对使用 `SimCreatureState`/`SimPlayerCombatState` 和预测模型，不读取 live 的 HP 或牌堆。核对成功仅意味着这些结构字段与随机流一致。

真实卡牌、怪物、运行、库存、战斗和普通房间组件已进入冻结根；Run/Player 的实际档案进入通用 POD 状态，已完成且清理的 Neow 记录也可进入通用事件槽。部分档案可以作为状态导入，但玩法动作在通用 Env 入口明确拒绝，直到档案相关规则完成。当前安装游戏的隔离启动器仍在未选择 Neow 的根上以 `live_event_pending:1` 拒绝完整根；这不是丢弃事件状态的理由。完整组件通过时才会从 blank template 组成 `live_snapshot_import`，否则保持 `seed_snapshot_import` 并停用 Env。`EnvLiveCardChecks` 只在明确的混合测试夹具中验证组件的 C++ 恢复与预测。怪物测试的预期来自正版 FSM 推进 API；该夹具不比较攻击/能力等战斗效果。

新增的根级怪物 RNG/FSM 核对在隔离正版游戏中发现个人 RNG 与种子模板不同，已停用 Env 分支诊断并显示 Native 继续执行。`--verify-branches` 的自然根接受门禁因此目前失败；`--expect-monster-root-rejection` 单独验证这次拒绝，不代替接受门禁。见[怪物组件报告](../../docs/reports/2026-09-26-combatsolver-env-live-monsters.md)。

- 未适配项包括完整 live 导入验收、其他模型内部计数/动态变量、其他怪物的条件/动态定义与扩展字段、stun、非战斗流、全部历史、选择答案及终局边界。普通战斗房间边界现在检查额外奖励、RelicObtained 订阅和 Reward/Event/RestSite/Treasure synchronizer 的 stack/buffer/bookkeeping，非空时拒绝；这仍不是奖励对象闭包。怪物组件仅核对实际冻结根，不声称已核对每条 native 后继的完整 FSM；原生 worker 的机器定义不代表 live 可变运行标志。卡牌子类与附魔/污染仍待补齐；pool UID 为本地重新分配，历史分配顺序及独立注册表顺序尚不能作为已保真的证据。完整 Env 导出不能证明 native 已冻结或导入了对应字段。`SimulationSnapshot.EnvBranch` 现在保存的是诊断用 `EnvSearchBranch`，不是已接入 Env 搜索执行后端的证据。设置页的 Native/Env/Both 现在会控制 companion 入口：Native 跳过 Env，Env 不支持时恢复 Native，Both 才等待两条路径；这仍是参考 Beam 核对/部署，不是原策略已切换到 Env。下一步仍须让完整字段合同和同策略 Env 读写/转移接口通过连续回合、选择、部署验收。
- `EnvSearchBranch` 已提供未来 P4 执行适配所需的 owned clone/predict/inspect/query/terminal 读取边界；它只承载动作身份映射与 C++ 预测转移，尚未提供 `SimulationSnapshot` 的完整评分/状态键投影，因此不会解除正式 Env 搜索门禁。
- `CaptureCalculatedVars` 是独立 v2 游戏纯查询：C++ 私有副本对 13 个已审计模型输出正版 `Calculate` 值、CalculationBase/Extra 与 nullable target 的原始公式乘数，保持全部 decimal 位；不输出原预测值/语义标签。C# Mod 用固定原 `Base + Extra * multiplier` 规则派生终局仍读取保留状态的 `OriginalPrediction`。Live mapper 检查 owner/类型/属性/canonical 静态 delegate；未知公式拒绝。原 Beam 终局 BodySlam 差分验证两种语义不同。此输入不意味着完整 score/policy 查询已完成。
- `CaptureOrbs` 是独立 v2 语义游戏查询，返回容量/队列顺序和被动/激发值；私有副本上的 Hook/getter 必须保持完整状态 hash 不变。C++ 查询协议没有原搜索键完整性字段。五种球、Focus、Dark/Glass 的 exact bits 已与正版和原 OrbMirrors 对拍；Mod 的 `EnvSearchOrbFingerprintProjection` 使用固定原版 `AppendOrbs` 输入编码，节点诊断实际调用原 `AppendOrbs` 核对。Orb 输入对拍通过不代表完整原 key；历史池/其余账本待补齐。
- `EnvSearchProjection` 是该读取边界的第一层不可变合同：复制 turn/phase/energy/stars、牌槽、玩家/敌人生命和格挡、九路战斗 RNG、事件数量与六计数；`OriginalStateKeyReady` 固定为 false，缺少卡牌动态模型指纹、orb/战略账本或风险条目时抛出 `EnvProjectionIncompleteException`，绝不以 `Env.StateHash` 或空值冒充原键。
- `EnvSearchCardFingerprintProjection` 从 v4 getter 的 `card_state` 读取完整变量列表及每项的 `is_calculated`/`is_string` 标志，按原 `CaptureCardStateFingerprintForTesting` 顺序编码普通卡牌和 Rampage 特殊增长状态，复用固定 upstream 的 `StateFingerprintBuilder`，精确保留 decimal 位与动态变量 unordered mix。Mod 排除原键忽略的 calculated/Derived 变量；Subclass 字段、附件或 String 变量输入缺失时拒绝，不能从部分 `card_state` 推断完整。Thrash 的增长不额外加入原键；原 Beam 后继诊断逐牌核对该指纹。它不提供完整状态键，剩余 orb/政策/风险等门禁仍关闭。
- `CapturePowerQueries` 是通用 v1 owned 查询，提供未筛选的能力事实和 `power_events`。Mod 的不可变 `EnvSearchPowerLedger` 根据通用事件维护根上的退休槽位，并复原原 `P+r` 聚合语义；根退休/重新获得场景与 16 个原 Beam 场景的个体及聚合编码已对拍。查询层不预先过滤模型或变量；Mod 指纹编码仍拒绝未知模型、缺失字段和未分类 StringVar。完整 state key gate 仍关闭。
- `Api/EnvCardPlayPolicy.cs` 在 Mod 中从游戏 getter 事实生成 `CapturePolicyProbe`，保留原 `CanPlayCard_core` 规则。C++ probe 实现及对应 ABI 已删除；原规则在 C# 适配层执行，而不是把其部分复刻留在通用 Env。协议/投影当前 `Complete=false`，Hook、AI 威胁、选择/生成策略等仍未覆盖，正式执行继续拒绝。
- `EnvSearchThreatProjection` 对齐 getter 中的怪物 roster、move ID、stun/attack 标记、多段重复次数、状态数量和原始 decimal damage bits；它只保存值副本，`Complete=false`，因为原 `ProjectHpAfterThreat` 仍需要 Hook 后伤害、状态效果和完整敌方生命周期。

- `EnvSearchBranch.RequireExecutionReady()` 是未来同策略执行入口的硬契约。它按历史账本、原状态键、候选政策和威胁投影顺序拒绝任何未完成适配；当前这些投影仍明确不完整，因此不会解除正式 Env 门禁。

- `EnvLiveCardChecks/LiveRootChecks` 是 TestMode 关闭的有限整根 fixture，覆盖 Strike/Rampage/Thrash/Thrash+BodySlam/BodySlam/Barrage。`LivePowerAllocationTrace` 观察 native PowerApplied/PowerRemoved 同步次序并维持 CLR→pool ref；value 读取当前 native 对象，包括已观察移除后仍持有的对象。固定 roster、已观察事件以外拒绝。当前生产 capture 每个根自建引用命名空间，尚无通用跨根历史池/未应用分配映射。
