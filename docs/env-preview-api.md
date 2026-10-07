# 通用 Env Preview 与查询 API

2026-10-01 交付范围调整为基本 RF 与容量/数值极值：仅全解锁档案，状态输入见
[输入规范](env-state-input.md)；实机捕获、悬停 UI、部分档案、完整因果账本不属于本轮交付。
下文未完成项保留能力边界，不作为本轮必须开发的高级功能。

游戏基线 v0.111.0。此接口执行与普通 `Step` 相同的原版流程，返回独立后继；胜利仍执行战后 Hook、回复与奖励。它当前提供一段合法动作的预览，不提供搜索策略，也未完成 Random Foreseer 的全部功能矩阵。

## Python

```python
from sts2sim_env import Env, TraceLimitExceeded

env = Env()
env.inspect_combat_named("DUAL001", "CULTISTS_NORMAL")
action = env.legal_actions()[0]
branch, result = env.preview(action)
assert env.state_hash == result["source_hash"]
assert branch.state_hash == result["successor_hash"]
# branch 是普通 Env：可以继续合法动作、导出快照或恢复选择。
restored = Env.restore_snapshot(branch.export_snapshot())
portable = branch.export_portable_snapshot()
# 可保存并在加载相同 native build 的另一个进程中恢复。
portable_restored = Env.restore_snapshot(portable)

# 局外与选择的原版事实，默认不包含未来幕。
facts = env.export_run_queries()
branch, result = env.preview_with_results(action)
before = result["run_results"]["before"]
after = result["run_results"]["after"]
```

`preview(action, trace_limit=16384)` 返回 `(Env, dict)`。`trace_limit=0` 禁用观察；正值为帧、两类事件和RNG调用记录合计的容量上限（最多 1,000,000）。容量不足抛出 `TraceLimitExceeded`，没有可用后继，源状态不变。需要选择时得到普通 Choice 后继，通过 `legal_actions()` 中的 Answer 动作继续。

四种纯查询：`export_combat_queries()`、`export_calculated_vars()`、`export_orbs()`、`export_powers()`。它们返回原版字段、getter 和公式操作数，在私有副本上运行 lazy getter；源状态与 RNG 不变。查询需要活跃战斗，包括战斗内选择；遇到未实现模型明确拒绝。查询不包含评分或客户端筛选。

`export_run_queries(include_future=False)` 返回 `sts2env.run_queries` v1：玩家牌组、遗物、药水槽和资源，当前地图（点类型、子节点、当前位置及 Fur Coat 标记）、当前奖励、商店、事件、休息、宝箱、合法动作，以及选择的实际候选。`is_victory` 区分已结束的胜利和战死。商店同时给出原版 Hook 后价格、是否有货/可购买及对应 `ShopBuy` 的 `action_index`。奖励领取完仍显示的界面保留 entries，并明确 `active=false`。地图查询只返回当前可见地图，不返回 RNG、未来房间队列或遗物袋。

`include_future=True` 显式开启下一幕已经预选的幕、先古与 Boss 事实；默认 `future=null`。这些是预测消费者的特权查询，`is_agent_observation=false`；RNG、未来房间队列与遗物袋不在这个输出中。强化学习的固定观测仍使用 `Observe` 与 `Determinize` 的信息边界。

`preview_with_results(action, trace_limit=16384, include_future=False, include_rng=False, include_causal=False)` 返回普通 owned 后继与 `sts2env.preview` v2。在原有 trace 之外，`run_results.before/after` 分别包含当前源/后继的 run queries。每份投影都标明自己的 `state_hash`；它们可以用于牌组变化、奖励与补货的展示。结果捕获只在显式请求时增加源分支与查询成本，普通 `preview` 继续返回 v1。

## C++ 与 C ABI

- C++：`PreviewAction(const Env&, const Action&, const PreviewOptions&)`，定义见 `env/preview.h`。
- C ABI：`sts2_env_preview(source, action, trace_limit, &successor)`；后继是普通 Env handle，结果 JSON 从后继的 `sts2_env_last_result` 读取。下次调用会替换该诊断缓冲区，应及时复制。
- 动作列表、合法 mask 和 observation 都支持长度发现：先用空/零长度请求取得所需长度，再分配调用者缓冲区；
  不要把历史的512/2048字节值当成协议上限。快照/JSON和恢复存储受 `STS2_ENV_MAX_*` 资源预算约束，
  预算不足返回错误且不产生后继。
- `sts2_env_export_run_queries(handle, flags)` 只接受 0/1，对应默认/特权 future。
- `sts2_env_preview_with_results` 与 `sts2_env_predict_operation` 的 flags 是位掩码：1 为特权 future，2 为特权 RNG，4 为特权model/Hook帧，可组合0..7；默认0。其它位拒绝。RNG与model/Hook诊断需要非零 trace_limit，否则返回 InvalidRequest 且没有后继。
- 四类查询和整局快照恢复在 `env/c_api.h` 中定义。由调用者销毁成功返回的 owned handle。
- 状态码：0 成功，1 未实现，2 追踪容量不足，3 不支持的操作/停点，4 原生 POD 容量溢出，5 无效或过期请求，-1 其他执行错误。失败后继为 null，错误从源/诊断 handle 读取。`Step` 是可变操作，不能套用纯 Preview 的失败原子性。
- C ABI 的 action kind 和 col/row 在转成 enum/int8 前校验；非法数值不会窄化成合法动作。
- 原 `sts2_env_solve_*`、`sts2_combat_prediction_*` 已退出当前 ABI。旧调用方需要迁移；冻结的 CombatSolver 工具不能用作当前 API 门禁。

## JSON 与选择重放

`sts2env.preview` 返回 `source_hash`、`successor_hash`、当前决策、动作及实际 `stop_reason`。
`operation_status` 明确区分 complete、needs_choice、paused；complete 表示本次操作已执行完毕，
不代表全部规则/trace 已完成验收。停点与 choice 返回可继续的普通 owned 后继。

`trace_semantics=operation_prefix`：若动作是 Answer，Env 从静止根重放整个待处理操作及全部答案。因此每次结果是**替换前一次该操作的部分追踪**，客户端不要拼接。结果不累计先前已完成的其他顶层动作；`choice_replayed` 标明是否执行了重放。

v2 的 `run_results.semantics=source_to_successor_projection`：Answer 的 before 是当前部分前缀状态，after 是重放后的状态；trace 从静止根开始。投影与 trace 的起点不同，不能把 before/after 当作整段操作的因果账本。Pool ref 仅在相应 owned 状态内有效，不保证跨分支、跨进程或对象回收后的全局身份。

`choice.options` 保留原版调用生成的 card/bundle/reward/alternative/proceed 类型、卡牌分组、奖励引用和 alternative 原始键（`Skip`、`REROLL`）。多选先回答 count，再回答候选 index；`answer_semantics` 与 `legal_answers` 表示当前问题。选择描述已进入 Hash 和快照。选择与重掷沿用现有执行路径；更多由 Hook 添加的 alternatives 仍须补齐。

每个 option 的 `enabled` 与 `legal_answers` 表示实际可操作性。backend 保留的已领取奖励、
或重抽后 UI 已移除的旧 REROLL 索引仍可用于解释前缀，不能再次执行。
availability 纳入 Hash、snapshot 和 engine 前缀校验，恢复不能重新启用过期选项。

- `frames` 以数组下标引用父帧，包含来源类型、模型和操作。当前只覆盖已安装的观察点。
- 帧进入/退出、`events`、`power_events`与RNG调用共用`sequence`，可据此合并还原已观察的顺序。`original_event_index` 引用 `events` 数组下标，与 sequence 不同。
- `damage_resolved` 独立记录实际伤害结果，包括致死命中；`damage_received` 遵守游戏 History 原本的记录条件。观察不扩大原版历史、不改变玩法计数。
- 部分事件冻结牌/播放/伤害事实及战斗 ID，对象清理后仍可读取；pool ref 属于该 owned 状态，不能作为跨对局身份。
- **`trace_complete=false` 始终明确标记当前追踪不完整**。尚未全面覆盖 Hook 因果、来源实例、全部局外结果和对象生命周期；已增加显式特权 RNG 调用观察，仍不能据此宣称完整因果追踪。

## 特权 RNG 调用诊断

默认 `rng_trace_enabled=false`、`rng_calls=[]`。Python 的 `include_rng=True`、.NET 的
`PreviewWithResults(..., includeRng:true)` 或 typed operation 的 `queryFlags:2` 显式开启。
C++ 使用 `PreviewOptions.collect_rng=true`。这不会改变普通 Observe/Determinize 的信息边界。

每条记录含 operation、参数与返回的原始类型及整数位模式、前后四个 Xoshiro 状态和 counter、
父调用、对应因果 frame、开始/结束 sequence。Gaussian 重试、重载调用、随机下标、权重选择及
Shuffle 的子调用保留原顺序。权重列表另存 float32 位模式；索引助手的空值/fallback 以 int32 -1 编码。
Void 结果为 `{type:"void",bits:0}`。Sequence 与现有事件共用顺序，但尚未覆盖每个 Hook。

流归属按实际 owned GameState 中的地址绑定 run/player/event/encounter/monster；不输出内存地址。
局部或复制的 Rng 标记 `scope="unclassified"`，目前不推断它们的持久身份或对象寿命。
owner_ref 仍服从相应 owned 状态的引用范围。Answer 返回整段重放前缀，RNG 记录也替换上一段。
容量计入 RNG 记录与因果帧；失败丢弃后继并恢复借用的观察上下文。

独立真值工具为 `tools/RngExecutionChecks` 与 `rng_execution_checks`：前者只观察正版 DLL 原始方法，
后者重放顶层参数并逐项比较全部内部调用、状态、返回位模式及洗牌结果。它验证 RNG 观察层，
不能代替整局/RF 全来源矩阵或实机验收。

## 特权模型 / Hook 调用帧

Python `include_causal=True`、.NET `includeCausal:true`、C++ `collect_causal=true`或typed flags4
观察实际generated model dispatch与Hook入口。默认关闭；这些调用会暴露隐藏牌堆的监听者身份/顺序。
单独开启RNG不自动开启model/Hook帧，所有组合仍执行同一原版流程。

`frames.kind`区分`operation`、`rng_call`、`model_method`和`hook`。
模型帧在入口冻结`source_model_kind/source_model_id`、`source_is_override/source_is_canonical`及
`source_ref/source_ref_kind`；后者区分pool、event_slot、combat_encounter、card_attachment和pending_definition。
`CanonicalCard/CanonicalRelic/EventModel.Canonical`模板帧为`source_ref_kind="canonical_definition"`且
`source_ref=0`，身份是model kind+ID，不暴露临时scratch槽。方法值的非null `model_ref`新增
`is_canonical`；模板ref同样为0，可变实例ref仍是状态内存储引用，不能当稳定寿命UID。
帧的`source_is_canonical_known`区分已表示的事实与未知模型表示；未表示的模型参数
`is_canonical=null`且`value_complete=false`，不能将未知当作可变实例。
模板身份在具体类派发中保留；同一定义模板相等，模板与其可变副本不相等。
该合同只覆盖已表示的三类模板handle；scratch字段读取仍需要有效承载存储，保存的诊断ID不保证
任意失效handle能继续执行玩法。全部对象创建/退役/回收和引用重用账本尚未完成。
`parent`是本结果frames数组下标；`sequence/finished_sequence`是进入/退出顺序，
`finished/unwound`区分已退出及异常展开。Choice/Boundary也是当前调用栈的展开，后续Answer/Resume
重新执行完整操作前缀，客户端替换上一段帧。引用仍只属于相应状态，不是对象寿命身份。

当前263个generated方法与144个Hook定义安装作用域，不代表全部条件的独立真值验证。
显式特权调用帧还提供方法值：`parameters_before/parameters_after`按正版DLL声明顺序记录
`name/declared_type/is_out/is_by_ref/is_in/value_complete/value`。方向来自固定版本DLL反射，
不把C++引用猜成`ref/out`。`out`入口是`{kind:"undefined"}`，正常完成后才记录实际值。
`method_values_observed`标识入口观察，`method_returned`标识正常完成；Choice/Boundary展开时
完成参数为空、`return_value=null`，不能把`finished=true`（帧已经退出）当作正常返回。

`declared_return_type`保留C#声明；`return_value/return_value_complete`给出冻结值和编码完整性。
decimal四个32位字、float/double位模式、可空引用、集合实际顺序和已支持的复合字段保留。
Task原版await在C++同步执行完毕记录`awaited_completion`，Task<T>记录等待后的T值；
没有Task对象身份。实例ref仍绑定当前状态，不能用于永久寿命识别。
未能识别的delegate只记录存在，`value_complete=false`；编码完整也不表示所有条件都经过原版验证。

`tools/ModelExecutionChecks`只观察三个同步decimal模型方法及两个damage Hook的正常完成，
32输入/720模型调用/784帧、4752入口及4752完成参数、64个out集合逐项对齐；
每组观察输出还与安装观察器之前的原版结果比较。此探针不覆盖Task完成或全部参数类型。
非generated回调、完整delegate值、对象创建/退休与引用重用仍待补齐，`trace_complete`保持false。
见[方法值有限验收](reports/2026-10-01-env-method-value-trace.md)。

## 恢复范围与剩余工作

当前 v4 logical snapshot 可恢复模拟器自身的地图、战斗、奖励、事件、休息点、商店、宝箱、结束决策，及已支持 action/typed operation 的选择前缀和 Boundary。导入时从静止根重放验证；旧版本明确拒绝。它仍绑定当前加载 runtime，不能作为跨进程持久存档。

显式 portable 协议为 `sts2env.portable_snapshot` v1。C++ 调用
`ExportPortableSnapshotJson()`，C ABI 调用 `sts2_env_export_portable_snapshot()`；导入共用
`ImportSnapshotJson()` / `sts2_env_import_snapshot()`。它携带实际创建的怪物状态机定义、
稳定 native 回调符号和完整事件 LocKey 字典，规范化 runtime 下标后在目标进程重新绑定。
`states` 注册顺序和所有 POD/池/RNG/选择前缀保留，Choice/Boundary 仍须从 stationary base
完整重放证明。未知回调、模型混用、错误定义/字典/前缀拒绝且无可用后继。

portable 快照必须匹配源码/内容/schema、编译器/目标/配置构成的 `native_build_sha256`，
以及 `native_runtime_abi`（数学运行时与浮点控制配置）。不承诺跨构建升级或跨平台迁移。
`wire_hash` 覆盖规范化状态、定义和字典，用于完整性校验，不是 native `source_hash`，
也不是签名。恢复后客户端从新 owned Env 获取 source hash，再提交状态引用请求。
pending typed operation 的根 hash 由导入器按本地 stationary base 重绑定。
portable 导出包含特权隐藏状态，不能作为 agent observation；它也不提供原版实机 capture。

## 类型化原版操作与安全停点

`sts2_env_abi_version()` 与 `sts2_env_api_descriptor()` 提供 ABI 1 合同、构建内容/schema 指纹、枚举、所有权及能力限制。Python 可读 `API_DESCRIPTOR`；独立 .NET 9 SDK 位于 `sdk/dotnet/Sts2Env`。

`sts2_env_predict_operation(source, utf8_json, length, trace_limit, flags, &successor)` 接受
`{format:"sts2env.operation",version:1,source_hash:uint64,kind:string,stop:string,params:object}`。
它返回 owned Env 和 before/after 原始事实，始终保留源状态。`source_hash` 必须匹配当前完整 Env；
refs 与商店 index 必须使用取得它们的那份 facts query hash，SDK 不自动更新旧引用的身份。

| kind | params 与执行合同 |
|---|---|
| action | `kind,index,target,col,row`，当前 LegalActions 中的原版动作 |
| obtain_relic | `relic_id`（canonical 字符串）或 `relic_ref`（live unowned instance），加 `insert_index`；调用 RelicCmd.Obtain |
| transform_cards | 有序 `cards` refs；`rng_source` 为 player_transformations/run_niche/run_combat_card_selection/event；`mode` 为 batch/sequential，分别复现原版批量/逐张调用 |
| reroll_card_reward | 当前可重抽 `reward_ref`；调用 CardReward.Reroll |
| merchant_purchase | 当前 `entry_index,ignore_cost`；完整 purchase wrapper 包含取得效果、补货和购买 Hook |
| generate_rewards | `room` 为 MONSTER/ELITE/BOSS/TREASURE；只允许胜利 SafeOutcome，继续同一原动作的实际结算 |

`stop` 默认 operation_done；另外支持 enemy_phase_done、safe_outcome、enemy_phase_or_outcome。
只在效果深度为零的原版检查点暂停；未到达指定停点时完成原操作并报告实际 stop_reason。
Boundary 后继只允许 Resume；若后续出现选择，Answer 保留已跨过的停点序号。
新 typed Action 在默认 `operation_done` 下也保留完整请求，遇到 Choice 时写入 snapshot 的
`pending_operation`。typed 或普通 Answer/Resume 都续接该 root，原 typed context 以
`continuation_of` 返回；不把本次 Answer 请求替换为悬挂操作。其 trace 是重放后的完整 prefix。
当前 trace 仍不完整，类型操作/SDK 合同测试不能代替 RF 全矩阵的正版差分验收。

真实游戏捕获各房间、部分档案、任意事件同步状态、全部原版内容/条件路径、更多嵌套选择仍未完成。C++ 新局参数现可携带当前角色的历史胜场数供 The Architect 选择对话，C ABI/Python 重置入口尚未接受非零胜场配置。当前可以预览合法奖励/购买/拾取动作并继续变牌等选择；正常动作不能表达的悬停操作、其他停止边界、完整结果与因果类型仍待补。当前 API 不代表 RF 全功能或通用 Env 完成。定向查询对拍见 [本批报告](reports/2026-09-27-env-run-query-results.md)。

## Crystal Sphere 事件查询

`run_queries.event.crystal_sphere` 在该事件棋盘初始化后返回 privileged 原始事实，
初始化前及其他事件为 null。字段包括 `width/height`、`active`、`tool`（1 小工具/2 大工具）、
`divinations_remaining`、`grid`（按 y 行再 x 列的 121 格）、`items` 的类型/尺寸/位置/value、
`revealed_bits` 与 `revealed_order`。grid 编码：0 隐藏空格、1 已清空格、2..16 隐藏物品
（index=值-2）、18..32 已清物品（index=值-18）。隐藏棋盘不进入普通 Observe。

使用源 hash 绑定的 EventOption typed Action 开始，得到 `Choice.source=crystal_sphere`；
Answer 0..120 是行优先 cell，121/122 切换小/大工具。重复点击仍消耗次数，工具切换不消耗。
最后一次点击可能停在真实奖励选择，调用者继续 Answer；不会自动领取或跳过。
当前罕见 placement retry 明确 Unported，真实游戏中途 capture/restore 尚未认证。
