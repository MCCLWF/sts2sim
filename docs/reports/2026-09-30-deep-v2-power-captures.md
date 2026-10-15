# Deep-v2 Power 出牌捕获修复

属于 E3/M4 条件覆盖；父版本 `6f6939e`，行为基底 `68b92bb`。完整 Env/RF、全条件合法集合和逐抽 RNG 认证仍未完成。本项不修改原版 DLL、Oracle 决策、随机抽取或比较规则，也未合入 main。

## 新批原始结果

200 个全新 D 前缀 run seed × 5 角色 × A0/A10，共 2000 个不同身份，各有独立 policy seed，使用 `coverage-v2`、Neow、stop=victory、**HP=10000 注入**。源码树 `sts2sim-seed-deep` 冻结 `68b92bb`；原 CLI SHA256 `e515cba636b2145815fd23f0c45bd58088317fa5ec793e0ba5c829900fb281e4`。完整来源、Oracle 运行时、外部资源、动作与输出 SHA 均保留在批 manifest。

原批进程已 EOF，exit1：2000/2000 finished，**1947 identical / 45 diverged / 8 native_error**，2318338/2341451 决策，逐点快照后继验证开启。Oracle 终点 158 death@act0、853 death@act1、338 death@act2、651 victory@act2；其中完整匹配终点分别为 157、847、327、**616**，不能将 Oracle 总胜利数当成匹配胜利数。

证据根是 workspace 下 `artifacts/legacy/mass-seeds-2026-09-30/`，原批目录 `deep-v2/`。原 manifest SHA256 `be2d71e0b7043f6462421cea8a5f70749a6257eedb6e2b27c1259037be2a77ce`。原生合法集合、双端逐抽 RNG 和真实 UI 合同并未因此获得认证。

## 首因与实现

`DD39B3661521B08 / DEFECT / A10 / policy=761454121` 的第 747 点，MadScience skill 后原版 energy=4，模拟器 energy=8 并额外 Channel。原版 Storm、Subroutine 私有 Dictionary 保存 `(CardModel, Before amount)`；旧移植占用 `CardState.extra[0/1]`，且非 Power 的 After 读取并清零同样字段，破坏 MadScience Type/Rider 和 SovereignBlade 的专属数据。

两者改用已存在的 POD `active_card_play_power_captures`，按 `(power, card)` 保存、完成时取出一次。保留原版 Dictionary.Add 的重复键错误以及不同 power/卡牌的隔离，使用 Before 的量而非 After 时的量；不新增 POD 字段，也不改变卡牌专属字段。

Rupture 原版在 Before.Add(card,0)，自身当前阵营回合受到未阻挡伤害时，若 source card 在字典中，则累积**当前 power amount**；否则立即 Apply Strength。AfterCardPlayed 才取出缓冲值 Apply Strength。旧移植提前增加 Strength，导致 Hemokinesis/Breakthrough 同次攻击伤害增加。新增既有捕获条目的 Add helper，使用 C# unchecked int 对应的 32 位环绕运算；原版 Apply 的 cardSource=null 保持一致。

现有 GC、Power 移除清理和快照字段遍历继续用于该 POD 捕获表。测试补充了捕获中的快照往返与相同后继，未认证任意 live 原版内部执行点恢复。

## 验证

1. 正版 `v0.111.0`、`TestMode=false` 的 `tools/PowerCaptureChecks` 直接调用原方法，并反射只读私有 Dictionary：10 行确认 Skill 配置不变、Storm=3/Subroutine=5 捕获后量变成 7/9、重复键拒绝、Rupture 嵌套不同卡缓冲 5/3、阻挡和敌方回合忽略、无卡来源立即 Strength=3、依次结束 Strength=8/11、重复 After 无额外结算。构建和执行 exit0。原始 stdout SHA256 `0d96f08bf02c8763cdf4251c62f661ef6ef403994926aa53ab3ae6795c1e2b4b`。此工具验证方法原语，真实完整轨迹由下一项覆盖。
2. 新 `power_card_capture` CTest 的期望来自以上原生数据和源码；保存途中快照、完成回调后比较恢复后继。独立构建与执行 exit0。
3. 原批全部 **53 条失败原件**严格离线重放，不生成替代动作：**38 identical / 12 diverged / 3 native_error**，57740/64722 决策，完整匹配 27 victory@act2、4 death@act1、6 death@act2、1 death@act0。比较逐点 snapshot successor 开启，进程 EOF、exit1，诚实保留尚未修复项。目录 `deep-v2-power-captures-all-replay/`，manifest SHA256 `d86ef0a3704f9692c4b893865cd2f5f389d0d360f2d713871ec45fd3e6901919`；候选源码 SHA256 `39eefba94091cbd3ba5cd33ef623c19d529f9760fc1acd782c7e1679c422c653`，CLI SHA256 `cfc8e204344b96ab456f63d1510b501e0681fae8f327892827d5a592fd8827eb`。原批其余 1947 条尚未在该候选版全量复验，不合并为候选版全 2000 通过数。
4. `taskset -c 0-2 scripts/check.sh` 已结束，日志终点 `all checks passed`：49/49 CTest，L2 200 runs 无差异，L3 81 complete combats、1625/1625 状态，另外 2 bounded probes、4/4 状态；L4 103 runs、18956/18956 决策。执行句柄在会话续接时已失效，最终日志可确认完整门禁通过，未单独保留进程退出码回执。日志 SHA256 `ff4d7bb068dd5dc1539ea1409387c7c37489e9ef55d5d55bf3e16dd22b299073`。

## 保留缺口

原失败剩余 12 条规则差异的原件、首点和动作均保留。已确认源码差异包括 DoorsOfLightAndDark 的 StableShuffle 需要 .NET List.Sort 而非 stable_sort、Cascade 忽略 ResolveEnergyXValue/ChemicalX、DrumOfBattle 的 AfterCardExhausted 缺少 GeneratePlayCount；其它 Iteration、EmotionChip、临时费用克隆及伤害/格挡条目仍待完整首因证明，未纳入本提交。

3 条容量失败仍在：`5ed18323ad45b4737519` 第 2023 点和 `d46e1014f30656a5d3d9` 第 1425 点有 1023 张仍存活卡；`83eac4924240268bd795` 第 2343 点、round1025 超过 StateLog 1024 容量。不能通过删除活卡、盲目截断历史或放宽严格比较来计为通过。
