<!--
  已批准的总体设计。最初在计划模式里写成，2026-09-24 批准；从 ~/.claude/plans 搬进仓库。
  后续调整直接改本文件，并在文末的"修订记录"里写一行。
-->

# 杀戮尖塔2 通用 C++ 环境（sts2sim）设计方案

## Context（背景）

目标：做一个独立、无头、可快速克隆状态的 C++ 模拟器，供两类使用者：树搜索（MCTS 等）和 Python 强化学习。
已确认的需求：
- **一开始就覆盖整局**：地图、事件、商店、奖励、休息点、先古之民、战斗。
- **与真实游戏逐位一致，包括 RNG**：同一个 seed 加同一串动作，要复现真实游戏。
- **只做单人**。

2026-09-27 用户新增方向：主线为通用 Env 和支持 Random Foreseer 类客户端的纯预测、局外预览、可恢复选择、原版查询及结构化执行事实 API。首批普通预览、模拟器房间恢复和原版语义回退已通过有限合同/回归；完整 Env/RF 功能尚未完成。CombatSolver 历史代码冻结，除非用户重新明确要求，不因 Env 完成而自动恢复。搜索策略、评分、风险、状态键和 UI 属于客户端。M3 参考 ISMCTS 保留为 Env 外部消费者示例；旧 Beam 和专用 prediction ABI 不属于公共 Env API。真实档案规则列为待补原版语义；全解锁仅是当前基准。

参考源码是 GDRE 反编译的游戏工程 `Slay the Spire 2/src/Core`，约 3500 个 .cs 文件，对应版本 v0.111.0（commit 41cef1ea）。你已经有 Harmony mod 的开发经验（MCC.RuntimeTuner、CustomYourSpire，.NET 9），本机也有 dotnet 9 SDK。

### 调研得到的关键事实

1. **引擎全程是 async/await，没有 STS1 那样的动作队列。**
   - 卡牌、能力、遗物、怪物招式都是 `async Task`，直接 `await` 各种 `*Cmd`。效果的执行顺序就是 C# 调用栈，本质上是深度优先。
   - 中途的玩家选择通过 `CardSelectCmd.*` 挂起，共 12 种选择入口。出牌目标在 `PlayCardAction(card, target)` 里预先给定。
   - 结论：**C# 引擎无法在效果执行到一半时克隆状态**。
2. **Hook 分发靠遍历，不靠注册。**
   - `AbstractModel` 上约 185 个虚方法，`Hook.cs` 里约 132 个分发函数。
   - 顺序完全由迭代顺序决定（`Combat/CombatState.cs:411 IterateHookListeners`）：
     - 先我方、后敌方。
     - 每个生物内部：能力按施加顺序 → 遗物 → 药水 → 充能球 → 手牌/抽牌堆/弃牌堆/消耗堆/打出区的卡（每张卡带着它的附魔和苦难）。
     - 最后是修饰器。
   - 另有 VeryEarly、Early、Late 三档，各自是一次完整遍历。
3. **RNG 实现干净。**
   - 算法是 Xoshiro256** 加 SplitMix64 初始化（`Random/MegaRandom.cs`、`Rng.cs`）。
   - 共 12 条局内流、3 条玩家流，每条流的种子是 `runSeed + XxHash64(snake_case 名)`。
   - 地图、遭遇生成、每个怪物、每个事件用的是临时从种子派生的 Rng。
4. **数值运算用 C# `decimal`。**
   - 存在非二进制小数，比如 `/100m`、`0.3m`、`0.33m`、`1.1m`。用 double 截断后会与原版不一致。
5. **内容规模**：卡 596、遗物 300、能力 268、药水 65、怪物 121、遭遇 91、事件 68（含 8 个先古之民）、附魔 23、苦难 7、充能球 5、角色 5、幕 4。Models 目录约 10.6 万行，引擎本体约 4 万行，Nodes（UI）9.6 万行可以跳过。
   - 效果代码有规律：流式的 `DamageCmd.Attack(x).FromCard().Targeting().Execute()`、`PowerCmd.Apply<T>`、`DynamicVars`。
   - 视觉代码（Vfx、Sfx、TriggerAnim、`NCombatRoom.Instance?`）可以机械地删掉。
6. **有现成的验证抓手。**
   - `.mcr` 战斗回放：初始 `SerializableRun` 加输入序列加 `NetFullCombatState` 校验和。
   - JSON 存档，包含全部 RNG 的 s0 到 s3。
   - `TestMode`、`NonInteractiveMode`，可以跳过等待和视觉。
   - `ICardSelector` 可以注入选择。
   - DevConsole 可以用 `fight`、`card`、`relic`、`power` 等命令搭场景。
   - AutoSlayer 有两个问题：从第 3 回合起作弊加 200 力量，而且被 `IsReleaseGame()` 禁用了。只能参考，不能直接用。

---

## 核心架构决策

### 决策 1：同步直写引擎 + 静止点快照 + 选择前缀重放（不用协程、不用手写状态机）

- C++ 引擎写成**普通同步函数**。C# 的 `await X(...)` 一对一翻译成 `X(...)`，调用顺序天然保持深度优先，和原版一致。这让移植约 1400 个效果体的工作量和出错率都最低。
- 引擎遇到选择点时调用 `ctx.choose(ChoiceRequest)`：
  - 如果"选择前缀"里有现成答案，就直接返回。
  - 如果没有，就抛出 `NeedChoice{request}`。
- 环境层的 step 流程：
  1. 在静止点，也就是等待出牌或结束回合的时候，保存一份状态快照。
  2. 执行动作。执行中抛出 `NeedChoice` 时，丢弃这份半成品，把"快照 + 待执行动作 + 已选前缀"作为挂起状态，向智能体暴露这个选择。
  3. 智能体给出选择后，从快照带着新前缀重新执行一遍。
  4. 因为 RNG 在状态里，重新执行是确定性的。中途选择大约只出现在 90 处，重放一张卡的成本是微秒级。
- 回合开始时 hook 里的选择（`HookPlayerChoiceContext`）也用同一套机制：从"结束回合"那个静止点带前缀重放。

### 决策 2：状态是可平凡复制（trivially copyable）的 POD 结构，克隆就是 memcpy

2026-10-01 用户要求动态容量；现行实现已调整为拥有型深复制：小列表内联，超出后按需扩容；
对象池使用地址稳定的分块存储并保持数值引用稳定。复制/选择重放拥有独立可变数据，不能再
memcpy GameState。下面的定长POD条目是历史设计依据，不是当前容器上限。

- 全部实例由状态拥有的对象池保存；各牌堆用保持顺序的 `StaticVec` 存下标，超过内联容量后扩容。
- 每个生物的能力使用同一可增长列表语义，模板参数只提供小状态的内联提示。
- 没有指针、没有堆分配，统一用 id 和下标引用。
- 目标：单局状态不超过约 8 KB，克隆耗时在 1µs 以内。
- 每个实例预留通用槽位：`Dec vars[k]` 和 `int32 extra[k]`，对应 C# 里的 `DynamicVars`、`ExtraFields`、`_internalData`。k 的大小由导出器统计后确定。

### 决策 3：内容用"数据导出 + 代码生成 + 逐文件移植"

- 数值、费用、稀有度、关键字、升级差值、卡池、怪物血量、遭遇列表，全部由 C# 导出器输出成 JSON，再生成 C++ 表。
- **导出器用反射检测每个 model 覆写了哪些 hook**，生成 `overrides[hook][model]` 位表。分发时只遍历真正覆写了该 hook 的对象，同时严格保持原版的迭代顺序。
- 效果体的移植分三步：
  1. 用脚本删掉 VFX、SFX、TriggerAnim 等纯视觉代码。
  2. C++ 侧提供一套与 C# **同名的 Cmd API**，比如 `DamageCmd::Attack(x).FromCard(c).Targeting(t).Execute(ctx)`。
  3. 按文件做 LLM 辅助的一对一翻译，每个文件都要过差分测试。

### 决策 4：用数值和容器语义做逐位对齐

- `Dec`：完整模拟 System.Decimal（96 位尾数加 0 到 28 位小数刻度），用 .NET 做随机对拍验证。
- float 运算（怪物权重、`NextFloat`）：C++ 里保持 `float`，禁用 `-ffast-math`。`.Sum<float>` 要用 double 累加再转回 float。
- `Math.Round` 是银行家舍入，C++ 用 `nearbyint`。Gaussian 用到的 log/sin 在同一平台的 glibc 下结果一致。
- `StableShuffle` 先按 `ModelId`（序数比较）排序，再按升级等级排序，然后洗牌。
- 需要模拟 HashSet、Dictionary 的遍历顺序：按插入顺序，删除后的槽位进空闲链表。例如 `Creature.SetUniqueMonsterHpValue` 就依赖这个。
- 存档相关的状态（`DiscoveredActs`、`BossDiscoveryOrder`、`NumberOfRuns`、时代解锁）会影响 up_front RNG 的消耗顺序。把它们做成 `ProfileConfig` 输入，并写进 trace 的头部。

### 决策 5：原版游戏只当"神谕"（oracle），不直接当环境

> 2026-10-03 范围说明：这是早期面向大量克隆/树搜索的架构决策，下面理由保留为历史。
> 新的 [Silent A10 整局 NoSL 计划](plans/2026-10-03-silent-a10-nosl.md)允许原版 DLL/Godot
> 用于教师和训练采样小试，按恢复能力、真实吞吐与成本决定后端分工；不以该旧决策禁止学习后端探索。
> 原版仍是规则真值，部署 actor 不做模拟搜索。C++ Env 架构与既有规则保真要求继续保留。

- 用 Godot 无头运行 C# 逻辑来做环境不可行：async、GC、无法克隆，搜索吞吐太低。
- 原版只用来生成 ground truth trace。

---

## 目标代码结构（新建 `/home/wst/Desktop/MCC/sts2sim/`，C++20 + CMake）

```
sts2sim/
  core/        rng.h (SplitMix64/Xoshiro256**/Rng 包装), xxhash64, dec.h (System.Decimal 模拟),
               static_vector, dotnet_hashset 顺序模拟, ids
  engine/      state.h (RunState/CombatState/Creature/Player/CardInstance POD)
               hooks.{h,cpp} (按 IterateHookListeners 顺序 + overrides 位表分发)
               cmd/ (Damage/Attack 管线: 附魔→Additive→Multiplicative→Cap→block→HpLost→Kill,
                     Block, Power, CardPile(draw/shuffle), Card, Orb, Stars, Osty/Pet, Select)
               combat.cpp (StartTurn/SetupPlayerTurn/EndTurn/ExecuteEnemyTurn, 胜负判定)
               monster_fsm (MoveState/RandomBranchState/ConditionalBranchState)
               run.cpp (新局 up_front 顺序, ActModel.GenerateRooms, 地图, 房间, 奖励/Odds, 商店, 休息点, 事件, 先古)
               choice.h (ChoiceRequest / 前缀重放 / NeedChoice)
  content/     generated/ (由导出 JSON 生成的表与注册)
               cards/ relics/ powers/ potions/ monsters/ encounters/ events/ enchantments/ afflictions/ orbs/
  env/         action.h (结构化动作 + 扁平索引 + mask), observe.h (张量编码 + JSON/文本渲染),
               env.{h,cpp} (reset/legal_actions/step/clone/hash/determinize)
  search/      参考 ISMCTS + 随机 rollout（Env 外部消费者示例，用作基准和正确性冒烟）
  python/      C ABI + ctypes 包装 + VectorEnv（pybind11/Gymnasium 作为后续扩展）
  tools/
    Sts2Exporter/   C# Harmony mod (.NET 9，引用游戏 sts2.dll，同 MCC.RuntimeTuner 的构建方式)：
                    遍历 AbstractModelSubtypes/ModelDb → content.json（含 hook 覆写表、升级差值）
    Sts2Tracer/     C# Harmony mod：每个 action/checkpoint 输出 JSONL（完整状态 + 意图 + RNG 计数 + 选择），
                    内置随机合法动作 bot（ActionQueue 入队 + CardSelectCmd.UseSelector），批量跑 seed
    rng_golden/     .NET console，直接编译反编译出的 Random/、StringHelper 哈希 → golden 向量
    port/           去视觉代码脚本、移植模板、按文件跟踪移植/测试进度
  tests/       RNG/哈希/Decimal golden、地图生成 golden、trace 回归语料、首个分歧点定位器
```

## 环境 API（面向搜索和 RL）

- `Env::reset(seed_str, character, ascension, ProfileConfig)`。另提供 `make_combat(...)` 子局接口：直接给定牌组、遗物、血量、遭遇，用于课程学习和战斗专项搜索。
- **动作是结构化的，同时提供扁平索引加 mask**：
  - 战斗：`PlayCard(hand_idx, target)`、`UsePotion(slot, target)`、`DiscardPotion`、`EndTurn`、`Choose(i)`。
  - 多选拆成逐张选择再加 `Confirm`，这样动作空间保持很小。
  - 局外：`MapMove(node)`、`EventOption(i)`、`TakeReward(i)`、`SkipReward`、`ShopBuy(i)`、`ShopRemove(card)`、`RestOption(i)`、`Proceed`。
- **面向搜索**：
  - `clone()` 是拥有型深复制；动态列表和对象池不与源分支共享。
  - `state_hash()` 遍历完整拥有型状态，排除 padding、未用尾部和已释放对象，可用于置换表。
  - 同实例去重：手里有 3 张完全相同的打击时，只生成 1 个动作。
- **防作弊的信息集采样** `determinize(state, rng)`：
  - 逐位一致意味着引擎是"全知"的：抽牌顺序、生成卡、怪物 AI、各幕预生成的遭遇和事件队列，都已经藏在 RNG 或状态里。
  - 为了让搜索公平，这个函数会重新洗匀未知的抽牌堆，重设隐藏 RNG 流，并从同一池子重新抽取未来的遭遇和事件队列。这是 ISMCTS 必需的。
- **面向 RL**：
  - 固定形状的观测张量：玩家、各敌人及其意图、能力、各牌堆的多重集编码、遗物位集、药水、地图图结构、当前决策类型。
  - 动作 mask、奖励钩子（胜负、层数、血量变化）、`VectorEnv` 批量 step。
  - 另外提供 JSON 和文本渲染，方便调试和给 LLM 看。

## 验证体系（逐位一致的核心）

| 层级 | 验证内容 | 真值来源 |
|---|---|---|
| L0 | Xoshiro、SplitMix、XxHash64、各 `Next*`、Shuffle、GaussianInt | `tools/rng_golden`：.NET 直接编译原版 `Random/*.cs` |
| L1 | `Dec` 的四则运算、比较、截断 | .NET 随机对拍，百万级用例 |
| L2 | 新局 up_front（遗物袋、先古、各幕房间预生成）、地图生成、Odds | Tracer 在开局导出存档 JSON 和地图，覆盖多个 seed |
| L3 | 战斗：每个 action 和回合阶段边界后的完整状态 | Tracer 的 JSONL；比 `NetFullCombatState` 更全，额外包含怪物意图和每怪 Rng |
| L4 | 整局：随机 bot 在原版里跑 N 个 seed，C++ 逐步重放 | 首个分歧点定位器报告 diff 字段、正在执行的卡/hook、RNG 计数差 |

L4 的语料同时就是移植的回归测试集。每移植一批内容，就跑出覆盖这批内容的 trace。

## 里程碑（每一步都能验证）

- **M0 基础设施**
  - core（rng、xxhash、dec、容器）加上 L0、L1 测试。
  - Sts2Exporter 输出 content.json，并生成 C++ 表。
  - Sts2Tracer 骨架：能在原版里用 `NonInteractiveMode` 加即时模式跑随机 bot，并导出 trace。需要实测 Godot `--headless` 能否带上 FMOD 和 Spine 扩展正常运行；不行就用窗口模式加 Instant。
- **M1 引擎骨架**
  - state、hooks、全部 Cmd、战斗循环、怪物状态机、选择前缀机制。
  - 整局流程骨架：地图、房间、奖励、商店、休息点、事件框架。
  - 用 L2 对齐开局和地图。
- **M2 第一个角色打通**
  - 铁甲战士的卡、起始和通用遗物与药水、第 1 幕全部怪物和事件、Neow。
  - 第 1 幕随机 bot 整局 trace 零分歧。
- **M3 环境层**（M1 之后可以并行）
  - env、动作与观测编码、隐藏状态 determinize、C ABI/ctypes、VectorEnv、参考 ISMCTS。
- **M4 补全内容**
  - 第 2、3 幕，Underdocks。
  - 静默猎手、故障机器人（充能球）、储君（星星）、亡灵契约师（Osty 宠物）。
  - 无色牌、附魔、苦难、修饰器、全部事件和先古。
- **M5 性能与规模回归**
  - 至少 1 万局随机 bot trace 零分歧。
  - 做 profile，目标战斗每 step 在微秒级，克隆在 1µs 以内。

**版本策略**：锁定 v0.111.0。游戏处于抢先体验阶段、更新频繁。每次更新时：
1. 重跑导出器。
2. 对比两版反编译源码，找出改动过的效果体。
3. 重跑 trace 回归。

**注意**：移植出来的内容代码源自 Mega Crit 的源码，只适合个人研究使用，不要公开发布。

## 批准后首先执行（M0）

1. 创建 `sts2sim/` 的 CMake 骨架、`core/rng.h`、`core/xxhash64`、`core/dec.h`。
2. 创建 `tools/rng_golden`（.NET console，直接编译 `src/Core/Random/*.cs` 和 `Helpers/StringHelper.cs` 中的哈希部分），生成 golden 向量，并跑通 C++ 单测。
3. 写 `tools/Sts2Exporter` mod，产出 `content.json`（含 hook 覆写表），再写生成器脚本。

## 验证方式

- 构建和单测：`cmake -B build && cmake --build build && ctest --test-dir build`。
- RNG 对拍：`dotnet run --project tools/rng_golden > golden.json`，C++ 测试读取后逐项比对。
- 导出检查：content.json 的条目数与本方案里的规模数字一致，每个 `{VarName}` 都能在 `localization/eng/*.json` 中找到对应项。
- 后续里程碑用 L2 到 L4 的 trace 差分器，报告首个分歧点，以零分歧作为验收标准。

---

## 修订记录

| 日期 | 修订 |
|---|---|
| 2026-09-24 | 批准。M0 实施时调整了两点：Sts2Tracer 由进程内 oracle（`tools/Sts2Oracle`，普通 .NET 进程运行 `sts2.dll`，不需要 Godot）取代；内容导出改为独立的 .NET 程序（`tools/Sts2Exporter`），不做成游戏 mod |
| 2026-09-27 | 用户调整为通用 Env 与 RF 类预测 API 主线，并冻结 CombatSolver 历史代码；除用户重新明确要求外不自动恢复。完成范围与验收阶段见新路线。本机 `sizeof(GameState)=145936` 字节，尚未达到约 8KB 的初始设计目标；Clone/Step 性能未验收，目标没有因该测量自动放宽 |
| 2026-09-27 | 实际长战斗容量修正后，当前工作树 `sizeof(GameState)=340560` 字节，本机 Clone 均值约6.60µs、EndTurn Predict约200.50µs；旧尺寸仅属扩容前基线。恢复实际语料有6/8失败，第一幕跨幕有限对拍通过不等于完整恢复/三幕完成。见[当前勘察](reports/2026-09-27-env-development-status.md)，原性能与正确性目标保持待验收 |
| 2026-10-02 | 动态池改为地址稳定的拥有型小块，逻辑capacity/cursor/hash/快照与规则保持不变；GameState本体12152字节，不含堆存储。本机普通Clone均值4.548μs，仍未达到历史1μs目标；搜索同21场87.632→46.576秒且路线一致，完整门禁及容量专项通过，见[性能报告](reports/2026-10-02-combatsolver-performance.md)。M5的完整规模目标未因本次提速自动验收 |
