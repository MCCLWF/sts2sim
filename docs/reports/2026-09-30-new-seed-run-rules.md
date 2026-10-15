# 新 seed 整局首因修复

基底 `5266daf`，分支 `fix/new-seed-run-rules`。与完整 API 分支并行，Env/ABI 新能力由另一聊天负责；状态/GC/快照校验改动已提前同步。已读 AGENTS、design、roadmap、deviations、pitfalls。真值为 v0.111.0 源码及正版 DLL。

## 实际基线

两批新采集均已完成：100 个正常血量 seed × 五角色 × A0/A10 = 1000 身份；另100个不重叠seed × 同十轴、HP10000注入 = 1000身份。每个身份独立 policy_seed。旧 legacy 策略未执行药水、Silent部分起始牌及空选等条件，扩大 seed 不能替代这些条件覆盖。

- 正常血量：975 identical / 25 diverged，35,543/36,520决策；全部第一幕死亡。
- HP10000：941 identical / 54 diverged / 5 native_error，828,163/863,809决策。原版终点有188次第三幕胜利，但包含C++失败身份，不能算作188次一致胜利或正常通关率。

证据根 `../artifacts/legacy/mass-seeds-2026-09-30`；`natural-v1`、`deep-v1` 各有manifest、summary及原始stdout/stdin/进程退出码/指纹。冻结采集树 `sts2sim-seeds` 的计划提交为 `c6b3a4c`，生产代码同main基底。源码SHA256 `cf08098f74ab90b8341ca639802b601ea4277a20f5087534ff550a17c71821b0`，CLI `442ad758be66bea427185eb162c4ef5cd417a5cb9da89f523dcb6e67bf59f41d`，Oracle DLL `4687fd0091b5be91b92071128e615d692f7b0363edfe54d0ec21c1cbff37b7e3`。

## 源规则修复

1. LargeCapsule从Owner.Character.CardPool按Basic rarity、Strike/Defend tag取对应角色牌，遗物数量读取DynamicVar。全部25条正常血量失败修复。
2. Dowsing满足Deck/主房间/Unknown当前地图点时累计RoomsEntered，第五次原生Transform为Abundance；既有extra[0]和Rooms DynamicVar同步。原版CompleteQuest的地图历史记录尚未接入，不能把玩法对齐等同完整历史API。
3. Noisebot使用目标Player ?? PetOwner，生成Dazed的creator=null；修正RocketPunch费用及Supermassive生成牌归因。
4. 新非reduceOnly Absolute费用项删除此前expiration完全相同、被其常数屏障永久遮蔽的项，保留不同寿命；容量仍32。D-008登记内部表示，独立证明见下节。
5. RingingPower.ShouldPlay读取当前回合CardPlaysStarted，包含尚未完成的外层出牌；嵌套自动出牌不能靠AfterCardPlayed标记判断。
6. BingBong新增CardState.bing_bong_skip_relic表示原版HashSet成员身份：返还被盗clone消费一次skip，新clone不继承。GC保留集合强引用，容器遗物回收时清成员；生成字段/snapshot引用校验同步。现有审计测试补首返/二返/新clone三个源规则断言。最初同Deck→Deck夹具不触发pile-change hook，已改为先RemoveInternal再返回，未为通过测试改规则。
7. Dampen恢复字典以原卡对象为键；两种CloneCard均清dampen_original_upgrade，保留实际当前升级。原Ironclad/Defect长局分别1207/1207、1468/1468完整通过。

## 验收与限制

复用全部84条原失败动作和Oracle输出，输入列表fixes-r4-inputs.json；最终fixes-r5为 **76 identical / 5 diverged / 3 native_error**，56,881/59,708决策。正常血量25/25完整一致，注入51/59完整一致。其余1916条基线通过身份尚未在本版本重放，不能拼成同一运行时1992/2000认证。

最终重放源码SHA256 `950527f06c87366e9021dc50269a600955d572b08d58c5888ac66648afa54107`，CLI `d0005cd7e9bd93f54d574497669460b132cb035b42a5953ce9b40db9841e321a`，manifest `4c619d86d5a6ad24d0639b2bc9d61f82ff827eb39ce14f1fd62023e5d3731b2b`。报告文字不属于源码指纹。

剩余5条规则分歧：3条共享CardCreationOptions/Candy问题和2条Regent免费星数/生成牌问题由独立分支处理，尚未集成至本资格。3条容量失败：2条活牌超过1023卡池，1条怪物历史超过1024；继续明确失败，不能删活牌或无条件截断历史假造一致。

完整 `taskset -c 0-2 scripts/check.sh` 退出0：46/46 CTest、L2 200/200、L3 81完整战斗/1625状态及2短探针/4状态，L4 99条固定路线/16,379决策一致。亲和性只限制并行度，没有缩减门禁。原日志new-seed-rules-check.log SHA256 `e2e7ec0d3d573add537153195860689a752f9b783e1d8b8d46040147f553bc8b`。

```bash
cmake --build build --target sts2sim_cli -j 3
python3 tools/differential/batch.py replay --out /path/to/new-output --jobs 4 --traces /path/to/original/oracle.stdout.jsonl
taskset -c 0-2 scripts/check.sh
```

未来800个新自然seed已冻结，共8000身份，与本批200个run seed不重叠。应先集成coverage-v2策略、级别参数合同及两端custom Rewards自动完成修复。全内容条件、双端逐抽取RNG、原生完整合法动作集、任意容量上界、真实客户端接受和完整API尚未认证。

### 同寿命局部费用项压缩的独立复核

审核对象是 `game/Models/CardModel.cpp:87` 的 `add_modifier`：新项为 `Absolute && !reduceOnly` 时，仅删除**此前 expiration 完全相同**的项；不同寿命保留。审核时基底 `5266daf736e6f5038d8e29d6b4847279d83eced3`，该文件 SHA256 `c047091526376513849f85a137f3468345e80932ae7df77c857b4e15efe3142f`。审核没有改动 root 源码。

原版依据（均为 v0.111.0 的 `src/Core`）：`Entities/Cards/LocalCostModifier.cs:89` 的 Modify，`Entities/Cards/CardEnergyCost.cs:94` 的按顺序求值、`:331/:341` 的两种 cleanup、`:351` 的 UpgradeBy、`:387/:399` 的 base 重置及 `:411` 的深复制；`Models/CardModel.cs:1194/:1203` 和 `Combat/CombatState.cs:189` 的卡牌克隆入口。

等价性：非 reduceOnly Absolute 是常数函数，因此活着时遮蔽全部前序项；被删项与新屏障具有完全相同的 expiration，两种 cleanup 对它们的删除谓词相同，屏障消失时被删项也已在原版消失。UpgradeBy 仅 cap Absolute.Amount，不改变其常数性、类型或寿命；重置 base 也不能越过屏障。深复制保留此关系并隔离克隆修改。HasLocalModifiers 仍有同寿命屏障作代表；只要原版删除了被压缩项，cleanup 同时会删除屏障，返回 bool/通知条件也保持一致。不同寿命与 reduceOnly 新 Absolute 不适用此前序删除规则。只认证规则行为，不认证原版私有 List 的逐项表示相等。

独立源规则差分：**10,017 个序列，258,520 次操作观测全部相同**（不是 1,017）。`.NET` 探针在 `/tmp/mcc-cost-proof-20260930/source_probe` 编译冻结复制的原版 `CardEnergyCost.cs`、`LocalCostModifier.cs` 与三个 enum 文件，费用算法未改；`Stubs.cs` 仅提供 mutable CardModel/owner/通知及无 CombatState 的宿主，global Hook 不执行。C++ 探针在同目录 `native_probe.cpp`，调用实际 CardEnergyCost 和 CombatState::CloneCard，链接冻结的 root 静态库。比较 Local/None 费用、HasLocalModifiers、WasJustUpgraded 和 cleanup 返回值；含四种 expiration、reduceOnly、升级/重置/Finalize、cleanup 顺序、克隆原件隔离，以及 1,000 次 Confused SetThisCombat / Melancholy AddThisCombat 交替。另有 seed640165661 的 10,000 个随机 24 步序列。native 全矩阵最多 19 项。原始输入、双端输出、进程命令/退出码、源及库哈希分别在 `probe.stdin.txt`、`probe-*.stdout.txt`、`probe-*.process.json`、`probe_inputs.json` 和 `probe_comparison.json`。

原始长局 `deep-v1/jobs/ab8143b7237070fb862a`（MB8341B3E917041 / NECROBINDER / A10 / policy640165661 / HP10000），曾在缺失第842决策、round55报 LocalCostModifier32 容量错误。本次完整重放原始 stdin、打开逐点快照后继验证，exit0；严格 `l4_run.compare_run` 比较每个决策及终点得到 **identical 1148/1148**，终点 act2/floor43/death。命令：

```bash
/tmp/mcc-cost-proof-20260930/sts2sim_cli replay --seed MB8341B3E917041 --character NECROBINDER --ascension 10 --hp 10000 --stop victory --verify-snapshots 1 < /home/wst/Desktop/MCC/sts2sim/artifacts/legacy/mass-seeds-2026-09-30/deep-v1/jobs/ab8143b7237070fb862a/replay-00000.stdin.txt
```

| 输入/工具 | SHA256 |
|---|---|
| 冻结 CLI | `ca7c3b2168d66be43a66cabdfff82436289eb060be42c8660a756ea7522747df` |
| 原始 stdin | `1f42b1291fb11234d641569c0b9d7948c5bff91b07a6b7b8f5d28f2548b105e9` |
| 原始 oracle.stdout.jsonl | `f3c84081d9e67d70d84eac7a029d10b3719354a62a1da11ae6414cd34ea8679d` |

证据根目录 `/tmp/mcc-cost-proof-20260930`；原局比较 `comparison.json`、命令 `replay.process.json`、工具与输入指纹 `freeze.json`、全产物指纹 `artifact_manifest.json`。`rerun.py` 可复跑以上两类验证，不需要在报告内内联原版源码。`minimal-source-regression.json` 提供 17 个源派生参数化场景、238 个操作；将原压力序列缩到 40 个循环，仍超过旧32项容量，可作为单个数据驱动 CTest 的候选语料。

限制：长局属于注入 HP10000 资格；快照验证使用同一冻结 CLI 的当前 schema，不证明旧 schema 迁移。独立探针只认证 local 规则，未认证 global Hook/完整原版宿主。此次压缩没有证明所有可构造的不同寿命或 reduceOnly 累积都不会超过32项；int16 存储与整数边界仍是既有范围限制。
