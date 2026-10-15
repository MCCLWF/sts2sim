# Env / RF API 第一批合同与原版操作证据

分支 `feat/env-rf-complete`，基底 `5266daf`。这是完整 E0–E5 目标的一次开发检查点，
不是完成报告或合并结论。另一对话负责新的大规模 seed、规则首因和独立冻结产物。

## 实际交付

- ABI 1 discovery：35 个导出、枚举/状态码、内容和 schema 指纹、所有权、明确的能力缺口。
- 纯 typed operation：Action、ObtainRelic、TransformCards、RerollCardReward、
  MerchantPurchase、胜利安全边界上的实际奖励结算。实例请求绑定取得它们时的 source hash。
- Choice/Boundary 使用 stationary base + 完整 operation + answers 前缀重放；
  EnemyPhaseDone、SafeOutcome 可以恢复/继续。snapshot v4 严格重放校验，仍限同一 native runtime。
- 独立 .NET 9 SDK 实际加载共享库，管理 library/SafeHandle/诊断/owned successor，
  序列化同一 Env 请求；Python 封装同一 ABI。坏 enum、错误引用和未支持能力明确失败。
- 修正自动触发药水的 discard 合法性；遗物默认加入楼层由 -1 改为原版非 nullable int 的 0。
  Event RNG 必须来自当前未结束事件。SDK 禁止将旧引用自动绑定到新 hash。
- 自定义奖励最后一项成功领取后直接结束 await，保留普通战斗房间的 Proceed；
  C++ 修复单独提交 `5201103`，已交给 seed 对话配对验证。

## 命令与原始结果

```bash
python3 tools/gen/gen_state_fields.py
cmake --build build -j 8
/home/wst/.dotnet/dotnet build tools/Sts2Oracle -c Release
ctest --test-dir build --output-on-failure
python3 tests/api/rf_operations.py --output-dir <新的外部证据目录>
```

- 构建成功。.NET Oracle 有一个既有 OracleRun.cs nullable 警告，无错误。
- CTest **49/49**，包括实际共享库 discovery、typed operation、SDK、Python 和既有回归。
- 原版探针 `RelicCmd.Obtain`：RFOP0001..RFOP0004 × Calling Bell/Cauldron/Astrolabe/Claws，
  **16/16 一致，72 个 native Choice 前缀**。从真实 StartRun + EnterAct(0) 的未选择 Neow 开始；
  比较源/每次选择/最终状态的卡牌、遗物、药水、动态变量 decimal bits、全部 run/player/event RNG。
- 每个 Choice 前缀都验证 clone、snapshot restore、合法 Answer、重放后同结果和 stale-request 拒绝；
  trace 开关不改变 successor；源快照不变；未知操作、错协议、过期请求无可用后继。
- 原版 RewardsSet 保留领取过的实例，UI 删除按钮；探针导出完整 backend index 与 selected，
  不把剩余列表重编号。原版没有最终额外 Proceed，因此 native 多一个选择即失败。
- 批次开始/结束源码和 runtime 指纹相同，qualified=true；原始证据保存在工作区外
  `artifacts/legacy/env-rf-api-2026-09-30/rf-operations-fixed-reward-indices`，不提交游戏文件或大日志。

## 真值资格与剩余验收

探针运行正版 v0.111.0 DLL。当前 headless 仍开启 TestMode，并由已有 scoped production-branch
adapter 恢复 CallingBell/Cauldron 等随机生成路径；这不是未打补丁的游戏进程证明。
O-001/D-001 资格限制仍保留。合同测试证明接口不破坏自身状态，不能证明全部游戏路径已正确。

| 项目 | 本批状态 | 完整验收缺口 |
|---|---|---|
| ABI、SDK、失败所有权 | 已实现，实际 native 合同通过 | 更大 RF 消费者、所有房间的独立真值语料 |
| typed 遗物取得 | 上述 4 遗物/16 case 原版通过 | 其他遗物/上下文/嵌套选择与 live source 映射 |
| typed 变牌、重抽、购买 | 已实现，合同有限通过 | 有序批量/逐张、RNG 来源、Courier 补货的直接原版矩阵 |
| 停止与续接 | EnemyPhaseDone/SafeOutcome 的合同通过 | 更多原版阶段、跨多个停点及独立 Oracle 停点证据 |
| 快照 | v4 同 runtime 与 prefix 恢复通过 | portable identity、完整真实游戏 capture/restore |
| 查询/trace | getter、before/after 与部分事实可用 | 完整 Hook/来源实例/RNG 逐抽取/对象寿命因果账本；trace_complete=false |
| 真实档案 | 明确拒绝不支持档案 | 解锁/纪元/首局/已见遭遇的原版规则 |
| RF 与规模/性能 | 需求矩阵已从原 RF 源码整理 | 全部单人 RF 功能、10k 完整零缺口对局、可复现 ABI/trace 成本 |

完整目标保持进行中。接下来整合 seed 对话的共享规则与策略协议，执行完整门禁，
继续补以上缺口；不能据此宣称“API 完善”或将完整目标标记完成。

## 第二批：共享修复整合与扩展矩阵

已整合 `6e7fead`、`3572e10`、`6ccb274` 的源规则修复及
`dbb2f50 → 3ef6cdd → 15677e8 → 6c3c6a2 → 3ff147b` 的策略/Oracle 合同链。
集成 tip `38d51d4` 的 `DOTNET=~/.dotnet/dotnet scripts/check.sh` **exit 0**，
50/50 CTest、L2、L3、严格 L4、跨幕胜利和逐点恢复全部通过。
这些是已定向覆盖的门禁，不能覆盖另一对话的新 8000 身份尚在暴露的首因。

原版操作探针 v2 增加有序变牌。五角色 × A0/A10 × RFOP0001..4 ×
player_transformations/run_niche/event × batch/sequential × 三种输入顺序
(`0`、`2,0,1`、`9,1,0`)：**720/720** 完整 source/final 与全部 RNG 一致。
先绑定全部原卡实例再逐张执行，不能把后续 deck index 重新解释为变化后的牌组。
进一步扩大四种遗物到五角色/A0/A10：**160/160 一致，700 Choice prefixes**
逐点 clone/restore/Answer/旧引用拒绝通过；不能按角色猜测相同选择数量。
原始批次在外部 `transform-matrix-v2`、`relic-matrix-v2`，各 10 个 job 都 qualified=true。

Preview 新增 `operation_status=complete/needs_choice/paused`，与 trace 完整性分别报告；
普通 Preview 的坏 action/trace limit 统一为 InvalidRequest(5)，源不变且输出 null。
最终合同 50/50 CTest；该增量的原版复查 16 遗物 case 与 72 变牌 case 全部通过。
完整 RF 功能要求已保存在
[RF 要求清单](../audits/random-foreseer-api-requirements.md)（21 项，固定 RF commit
`c4f14db684d831f9786d1c6af15b172b89d04939`）；其 baseline API 段是历史事实，
不能替代本报告的当前状态。

奖励 UI 复核另发现真实资格缺口：CardReward.OnSelect 固定了 headless alternatives，
但原版 Reroll→Populate→NCardRewardSelectionScreen.RefreshOptions 会移除 REROLL。
目前 headless selector 及 Env cached choice 可允许再次 REROLL；已与 seed 对话协调，
将补真实 UI 合法性及 Orrery/Driftwood 专用原版矩阵。现有成功样本不掩盖该缺口。

## 第三批：奖励真实可操作性

以上 REROLL 缺口已在本 API 分支修复。ChoiceOption 增加 `enabled`，Hash/snapshot/查询
同步保存；普通 Answer 和 engine 前缀都拒绝 disabled 选项。后台仍保留原索引，
已领取奖励、禁止跳过的 Proceed 和 UI 已移除的 REROLL 不再是合法动作。
纯 Predict 的非法动作统一返回 InvalidRequest(5)。新 snapshot schema 指纹会拒绝旧字段合同。

源码依据：CardReward.cs 的 Reroll(321–331) 禁用 CanReroll 并 Populate；
Populate(146–168) 重新 Generate alternatives 并传给 screen.RefreshOptions；
NCardRewardSelectionScreen.cs:127–174 替换列表、删除旧按钮并重建按钮。
headless CardReward.OnSelect 的旧回调列表不能作为实时 UI 合法选项。

独立 `tools/RewardUiChecks` 运行正版 DLL、**TestMode=false**，4 seed × 五角色，
**20/20 primitive** 确认重抽前为 Skip/REROLL，重抽后只剩 Skip；保留卡和 Rewards RNG 原始输出。
这是传给 RefreshOptions 的真实方法输出证明，不是 Godot 界面捕获。
headless bootstrap stderr 有一个既有 empty-path file-open 日志；进程 exit 0、20 个方法断言通过。

公共 API 的 **Orrery + Driftwood** 矩阵：4 seed × 五角色 × A0/A10，**40/40 一致，600 Choice prefixes**。
先分别比较 DRIFTWOOD setup 的 before/after，再取得 ORRERY；每份奖励一次 REROLL 后选择卡。
比较原卡、新卡、候选/可用选项、每个 prefix 的完整 run/player/event RNG 和最终牌组；
每次 prefix clone/restore/Answer 一致，第二次 REROLL 无后继且源不变。
全部 10 个 job source/runtime unchanged=true。原始证据在外部 `reroll-ui-matrix`。

首次新增字段时，snapshot reader 的字段数写到了 Action 而非 ChoiceOption，恢复测试和第一组
Orrery 原版对拍失败（原件 `orrery-driftwood-ui` 保留）；已按正确字段合同修复并重新验收。
修复后的 **50/50 CTest 与完整 scripts/check.sh exit 0**，包括跨幕/胜利和逐点恢复。
这个增量仍不证明 RF 全矩阵、live capture、真实档案和完整 trace 已完成。
先前 headless 允许重复 REROLL 的历史轨迹需要保留其资格标签；不能冒充真实 UI 操作序列。

## 第四批：商店 typed operation

`MerchantPurchase` 现在返回 `operation_result`（AllEntries 槽位、原版 success、ignore_cost），
商店查询也暴露稳定的 `entry_index`；不会把当前 stocked 子集的 UI `action_index` 当作后端槽位。
购买前仍校验库存和金币；原版入口返回 false 的有效尝试与非法请求分开报告，未完成的选择前缀不伪造结果。

正版 DLL 矩阵：`THE_COURIER` 注入、普通商店、连续同槽补货、删牌选择。TestMode=true **10/10 case、4 prefixes**；同一 shipped-DLL fixture 在 TestMode=false **4/4 case、2 prefixes**。每个 case 比较全部商店条目、价格、库存、金币、卡牌/遗物/药水 payload、run/player RNG，公共共享库 fixture 通过 C ABI snapshot import，源状态不变、trace 开关 successor 等价、删牌每个 prefix 可恢复。首轮 A10 发现 3 个遗物 CanonicalVars 未被内容导出；按正版 `PaelsClaw.cs`、`PaelsGrowth.cs`、`RoyalStamp.cs` 补回 `data/content.json` 并重新生成内容表后，IRONCLAD A10 **6/6 case** 严格通过。原始失败保留在 `artifacts/legacy/env-rf-api-2026-09-30/shop-native-matrix-r1`，不算通过证据。

TestMode=false 首次入口的 Godot 地图、删牌卡节点、replay 文件写入按源码确认属于表现/记录层；限定适配只跳过这些调用，房间、商店、Hook、购买、CardSelect 与 RNG 仍走正版。它是 shipped-DLL 注入 fixture 资格，不是未修改交互游戏或 live capture 证据。

## 第五批：可复现性能探针

将 `tools/bench/env_api_bench.cpp` 注册为独立 `env_api_bench` CMake 目标，并在同一台机器、同一构建、固定 `DUAL001` Cultists 根状态执行：预热 32 次，9 个样本，每样本 1000 次。结果原始 CSV 保存在工作区外 `.local/env-rf-api-bench-2026-09-30.csv`；每项包含 digest，根状态校验在探针前后均稳定。

| 操作 | 中位数 ns/op | 均值 ns/op |
|---|---:|---:|
| Clone（含析构） | 14,112.38 | 20,236.53 |
| Hash | 12,168.23 | 12,145.29 |
| Predict EndTurn（含析构） | 405,277.84 | 405,496.41 |
| Preview trace on（含析构） | 427,723.70 | 428,569.21 |
| Preview trace off（含析构） | 427,284.06 | 427,224.45 |
| ExportSnapshotJson（含字符串析构） | 91,828.36 | 91,910.42 |

探针同时记录 `sizeof(GameState)=804552`、trace-on 一帧/10 事件/4 个 power 事件，以及 trace on/off 后继 hash 相同。该数据只证明当前 C++ native API 的可重复成本；没有同一状态、同一语义、同一硬件下的 C# 基线，因此不能据此声称 C++ 已经“上线更高”或优于 RF 的运行时。

## 第六批：SDK 结果模型与合入门禁

独立 .NET SDK 的 `Sts2EnvPreview` 增加 `OperationStatus`、`TraceComplete` 和结构化 `OperationResult`；当前已知的 `merchant_purchase` 结果映射为 `Sts2MerchantPurchaseResult`，未知未来结果仍保留在 `ResultJson`。真实共享库 .NET 合同测试验证普通 action 的空结果、完成状态和 `trace_complete=false`，无手写 JSON 依赖。

在该提交上重新执行完整 `scripts/check.sh`，退出码为 0：CTest **52/52**，L2 **200/200**，L3 基准 **40/40**（另含非英雄遗物与 Untouchable 定向探针），L4 首幕、跨幕、胜利和 Pael's Wing 路线全部报告 `0 unported, 0 other failures`；日志原件在工作区外 `.local/env-rf-api-final-gate-2026-09-30-v2.log`。该门禁证明当前已实现范围未回归，不改变 RF 全矩阵、live capture、portable snapshot、完整因果 trace 和 10k 全条件覆盖仍未完成的结论。

同一提交的独立 RF consumer 复核也通过：`transform_cards` 2 seeds × 2 RNG source × 2 mode × 2 order 为 **16/16**，`obtain_relic` 2 seeds × 4 relic 为 **8/8**，商店 Ironclad A10、TestMode=false、Courier/连续补货/删牌为 **5/5**；三批均报告 `runtime/source unchanged=true`。这些是当前 typed operation 子集的回归证据，不是完整 RF 21 项功能矩阵。

## 第七批：Crystal Sphere 公共 API

新增 privileged 棋盘/物品/揭示顺序查询，并以关闭 TestMode 的原版 DLL 执行整条
EventOption、小游戏点击和奖励选择。五角色/A0/A10 × 4 seed × 双事件选项 × 三输入顺序
**240/240、2120 个 Choice 前缀一致**，每个前缀 clone/restore/Answer/源不变与 stale
请求检查通过。修复 default typed Action 在 Choice 丢失请求的问题，typed/ordinary
Answer/Resume 均保留原 root 的请求和 stop policy。详见
[本批报告](2026-09-30-env-crystal-sphere-api.md)。

这是事件根注入资格，罕见 placement retry、live capture 与完整 RF 矩阵仍待完成。
本批完整门禁 **exit 0**：52/52 CTest、L2 200/200、L3 81 场加 2 probes、
L4 103/103 与 18956/18956 决策一致；没有 Unported 或其它差分失败。

## 第八批：共享历史规则与原版非 TestMode 输入

已集成事件排序/X-cost/克隆/Poison/history 修复，在 API 分支重新生成包含
ChoiceOption.enabled 的 schema。进一步将原生奖励 UI 输入适配统一用于 Env operation，
TestMode=false 的四遗物 × 四 seed × 五角色/A0/A10 矩阵
**160/160、700 prefixes 一致**。水晶球复查6/6及57 prefixes、变牌16/16、
商店5/5及2 prefixes、Driftwood/Orrery一次重抽2/2及30 prefixes均通过。

集成最终完整门禁 **exit0**，52CTest、L2 200、L3 81+2、L4 103/103及18956/18956
决策一致。首轮 Calling Bell 尝试创建 Godot 场景而退出的原件保留，未算通过；
详见[集成报告](2026-09-30-env-rf-history-integration.md)。

这些仍是 headless fixture 资格。seed 聊53旧失败仍有3容量错误，完整2000深层复验进行中；
真实档案/live capture、portable snapshot、完整因果 trace、RF全21项和规模验收尚未完成。
本批记录时 main 未合并，完整目标保持进行中。

## 第九批：当前 API 完成状态复核

2026-09-30，复核代码基线 `da39de0`。main 已由 seed 对话 fast-forward 到该提交；
这是已通过门禁的阶段代码合入，不是完整 E0–E5 / RF 验收完成。

本轮直接加载 `build/libsts2env_c.so` 的 discovery，确认 ABI 1、35 导出、snapshot v4，
实际能力仍声明 `cross_process_portable=false`、`trace_complete=false`、
`rf_full_matrix_verified=false`。重跑以下八项实际 native/SDK 合同，**8/8、exit 0**：

```bash
ctest --test-dir build --output-on-failure \
  -R '^env_(api_descriptor|native_operations|dotnet_sdk|m3|snapshot|preview|run_queries|python)$'
```

| 范围 | 可用并有有限证据 | 完整验收尚缺 |
|---|---|---|
| 通用 Env | Reset/Step/Clone/Hash、纯预测、Choice/Boundary 续接、查询、同 runtime 快照、Python/.NET SDK、失败状态与后继所有权 | 真实 profile、任意实机静止点 capture/restore、跨进程身份/快照、完整来源/Hook/RNG/对象寿命 trace、更多停止策略及完整内容条件 |
| RF | typed 遗物/变牌/奖励重抽/商店操作，水晶球事件/点击，普通战斗动作及特权查询 | 21 项全部来源、上下文、嵌套效果和停止/结果的独立公共消费者矩阵；实机输入映射和完整因果结果 |

seed 对话的 f63 全2000深层复验被 ENOSPC 中断：1231 条完整结果记录
（1213 identical、2 capacity、16 tool_error），另有一条截断末行。
这些只是中断批次统计，tool_error 不算规则差异，也不算通过；不能写成2000全量验收。
容量候选修复尚未交付，本报告不把它计入 `da39de0`。
上一完整门禁的52 CTest、L2 200、L3 81+2、L4 103/103及18956决策证据保留，
但完整10k轨迹验收仍没有达成。通用 Env 与 RF API 均继续标为整体未完成。

## 第十批：显式跨进程 portable 快照

在 `73d053e` 基线开发同native构建 portable v1，保留原local v4 runtime约束。
FSM实际定义/创建时常量与状态顺序、typed callbacks、stun、事件回调及文本字典显式绑定；
构建/math runtime/浮点配置不匹配拒绝，恢复后native source hash重绑定。
ABI1新增一个导出（共36），Python/.NET SDK可传输独立portable DTO。

157组定义库存/状态在两个独立新进程各157 source/214后继一致；111怪物部分只算
定义传输库存，其余战斗/选择/边界执行另计。12类非法输入无successor且diagnostic Env不变。
本轮TestMode=false RF **21/21用例、125前缀**原版一致；两个新进程各**125恢复后继**通过，
保留父捕获及原共享库指纹。完整门禁53/53CTest、L2 200、L3 81+2、L4 103/18956，exit0。
见[完整增量报告](2026-09-30-env-portable-snapshot.md)。

这是E1的一项可复核增量。真实profile、任意实机capture、完整因果/RNG/对象寿命trace、
RF21项全部来源/条件、10k零缺口及公平性能仍未完成。此处21个测试case不是RF21项功能全矩阵。
seed对话的history摘要提交已到main `575e099`，尚未计入本轮portable冻结构建证据；后续组合需重新验收。

## 第十一批：portable/history组合门禁

API `ec0eb1b`与main `575e099`已组合并重新生成schema，完整门禁exit0：53CTest、L2 200、
L3 81+2、L4 103/18956。新portable矩阵两进程各157/214/12通过；两长局4907/4907；
TestMode=false RF新21用例/125前缀通过，两个新进程各125恢复后继一致。
上一构建的实际二进制已封存，旧portable build实际拒绝，不宣称跨构建迁移。
见[组合报告](2026-09-30-env-portable-history-integration.md)。

通用Env和RF仍整体未完成；同构建portable这一E1增量不替代真实profile/live capture、
完整trace、RF全部来源/条件、全规模/性能。seed的1993/2000资格属于其冻结0c版本，未冒充当前再次全量运行。
