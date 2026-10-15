# 通用 Env 小批：Driftwood 重掷奖励

日期：2026-09-27。分支：feat/generic-env-rf-api。游戏基线：v0.111.0。

## 目标

按正版 `Models/Relics/Driftwood.cs` 移植 Driftwood 晚期奖励 Hook：仅对遗物持有者的奖励集合，把其中每个普通 `CardReward` 标记为可重掷。保持奖励 Hook 阶段顺序和原版返回值，不加入策略字段或 RF 专用预测逻辑。

## 源码语义与实现

- 正版 `TryModifyRewardsLate` 先比较 `player` 与 `Owner`。不匹配时立即返回 `false`。
- 匹配时用 `OfType<CardReward>()` 遍历奖励并设 `CanReroll = true`，最后返回 `true`，即使该集合没有卡牌奖励也一样。
- `RewardType::Card` 对应普通 `CardReward`；`SpecialCardReward` 是独立类型，不应被标记。
- Driftwood 的 `Ancient` 稀有度已在 `data/content.json` 的 `RELIC.DRIFTWOOD` 条目中记录，模型 getter 从内容表读取。
- 按 Hook 调度，`TryModifyRewardsLate` 在所有普通 `TryModifyRewards` 监听器之后运行。只修改目标持有者的晚期 Hook，不改变 `RewardsSet` 的 Populate、Hook、AfterModifyingRewards、排序顺序。
- 已有 `Reward::st()` 可变状态访问器，直接写 `can_reroll=true`，不新增公共 setter。

## TestMode / headless 边界

- Driftwood、奖励生成、普通 `CardReward` 的 `CanReroll` 状态和 `CardRewardAlternative.Generate` 的 `REROLL` 选项判断没有直接读取 TestMode。
- 正版 `RewardsSet.Offer` 在 TestMode 下调用 `testSelector`；没有 selector 时会自动选完奖励。`OracleRun.RunOne` 安装自己的 `RewardsSet.testSelector`，因此本计划使用的是 Oracle 的受控选择器，不是该默认“全部选取”分支。生产 UI 仍由 `NRewardsScreen` 显示。
- `Reward.CreateIcon` 的 TestMode 分支只省略图标节点，属于表现层。`ProductionBranches` 当前恢复的商店药水价格和两种遗物奖励逻辑不涉及本批奖励 Hook。
- 因此可使用全流程 L4 对拍作有限 headless 证据；它不等同于全面 TestMode Off 或真机验收。

## 定向验收配置

使用 Oracle `run` 而不是 L3 `combat`：L3 战斗工具只覆盖战斗状态和战斗结束附加奖励，不经过常规房间的主 `RewardsSet` 选择路径。L4 run 可通过 `--relics DRIFTWOOD` 从开局持有该遗物，并比较正常奖励生成和后续整局状态。

候选固定矩阵为 Ironclad、A0、不开 Neow、HP 999、DRIFTWOOD，run seeds `DWDRIFT001,DWDRIFT002` × policy seeds `1,2`。每次验收都先查看 trace：必须至少出现一次主奖励集合中的普通 `Card` reward，之后才把该局视为触达奖励路径。HP 999 仅用于提升走到后续房间的概率；不预设奖励内容或随机期望。

```sh
mkdir -p /tmp/driftwood-oracle-2026-09-27
~/.dotnet/dotnet tools/Sts2Oracle/bin/Release/net9.0/Sts2Oracle.dll run \
  --seeds DWDRIFT001,DWDRIFT002 --characters IRONCLAD --policy-seeds 1,2 \
  --max-decisions 3000 --ascension 0 --no-neow 1 --hp 999 --relics DRIFTWOOD \
  > /tmp/driftwood-oracle-2026-09-27/a0.jsonl \
  2> /tmp/driftwood-oracle-2026-09-27/a0.stderr
python3 tests/l4/l4_run.py \
  --trace /tmp/driftwood-oracle-2026-09-27/a0.jsonl \
  --cli build/sts2sim_cli -v
```

**直接验证重掷状态需要 Oracle 侧可观测性。** 原有 L4 trace 的 Card 项不包含 `CanReroll`，卡牌选择 decision 也未记录传入的 alternatives；本工作增加以下只读 JSON 字段，使主线程可同步 L4 C++ replay/schema 后直接差分这些事实。

### Oracle 只读事实输出

为让 Oracle trace 能直接验收上述状态，只扩展 `tools/Sts2Oracle/OracleRun.cs` 的只读 JSON，不改变 policy、随机数或所选动作：

- `RewardJson` 的 Card reward 对象增加 `can_skip`、`can_reroll`、`has_been_rerolled`，取自正版 `CardReward.CanSkip`、`CanReroll` 和私有 `_hasBeenRerolled` 字段。
- `GetSelectedCardReward` 的 `decision.options` 增加 `alternatives`（按原版传入顺序输出 `CardRewardAlternative.OptionId`）及 `can_reroll`（alternatives 中包含 `REROLL`）。保留既有 `can_skip=true` 与 `min`/`max` 语义不变。
- 原版键名按 `CardRewardAlternative.Generate` 源码保留大小写：`Skip`、`REROLL`；不做格式化或本地化转换。

这样 rewards decision 的 `options[]` Card 对象可核对 reward hook 改写后的 `can_reroll`；随后的 `card_select` decision 可核对 UI 收到的 alternative 标识。L4 C++ replay/schema 的同步修改由主线程负责，不在本工作文件范围内。

## 工作范围

- `game/Models/Relics/Driftwood.h`
- `tools/Sts2Oracle/OracleRun.cs`（只追加原版事实字段）
- `docs/plans/2026-09-27-env-driftwood.md`
- `docs/reports/2026-09-27-env-driftwood.md`

生成器登记、构建、C++ replay/schema 和公共查询/Env 集成由主线程完成。本批不改 oracle policy、随机数、动作、生成产物、Hook、Reward 实现、Python 或 CMake。
