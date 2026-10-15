# CombatSolver Env 运行与战斗组件

日期：2026-09-26  
分支：`feat/combatsolver-env-backend`  
计划：[M5 后端](../plans/2026-09-26-m5-combatsolver-env-backend.md)

## 前置与范围

基线 `c7abca5` 已实现卡牌/怪物组件与严格 Env 快照传输，但实机发现怪物个人 RNG 与种子模板不同。完整 live 根和同算法 Env 执行仍未完成。本次增加各实际组件，**不将混合组件测试标为完整根验收**。正常游戏安装和用户最终交付标准不变。

## 实现

- RunContext：12 条 run 和 3 条 player RNG 的 counter/四字、原始 odds float 位、seed、幕队列/游标、坐标、地图、遗物袋、人物运行字段。验证 RNG 类型/无别名、odds 绑定/定义/枚举顺序、模型与容量。
- Collections：从实际 .NET 9 HashSet 的槽位与移除链转换为 DotnetSet；拒绝未知 comparer、布局、顺序和自由链。地图网格的 int8 引用另设边界检查。
- Profile：实际 run/player 的 epochs、seen encounters、NumberOfRuns 独立拥有。部分档案可以被捕获，但当前 Env 全解锁消费者明确拒绝并显示具体计数；不会将源改为 UnlockState.all。
- Creatures：HP/block/side/slot/CombatId、人物/宠物与能力引用/顺序，amount/duration/精确变量、已审计私有计数。绑定冻结怪物时验证模型 ID。非 Normal HpDisplay、未知状态和精度损失拒绝。
- Inventory：活跃遗物变量/熔化/蜡/状态/楼层、固定八槽药水空位、Dark/Glass 实例 decimal 和队列。修改药水固定变量、排队状态、多叠遗物及未知字段拒绝。
- CombatContext：实际管理器/动作队列静止边界、遭遇 RNG/生成列表、资源及九项历史聚合。原版 HappenedThisTurn 参与判据；冻结其查询依据而非声称保留完整历史。
- 防遗漏：内容导出表会省略引用/自动字段；运行时遍历实际子类字段另作严格检查。SpoilsMap 自动属性等会明确拒绝。
- API：GetPlayerRng 读取 Rewards/Shops/Transformations，值在 session 释放后仍属于快照。

全部组件在主线程原稳定性边界内冻结；卡牌/能力/库存 lazy 变量在 before stamp 前预热。后台持有 owned JSON。

## 独立检查

```bash
~/.dotnet/dotnet build tools/CombatSolverEnv/CombatSolverEnv.csproj -c Release
~/.dotnet/dotnet run --project tools/EnvSessionChecks/EnvSessionChecks.csproj -c Release
~/.dotnet/dotnet run --project tools/EnvLiveCardChecks/EnvLiveCardChecks.csproj -c Release
```

Mod 构建 0 warnings/errors；managed session/API/import 检查通过。完整组件工具 0 warnings/errors、exit 0，原始输出末段：

```text
live run component: all 15 RNG streams, exact odds, coordinates, acts/bags/map links, owned restore and native Shuffle-driven prediction passed; full_root=false
live profile component: actual partial progress owned, all-unlocked compatibility explicitly refused; full_root=false
live creature/power component: exact roster, HP/block, refs, decimal vars, private counters, owned C++ restore and native-derived Strike prediction passed; full_root=false
live inventory component: exact relic/orb values, ordered potion holes, owned C++ restore, pure potion prediction and explicit variable/counter rejection passed; full_root=false
live combat context: native histories including extra-turn boundaries, actual encounter RNG/rosters/flags/resources, owned C++ restore/predict and async-boundary rejection passed; full_root=false
```

工具以正版对象、RNG Shuffle、能力伤害回调、实际历史 entry/query 为参照。每个预测夹具仅替换所验组件，其他字段明确是 seed fixture。库存 FirePotion 判据来自原版变量值及无格挡目标，不覆盖原版完整 OnUse；充能球触发/激发尚未对拍。无头存档桩有一条 `Failed to open file` 日志，工具逻辑检查均完成；不能把它当作真实客户端存档验收。

本机保留日志：`/tmp/mcc-live-combat-context-tool-{build,check}.log`、`/tmp/mcc-live-components-{mod-build,session-check}.log`。不提交日志和游戏文件。

## 实机与核心回归

```bash
python3 tools/CombatSolverEnv/run_smoke.py --expect-profile-root-rejection --memory-reservation-mib 2048 --out /tmp/mcc-env-live-components-game-20260926
```

通过，launcher `Passed` 且 `UNATTENDED_INSTANCE_REMOVED`。实际冻结：cards 20、monsters 2、creatures 3、powers 0、relics 1、potions/orbs 0；run 12 流/player 3 流、map 65 点，当前坐标 `(3,0)`。原始档案与拒绝：

```text
LIVE_PROFILE_COMPONENT source=run runs=0 epochs=57 encounters_seen=0 missing_epochs=0 missing_seen_encounters=85 full_veteran_compatible=False immutable=true full_root=false
LIVE_PROFILE_COMPONENT source=player runs=0 epochs=57 encounters_seen=0 missing_epochs=0 missing_seen_encounters=85 full_veteran_compatible=False immutable=true full_root=false
BRANCH_ENV_SUMMARY native_expanded=100 native_transitions=335 env_checked_transitions_root_cumulative=0 env_disabled=True reason=live_root_components:live_run_requires_fully_unlocked_veteran_profile:run(runs=0,missing_epochs=0,missing_seen_encounters=85),player(runs=0,missing_epochs=0,missing_seen_encounters=85) scope=structural_and_combat_rng root_source=seed_snapshot_import native_timing_includes_verification=true
```

游戏 UI `ENV_BRANCH_WARNING shown=true`，Native 继续搜索。RSS 采样峰值 1,463,124 KiB，小于 2048 MiB 预留。证据目录为上述 `--out`。此场景拒绝早于原种子怪物检查；自然根接受 fixture 仍未通过，不能以拒绝用例代替。

历史审计发现 `player_attack_skill_cards_started_this_turn` 在额外回合错误保留。原版 HappenedThisTurn 同时比较 side/round/人物 TurnNumber；实际 SwitchSides 在额外回合也增加 TurnNumber。现改为完成 side/回合更新后、OnSideSwitch 前归零。

```bash
~/.dotnet/dotnet run --project tools/ExtraTurnHistoryChecks/ExtraTurnHistoryChecks.csproj -c Release
ctest --test-dir build -R '^colorless_m4$' --output-on-failure
```

原版工具 TestMode=false，通过实际 SwitchSides、history query、Nostalgia 位置修改：额外回合同 round/turn+1、旧 started count=0、第一张 Attack 回 Draw/Top；Enemy 切换与正常下一回合也清零。明确 `full_turn_and_damage_effects_executed=false`。C++ Nostalgia/Hellraiser/Ambergris 整回合回归通过 1/1（3.69 s）；旧逻辑负向回归 exit 83，修复后通过。12 点伤害期望基于上述原版 primitive 与逐字调用顺序，未声称原版完整回合伤害对拍。

`scripts/check.sh` 全门禁 exit 0：9 个 CTest 通过；L2 200 runs、0 mismatched；L3 80/80 combats、1611/1611 states matched；L4 80 runs、4089/4089 decisions matched、0 unported/diverged；三个整局 smoke 均完成。日志 `/tmp/mcc-live-context-full-check.log`。这些回归覆盖原有引擎，不把局部 mapper 扩大为成品证明。

## 未完成

完整历史/模型注册与已移除对象闭包、历史 pool 游标、其他模型扩展字段、附魔/污染、其他怪物动态定义/条件/stun、其余场景状态及完整 live 文档组成仍待验证。Cpp 全解锁假设尚未支持实际部分档案。

审计发现 JugglingPower.AfterApplied 原版读取本回合 Started Attack，Cpp 当前初始化 0；不能用 Finished Attack 代替。HpDisplay 会参与 Hellraiser 自动出牌限制，当前捕获拒绝非 Normal，但其未来效果的字段/更新尚未移植；本组件检查不能证明这些效果已保真。

生产分支仍标记 `seed_snapshot_import`，未把局部组件拼成假完整根。Env 尚未完全接管原 CombatSolver 算法；性能对比、公用设置 UI、连续回合/选择/部署及最终 mods 成品仍是原目标的完成条件。

## 本阶段增量（58fc38e 后）

- C++ `CreatureState` 增加原版 `HpDisplay` 三态并进入协议 2、逻辑哈希、严格导入；HardenedShell 保留私有 `decimal` 的四个 decimal bits，Hellraiser 只在所有可攻击敌人无限生命时应用 9 次上限，WaterfallGiant 的爆炸状态写入 `InfiniteWithoutNumbers`。
- Juggling 的 `AfterApplied` 改为读取本回合已开始的 Attack 数量，与已完成 Attack 分开；额外回合 side/round/turn 更新后两项都清零。原版 TestMode=false 工具输出 Started=2、Finished=0、Juggling 私有计数=2，C++ `colorless_m4` 回归通过。
- 新增 `EnvLiveRoomContext`：单层普通战斗静止边界才允许继续，逐一检查 CombatRoom 额外奖励、`RelicObtained` 订阅、奖励同步器 stack/buffer/completed/nextId；任一非空直接拒绝。实际游戏抓到 3 个现存订阅者并安全停用 Env，未清空或忽略它们。
- 房间检查现在还覆盖 RewardSynchronizer、EventSynchronizer、RestSiteSynchronizer、TreasureRoomRelicSynchronizer 的消息、事件、选项、投票、完成任务、hover 和陈旧 bookkeeping；presentation-only 订阅逐一分类，CardReward/未知订阅仍拒绝。`RoomContextChecks` 注入非空事件/页码/休息点/宝箱投票验证拒绝路径。
- 新增 C++ seed-free blank template 与 `EnvLiveRootAssembler`：模板不创建新局、不抽 RNG，live 组件覆盖真实字段，未映射的 run 子状态只保留经房间契约证明为空的 POD 默认；缺字段或默认不空直接拒绝。当前实机 profile 仍在 assembler 前拒绝，因此尚未有 live root acceptance。
- 设置页现在有持久化 Native / Env（不支持时回退）/ Both 和 Env/Native 对比开关；Native 会跳过 Env 入口，Env 不可用时显示原因并由 Native 继续，Both 才等待两条路径。这里的 Env 仍是参考 Beam 核对/部署，尚不宣称已经替换原生搜索后端。

验证：`scripts/check.sh --quick` 通过（9/9 CTest）；真实 `--expect-profile-root-rejection` 通过，含 2048 MiB RSS 约束，新的房间检查会在真实奖励订阅存在时拒绝。
