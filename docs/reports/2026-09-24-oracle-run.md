# oracle `run` 执行报告

日期：2026-09-24  
分支：`feat/oracle-run`

## 实现

- `Sts2Oracle run` 在 `Oracle.Run` 注册，支持 `--seeds`、`--characters`、`--policy-seeds`、`--ascension`、`--max-decisions`、`--stop before-act1-boss` 和 `--rng-ledger`。
- 每个组合建立新局，调用 `RunManager.EnterAct(0)`，用 `SingleThreadContext.Pump` 驱动异步延续。当前局状态由 `DumpRun` 生成，字段与计划中的 RUN 协议一致。
- 地图选项来自当前 `MapPoint.Children`，按 `(col,row)` 升序；进入第一幕 Boss 的边已经到达时输出 `reason=boss`，不进入 Boss 房。
- 战斗沿用 `OracleCombat.Dump`、`PlayCardAction` 和 `EndPlayerTurnAction`。动作策略为可出牌时 85% 随机出牌，否则结束回合；目标按战斗生物的自然顺序展开。`NEUTRALIZE`、`SURVIVOR` 继续跳过，避免尚未完成的特殊视觉选择分支。
- 奖励由 `RewardsSet.testSelector` 驱动，选项按 `RewardsSet.Rewards` 顺序；每次领取前输出一次 `rewards` decision，策略在“剩余奖励”和“proceed”之间均匀选择，放弃剩余奖励时调用 `SkipLocalRewardsSet`。卡牌奖励通过 `ICardSelector` 记录为 `card_select`。
- 所有 `CardSelectCmd` 选择由 `CardSelectCmd.UseSelector` 接管，选项保持调用方传入顺序，随机选取满足 `min/max` 的子集；`source` 在无状态机方法名可见时写 `CardSelectCmd`。
- 事件从 `EventSynchronizer.GetLocalEvent().CurrentOptions` 读取，保持显示顺序；锁定选项和 `WillKillPlayer` 选项不进入策略候选集。Neow 也走同一路径。
- 休息点从 `RestSiteRoom.Options` 读取，按显示顺序输出 `OptionId` 和 `IsEnabled`，随机选一个启用项。
- 宝箱通过 `TreasureRoomRelicSynchronizer` 的真实入口 `PickRelicLocally`/`SkipRelicLocally` 选遗物，再调用 `TreasureRoom.DoNormalRewards`；遗物顺序来自同步器当前遗物列表。
- 商店条目来自 `MerchantRoom.GetLocalInventory().AllEntries` 的自然顺序，输出卡、遗物、药水和移除服务；当前策略只输出 `leave=true`，下一次地图动作实际离开商店。

## 无头改动和协议决定

- `GameBootstrap.UseInMemorySaves` 现在初始化 `SettingsSave` 和 `PrefsSave` 的测试默认值。真实游戏在 Godot bootstrap 中会做这件事；无头模式下 `CombatManager.DoTurnEndCards` 会读取 `PrefsSave.FastMode`，否则带回合结束牌的战斗会空引用卡死。这不改变游戏逻辑。
- 移除了 Oracle 项目的 `InvariantGlobalization`。Neow 的遗物选项会格式化带复数规则的本地化文本；强制 invariant 模式会抛出 `CultureNotFoundException`，而正常 .NET 文化实现与游戏的本地化路径一致。
- `--stop` 未提供时默认停止在 Act 1 Boss 前，符合本计划的“Boss 房间门口”目标；显式传入其他值可继续使用相同驱动器。
- 商店购买、奖励的具体跳过时机、卡牌选择子集大小均由策略随机决定；商店当前只选择离开，因为计划只要求记录和驱动决策点，购买语义由后续内容里程碑补齐。

## 验收

构建：

```text
/home/wst/.dotnet/dotnet build tools/Sts2Oracle/Sts2Oracle.csproj -c Release --no-restore
Build succeeded. 0 Warning(s), 0 Error(s)
```

快速检查：

```text
scripts/check.sh --quick
100% tests passed, 0 tests failed out of 2
quick check passed (differential checks skipped)
```

批量命令（原始统计由 JSONL 汇总）：

```text
run --seeds 1..20 --characters IRONCLAD --policy-seeds 1,2 --ascension 0 --max-decisions 3000
begin 40, end: death=40, error=0
decision: event=73, map=192, combat=1522, rewards=233,
          card_select=84, shop=16, relic_select=4, rest=3

run --seeds 1..20 --characters IRONCLAD --policy-seeds 1,2 --ascension 10 --max-decisions 3000
begin 40, end: death=40, error=0
decision: event=58, map=148, combat=1155, rewards=190,
          card_select=75, rest=4, shop=12, relic_select=3
```

同一命令连续执行两次，`cmp` 返回 0，输出 110 行逐字节相同（单种子、策略种子 1、`--max-decisions 100`）。

RNG 账本：

```text
run --seeds 1 --characters IRONCLAD --policy-seeds 1 --max-decisions 10 --rng-ledger
exit 0; 输出包含 event=rng，且记录 UpFront、RunRngType 和 other 流
```

批量运行的 stderr 仍会有 headless 初始化阶段的已知资源 stub 日志（`Failed to open file`、资源未缓存警告），不进入 JSON 协议，也没有对应的 `error` 结束事件。
