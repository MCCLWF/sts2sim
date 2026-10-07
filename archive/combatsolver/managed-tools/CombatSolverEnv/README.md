# CombatSolver Env 派生 Mod

> **冻结档案（2026-09-27）**：本项目及依赖其旧 SDK 的工具被 `tools/Directory.Build.targets` 禁止构建，下面的命令和专用 ABI 描述是历史记录，不适用于当前 Env。旧 C++ 预测与 Beam 已归档到 `archive/combatsolver`。当前开发以通用 Env 和 Random Foreseer API 为主线；除用户重新明确要求外不恢复。见 [通用 Env 完成路线](../../docs/plans/2026-09-27-generic-env-completion.md)。


> **2026-09-27 状态：移植暂停。** 按用户新方向先完成[通用 Env 与 RF 公共 API](../../docs/plans/2026-09-27-generic-env-completion.md)。完成后不会自动恢复，重新启动需要用户后续明确要求。本文的接口、有限测试和最终 Mod 目标为历史记录，后续条目不表示正在继续移植。

此项目编译固定子模块的 `src/**/*.cs`，保留 CombatSolver 的原始搜索、评价、候选、剪枝、预算和部署代码。初始化源码仅替换 Mod ID；`prepare_sources.py` 在生成的根、快照、回放和搜索结束源码中加入验证入口，不修改子模块。每个入口检查唯一锚点，版本漂移会使构建失败。职责与限制见 [ARCHITECTURE.md](ARCHITECTURE.md)。

当前仍是开发版：内置 `Sts2EnvCompare` 的 `env/native/both` 和纯预测 session 接口，设置页提供持久化的 Native / Env（不支持时回退）/ Both 选择及对比开关。Native 会跳过 Env 入口；派生版 Env 模式目前被 `live_root_and_same_policy_adapter_incomplete` 门禁明确拒绝并由 Native 继续。Both 可运行开发核对，部分档案或未知状态会明确停用 Env。现有参考 Beam 不是原算法 Env 后端，不能将设置项或同程序集解释为完整替换已完成。

独立战斗预测会话现在有 `CaptureGameQueries()`：从私有分支读取五牌堆卡牌费用/关键词/可打出条件/目标、资源及怪物原始意图，返回 owned JSON 和源哈希。TestMode=false 的四组 getter 差分已通过；协议中的政策与威胁完整度仍为 false。原搜索 Whirlwind X 费用和不可打出卡的返回费用与游戏 getter 明确不同；这些查询尚不能替代原搜索的政策查询或评分，详见[查询报告](../../docs/reports/2026-09-27-combatsolver-env-game-getter-queries.md)。

getter 协议现为 v4，`card_state` 返回未按搜索筛选的游戏字段、全部变量类型/BaseValue 和 Rampage/Thrash 的精确增长输入。`EnvSearchCardFingerprintProjection` 在 Mod 中筛除 Derived、拒绝缺少特殊实例字段/附件/未分类字符串，复用原 Beam 编码。三组 live 根各 22 张牌与 16 个后继场景逐卡一致；原键只特殊编码 Rampage，不额外编码 Thrash。完整原状态键仍不可用，见[通用卡牌与 Mod 政策报告](../../docs/reports/2026-09-27-combatsolver-env-generic-card-policy-queries.md)。

`EnvSearchPileFingerprintProjection` 在 Mod 中用这批通用卡牌事实生成原搜索的无序牌堆保留键、循环牌堆形状部分及四个牌堆的状态键片段。16 个原 Beam owned 后继逐位对照通过。牌堆投影本身只声明牌堆部分；与其他输入的有限组合见下文，详见[牌堆键报告](../../docs/reports/2026-09-27-combatsolver-env-mod-pile-search-keys.md)。

`EnvSearchPotionInventoryProjection` 用 owned snapshot 中的原根固定槽数、当前槽引用和通用药水模型 ID，在 Mod 中复刻原药水库存键。16 个原 Beam 后继（含药水消耗）逐位对拍通过，未知模型/槽位变动明确拒绝，见[药水键报告](../../docs/reports/2026-09-27-combatsolver-env-mod-potion-inventory-key.md)。

`EnvSearchCycleEnemyProjection` 保存原根敌人顺序，只在名单稳定且活跃敌人生命可直接证明为正的分支计算原位图。Mod 再按原顺序组合牌堆形状、位图、药水键和显式 C# 搜索边界，16 个 Cultists 原 Beam 后继的完整 `CycleShapeKey` 逐位一致。新敌人、逃跑、活跃零 HP 复活窗口、Waterfall Giant Steam 特例与槽身份变化会拒绝；这是有限子集，不代表任意战斗的循环键、完整状态键或正式 Env 搜索已完成，见[循环敌人位图报告](../../docs/reports/2026-09-27-combatsolver-env-mod-cycle-enemy-mask.md)。

`CaptureCalculatedVars()` 独立 v2 owned 查询只从 C++ 读取正版 `Calculate` 值及计算公式的 Base/Extra/原始乘数；C# Mod 根据这些事实计算固定原预测器的终局保留状态值。`GetValue(cardRef, variable, targetRef, EnvCalculatedValueSemantics)` 保留游戏/原预测两种读法，targetRef=0 表示 null。Live 校验真实 owner/静态公式绑定；未知公式拒绝。三组根各 13 变量/52 值逐 decimal 位一致，原 Beam 16 场通过，其中终局 BodySlam 验证两种语义有别。该接口尚不是完整原评分/政策，见[本次报告](../../docs/reports/2026-09-27-combatsolver-env-generic-calculation-operands.md)。

`CaptureOrbs()` 返回通用 v2 owned 队列容量/顺序/引用与五种球的 decimal 被动/激发值，语义为 game；`EnvSearchOrbFingerprintProjection` 在 Mod 中使用固定原 AppendOrbs 编码。三组真实根各 6 球/容量 8，正版 getter、原 OrbMirrors、C++ 与原部分指纹相同，包括正负 Focus 和 Dark/Glass 小数。Mod 的 `OriginalStateKeyComplete=false`，不能把此部分指纹当成完整状态键。见[球键报告](../../docs/reports/2026-09-27-combatsolver-env-orb-key-inputs.md)。

`CapturePowerQueries()` 返回原始游戏能力字段，包含全部动态变量的类型/数值及未筛选的回合字段。C# `EnvSearchPowerFingerprintProjection` 负责原字段筛选和编码；`EnvSearchPowerLedger` 从通用能力事件派生根退休集合，保持分支隔离。三组真实根各 11 个能力和 16 个原 Beam 场景的原个体/聚合编码已通过，包含根 Strength 归零/重新获得；完整 key/其他账本仍未完成。协议为 power query v1 / prediction envelope v7，native library 与 DLL 需成套构建，见[通用能力边界报告](../../docs/reports/2026-09-27-combatsolver-env-generic-power-boundary.md)。

派生 companion 通过主线程 provider 冻结实际组件并组装 owned 根，worker 导入该文档进行 reference Beam 诊断；原生路线核对使用同一文档的独立 session。捕获失败显示 `env_imported=false`，不会降级到种子搜索。这个诊断根尚未与原 coordinator 请求绑定，reference Beam 也不是原政策，不能用于正式同根性能结论。

`CapturePolicyProbe()` 由 Mod 的 `EnvCardPlayPolicy` 从通用 getter 文档派生原搜索 `CanPlayCard` 核心判定、返回能量及星费；对应 C++ 实现和 C ABI 已删除。在三组完整 live 根与固定 upstream 逐卡对齐。它标记 `policy_projection_complete=false`，仍缺风险/事件/威胁、其他只读政策和评分，不可用来开启 Env 搜索，见[迁移报告](../../docs/reports/2026-09-27-combatsolver-env-generic-card-policy-queries.md)。

`both` 模式默认核对原生搜索分支的结构后继及九条战斗 RNG 的 counter/s0/s1/s2/s3，包括普通出牌与 EndTurn；每个快照独立拥有 Env session，随 `ReleaseSimulator` 释放。另已捕获并核对根级怪物个人 RNG/FSM。新增检查发现自然实机根的个人 RNG 与种子模板不同，当前会明确停用 Env 分支诊断，并在游戏内显示 Native 继续执行。此核对仍不覆盖全部内部状态、每条怪物后继、历史和非战斗随机流，也不是 Env 独立执行后端。`STS2ENV_VERIFY_BRANCHES=0` 可关闭这项开发诊断；开启时 native 搜索时间包含验证开销，不能用其原始耗时宣称提速。

公开预测接口位于 `CombatSolverEnv.Api`：`EnvPredictionApi.OpenSeededIroncladCombat`、`EnvPredictionSession.Clone/Predict/TryPredict/InspectJson/StateHash`。`TryPredict` 区分 `Ready/Unported/Error`，失败没有后继。入口名称明确说明这是种子战斗，不是 live 根导入。各分支须独立 Dispose；共享源句柄的调用由锁保护。示例：

```csharp
using CombatSolverEnv.Api;
using var root = EnvPredictionApi.OpenSeededIroncladCombat(new("DUAL001", "CULTISTS_NORMAL"));
using var next = root.Predict(new(EnvCombatActionKind.EndTurn));
string snapshot = next.InspectJson();
EnvStateSnapshot complete = next.CaptureSnapshot();
EnvRngState shuffle = complete.GetRunRng(EnvRunRngStream.Shuffle);
using var restored = EnvPredictionApi.RestoreCombatSnapshot(complete);
var definitions = root.InspectMonsterMachines(); // owned JSON; indices scoped to runtime_id
```

`CaptureSnapshot` 导出完整 **Env 逻辑状态**（协议版本 2），包含内部字段、池槽引用和分配游标、HashSet 空闲链、精确 decimal/浮点位及选择前缀和重放根。JSON 独立拥有存储，源 session 释放后仍可读取。`RestoreCombatSnapshot` 在同一 native runtime 内恢复独立的战斗/选择分支，严格核对字段、schema、模型 ID、运行时身份、存储/引用、状态机归属和已有 hash；选择必须能从重放根及答案前缀重现。错误与 `Unported` 分开，失败没有后继。

原搜索的胜负评分需要在战斗安全点截停，保留战后回复、清理和奖励之前的状态。独立预测会话具有独立句柄类型、hash 域和 `sts2env.combat_prediction` v7 外层快照；普通 run 快照接口不接受这类终局。v7 保存通用执行事实、卡牌与出牌快照、伤害来源、攻击命中结果、能力事件以及选择动作起点的事件基线。C++ 不再保存搜索 Risk；每次进入 OnPlay 记录 `CardEffectStarted`，保留实际目标与重播身份。Mod 的 `EnvSearchHistoryProjection` 负责原历史顺序、默认 inference 下 Rampage/VoidForm 风险、Self 目标适配、六计数及延迟条目索引。非默认 inference 明确拒绝；其他方法、Hook 和预算限制风险仍未覆盖，完整度为 false，尚不能用作正式搜索状态键。16 个原 Beam 场景已通过，包含重播和风险后的抽牌引用，见[Mod 历史投影报告](../../docs/reports/2026-09-27-combatsolver-env-mod-history-projection.md)。旧外层版本明确拒绝。从普通游戏快照打开已暂停选择会拒绝，因为缺少历史动作起点。示例：

```csharp
using var searchRoot = EnvPredictionApi.OpenCombatPrediction(complete);
using var branch = searchRoot.Predict(new(EnvCombatActionKind.EndTurn));
EnvCombatTerminalStamp? terminal = branch.Terminal;
EnvCombatPredictionSnapshot frozenBranch = branch.CaptureSnapshot();
using var restoredBranch = EnvPredictionApi.RestoreCombatPrediction(frozenBranch);
```

EnvStateSnapshot.PendingChoice、EnvCombatPredictionSnapshot.PendingChoice 和 AnswerPrefix 现在提供 owned 的选择请求/答案前缀（选项数、可选范围、卡牌引用、已选项、可跳过和来源）。它们只描述 Env checkpoint，不把原 PlanCardChoice 或策略选择自动转换成答案；原 Beam 的多层 Choice 顺序、策略账本和部署接管仍由正式 Env 门禁拒绝。终局无合法动作，继续预测明确失败；Clone、选择前缀重放与恢复均保留截停信息。该接口提供独立引擎分支，尚未把原 Beam 的执行、全部评分查询和预测事件流接到 Env。快照是同 native runtime 内的受信诊断数据；普通输运 hash 检查完整性，不证明历史胜负来源。

这条导入通道尚未提供完整 **live 游戏字段映射**。新增的 seed-free blank template 只包含 POD 默认值、协议/注册表元数据和独立的 `monster_default`，不 Reset、新局、不调用玩法 Hook，也不抽 RNG；它本身不能导入为 combat root。live assembler 只在所有组件和普通战斗房间边界都通过后覆盖真实字段，奖励/事件/休息点/宝箱/商店等未映射字段必须保持空模板默认，否则拒绝。Run 与 Player 的实际解锁档案现已分别写进通用 POD 状态并经过快照/克隆；部分档案的动作仍由 Env 明确拒绝，直到相关内容规则移植完成。当前安装游戏继续在遗留事件同步器记录处拒绝完整根，companion 明确 `root_source=live_capture env_imported=false`。只有完整 live 组件导入通过后才会标为 `live_snapshot_import`。跨进程存档不支持，怪物状态机与回调注册号属于当前 native runtime。完整快照含隐藏信息，只供预测适配/核对使用，不属于智能体 observation。

开发验证：

```bash
~/.dotnet/dotnet run --project tools/EnvSessionChecks -c Release
~/.dotnet/dotnet run --project tools/EnvLiveCardChecks -c Release
python3 tools/CombatSolverEnv/run_smoke.py --verify-branches --out /tmp/env-branches
python3 tools/CombatSolverEnv/run_smoke.py --expect-branch-rejection --out /tmp/env-root-rejection
python3 tools/CombatSolverEnv/run_smoke.py --expect-monster-root-rejection --out /tmp/env-monster-rejection
python3 tools/CombatSolverEnv/run_smoke.py --expect-event-bookkeeping-rejection --memory-reservation-mib 2048 --out /tmp/env-event-rejection
python3 tools/EnvQueryChecks/check.py --out /tmp/env-queries
```

分支 fixture 显式清除上游默认注入的 Strike，并保留自然敌人生命；限定 100 个展开节点后结束。其接受门禁目前因新检测到的个人 RNG 不同而失败。两个拒绝 fixture 分别检查注入牌不匹配与自然根怪物 RNG 不匹配，核对 Env 被停用及 UI 提示，不能替代接受门禁。它们不验收终局、任意 live 根或完整 Env 后端。

构建：

```bash
cmake --build build --target sts2env_c -j 4
~/.dotnet/dotnet build tools/CombatSolverEnv/CombatSolverEnv.csproj -c Release
```

产物为 `.godot/mono/temp/bin/Release/CombatSolverEnv.dll`。安装时将 DLL、`CombatSolverEnv.json`、`build/libsts2env_c.so`、上游 `LICENSE` 和 `THIRD_PARTY_NOTICES.md` 放入游戏 `mods/CombatSolverEnv`。可以使用 `-p:CopyModOnBuild=true` 复制这些文件。

此派生版包含完整 CombatSolver，须停用原版 `CombatSolver` 和独立 `Sts2EnvCompare` 后再启用，避免重复生命周期订阅和补丁。隔离测试通过 `run_smoke.py` 创建仅含派生版的源码视图，再由上游启动器创建与清理正式 headless 实例；不改动正常游戏的 Mod 配置。

## 真实卡牌组件开发检查

`Runtime/EnvLiveCards.cs` 在主线程冻结真实卡牌数据；它是完整 live 根映射的一部分。完整组件通过后生产诊断使用实际组装根，部分档案等失败会拒绝；不能将有限根接受视为原算法 Env 后端已完成。

```bash
~/.dotnet/dotnet run --project tools/EnvLiveCardChecks/EnvLiveCardChecks.csproj -c Release
```

检查使用正版游戏对象，覆盖同名不同实例、费用到期、变量、克隆/牌组引用、已移除原件、冻结隔离及 C++ 组件恢复/预测；其余状态明确使用种子夹具。已审计 Rampage/Thrash 的 decimal 内部增长字段及 13 个计算型公式；Thrash 对这些公式调用 `Calculate(null)`，未知公式仍 Unported。其余特殊变量、内部字段、附魔/污染与 Dampen 仍明确拒绝。

同一工具还检查 Cultists 的 FSM/RNG、动态 follow-up/运行标志恢复，Toadpole 条件分支和 SludgeSpinner 随机分支。预期来自正版状态机推进；不比较战斗效果。其他怪物动态定义、内部字段和 stun 仍须适配。另从正版 `CreateCreature` 采集缺失/负坐标的 RNG 向量，C++ 回归验证保留负坐标。详见[报告](../../docs/reports/2026-09-26-combatsolver-env-live-monsters.md)。

现在同一工具也验证实际 run/player 15 流、odds/地图/幕/遗物袋、人物/能力、活跃库存和回合边界/历史聚合的组件恢复及纯预测。`EnvStateSnapshot.GetPlayerRng` 可读三条人物流。未解锁档案的运行次数、57 位纪元集合及按原模型 ID 编码的已见遭遇位图进入 Run/Player 的 POD 状态；动作和原 Beam 验证分支仍会明确拒绝部分档案。已完成且清理的 Neow 事件会进入通用 `EventState`，同步器页码、投票槽和待处理任务也会一并校验；未完成事件目前明确拒绝。`--expect-event-bookkeeping-rejection` 检查当前实机下一处拒绝点、游戏 UI 和 Native 继续执行；它不代表 Env 接受或替换搜索已完成。详见[档案状态报告](../../docs/reports/2026-09-27-combatsolver-env-live-profile-state.md)。

## 完整根有限接受夹具

`EnvLiveCardChecks` 的 TestMode 关闭整根检查用全解锁正版 Ironclad/Cultists A0 对象及生产命令组装完整文档。Strike、Rampage、Thrash、Thrash+BodySlam、BodySlam、Barrage 六个场景的导入、实际出牌与连续两次 EndTurn 全部字段一致，包括小数增长/计算、真实充能球触发、实际敌方伤害、Weak 到期移除、能力增长和重洗 RNG。只读合法动作查询使用副本保证源不变；mapper 对缺字段、重复写入、组件不一致和新 schema 字段明确拒绝。

跨 capture 的 native PowerApplied/PowerRemoved 观察器仅用于固定 roster 夹具，保持已观察能力实例引用；移除后的字段读取当前仍持有的正版对象，不复用旧 capture 或 Env 值。这不是安装游戏任意 live 接受证明，未观察的移除对象、未应用分配和动态怪物闭包仍未完成。当前安装游戏先在遗留事件同步器记录处拒绝完整根；即使消除此处拒绝，部分档案的动作仍须等通用 Env 卡池/药水池/首局规则对拍后才能放开。原算法后端、游戏内连续回合/选择/药水/部署及等预算性能验收仍待完成。见[整根报告](../../docs/reports/2026-09-26-combatsolver-env-whole-live-root.md)和[档案状态报告](../../docs/reports/2026-09-27-combatsolver-env-live-profile-state.md)。
