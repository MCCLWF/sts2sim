# Env 状态输入规范

状态采集和游戏到本协议的转换由调用端实现。当前支持正版 v0.111.0、单人、全解锁老档案。
先读 sts2_env_api_descriptor() 的 ABI、模型/内容/schema 指纹和能力。非全解锁档案导入返回
Unsupported，不自动覆盖为全解锁。实际调用端、实机悬停、部分档案不在本轮交付范围。

## 输入格式

sts2_env_import_snapshot(diagnostic_handle, bytes, length, &owned_env) 接受 UTF-8 JSON。
local 为 sts2env.logical_state v4，仅同一已加载 native runtime；portable 为
sts2env.portable_snapshot v1，允许跨进程，但要求同一 native 构建、数学运行时及浮点环境。
旧 schema、构建、模型 ID 明确拒绝，不支持跨构建迁移。

local 完整根字段：format、version、schema_sha256、model_id_hash、registry_scope、
runtime_id、run_rng_streams、player_rng_streams、decision、state_hash、state、pending、
pending_operation、boundary、boundary_skipped、answers、choice、replay_base。
缺失/额外字段、重复 JSON key、错误类型拒绝。state/replay_base 完整成员及类型以当前构建
生成的 [state_fields.generated.h](../engine/sim/state_fields.generated.h) 和 schema 指纹为准；
声明在 engine/map.h、engine/run_state.h、engine/sim/state.h，校验在 env/snapshot_import.cpp
及 env/snapshot_validation.cpp。导出的完整快照是当前版本的模板。Observe、Oracle dump 和
查询投影不能直接作为完整状态输入。

地图输入是实际生成/恢复后的图：普通、黄金路线、特殊藏宝图及其SavedActMap恢复形式使用
同一节点/连边/网格表示。类型差异不应导致以seed重生成地图；保留第二Boss及任务标记。

portable 根字段：format、version、native_build_sha256、native_runtime_abi、wire_hash、
logical_state、registry。registry 保存实际 callback/FSM/event text 可解析定义，logical
使用 canonical 索引。wire_hash 保护 canonical payload；导入重绑定本地 registry 后必须
使用新 Env 的 state_hash，不能将 wire_hash 当 source_hash。不序列化任意 managed delegate
或 Task 栈，也不接受未登记 callback。

## 字段与编码

必须保留 run/角色/档案/地图/房间、事件/奖励/商店、player/combat、对象池、牌堆顺序、
可变模型字段、历史统计及全部游戏 RNG。不能重新生成缺失状态或补抽 RNG。原版私有 List
的等价统计表示见偏离登记册，调用端必须提供对应统计。

- enum 使用当前构建 descriptor/生成内容表的编号，不假定跨版本相同。
- 对象引用是所属池 slot，0 表示 null；需要存活的引用必须指向 live slot。相同模型 ID
  的不同实例不能合并。owner/target、clone/deck_version、附件/capture 等关系须一致。
- Pool 为 {capacity,next,count,slots:[{index,value}]}，保留 slot 身份和下一次分配顺序。
  StaticVec 是活动元素的有序数组；DotnetSet 保存 count/free_list/free_count 和逐 slot
  的 used/value 或 next_free，不能改成普通去重数组。
- 整数匹配 schema 范围，不经浮点中转；uint64 JSON 不能先存进丢精度的 JS Number。
  decimal 为 {decimal_bits:[lo,mid,hi,flags]}；float/double 为 {f32_bits:...}/{f64_bits:...}，
  保留原始位模式，不用格式化小数替代。
- RNG 保存 seed、四个 uint64 状态字及 counter；stream 名称/顺序严格匹配导出协议。
  全零 RNG、错 stream、错 run seed 关系拒绝。用 seed 重新 Reset 仅适用于新局。
- run/player 的 profile.full_veteran_compatible 均须 true：57项 epoch 全解锁、各幕
  BossDiscoveryOrder 的12个 Boss 已见、number_of_runs>0。普通/事件 encounter 不要求
  全部见过，encounters_seen 位图仍保存实际历史。校验事实一致，不只信 true 标签。ancient visits、
  角色胜场等实际历史数值另外保存；全解锁不表示这些计数固定。

## 静止点与继续

无 pending 输入必须位于支持的房间决策；战斗必须是玩家 Play 阶段且无正在执行的效果。
pending/pending_operation/boundary/choice/replay_base 为 null，answers 为空，
boundary_skipped 为0。非 pending 的 state_hash 可为 null，由 Env 导入后计算；提供时
必须正确。decision 必须与 state 一致。

Choice/Boundary 必须提供完整 stationary replay_base、唯一一个 pending action 或 typed
operation、已有 answers、当前 choice/boundary 及正确 state_hash。Env 从静止根重放验证
当前 prefix，不能从捕获的任意中间机器栈继续。Choice 用合法 Answer，Boundary 用 Resume。
无法提供前缀时只能在操作前或结束后的静止点输入。

## 操作与所有权

typed 请求为 {format:"sts2env.operation",version:1,kind,source_hash,stop,params}。
引用/商店 backend slot 绑定获取它们时的源 hash；不能在状态改变后自动重绑定旧引用。
变牌 RNG 来源必须对应原版调用来源，不能给操作额外塞 RNG 状态。六种操作和四种停点见
[env-preview-api.md](env-preview-api.md)。

成功导入/预测返回 caller-owned Env handle，最终 sts2_env_destroy。结果/错误字符串借用
所属 handle，下一次调用前复制。纯 Predict/Preview 成败均不修改源游戏状态；失败 successor
为 null。Step 是可变执行，不承诺失败回滚。operation_status 为 complete/needs_choice/paused，
后两者需继续，不能当作完整结果。错误区分 Unsupported、Unported、Capacity、InvalidRequest、
TraceLimit、一般错误；资源不足不能返回伪造成功。

trace_complete=false 表示诊断事件不完整，不表示完成的 successor 是半执行状态。
隐藏牌序、未来预选事实、RNG 为特权数据；正常智能体用 Observe，不直接使用快照。
快照解析不再使用旧的4 MiB、250k节点或固定游戏对象池上限。`StaticVec`/对象池按需增长，
对象引用和计数使用32位范围（Affliction UID为64位）；解析器仍保留64层结构嵌套保护。
宿主可在库加载前用 `STS2_ENV_MAX_SNAPSHOT_BYTES`、`STS2_ENV_MAX_JSON_NODES` 和
`STS2_ENV_MAX_RESTORED_STORAGE_BYTES` 设置资源预算，超出预算返回 Capacity/InvalidRequest，
不会截断或伪造成功。C ABI、Python 和 .NET 的 action mask/observation 使用长度发现，
不再假定固定512/2048字节缓冲区；这些预算和协议字段仍绑定当前 native build，不能跨构建迁移。
