# 通用 Env：局外查询、选择描述与 owned 预览结果

日期：2026-09-27。分支 `feat/generic-env-rf-api`，起点 `ac4c77c`。游戏真值 v0.111.0。计划：[局外查询与结果](../plans/2026-09-27-env-run-query-results.md)。

## 结论与范围

本批公共接口、选择描述、结果投影与有限原版差分已完成。通用 Env / RF **整体仍未完成**，CombatSolver 继续冻结。下一项主要缺口是普通跨幕/最终胜利及相关 oracle 主流程；其他角色与内容、真实档案、悬停操作、完整 trace、独立 SDK/捕获桥和 RF 全功能验收继续在原完成路线中。

当前新增能力让客户端通过公共接口读取牌组/资源、商店、奖励和选择，并预览正常购买、拾取、奖励及回答选择后的状态。引擎执行仍依据原版 Cmd/Hook/选择前缀；没有加入 RF 镜像算法或搜索评分。之前回退并归档的搜索专用预测结算仍保持撤销。

## 实现

- `env/run_queries.*`：`sts2env.run_queries` v1，当前玩家资源、牌组、遗物、药水槽；合法动作；奖励、商店、事件、休息、宝箱；当前选择。事件给原版文本键；商店保留所有槽、当前库存、Hook 后价格和 stocked 索引 `action_index`，可以映射普通 ShopBuy。
- 默认 `future=null`；显式开启才输出预选下一幕的幕/先古/Boss。查询不给 RNG、未来房间队列或遗物袋；它是预测消费者接口，明确 `is_agent_observation=false`。
- 所有 getter 在 owned 私有副本运行；前后检查私有与源 Hash。读取不会调用 Populate、CalcCost 或抽随机数。实际调用未实现 getter/Hook 时失败；模型 metadata 的 `static_usable` 只是现有静态 whitelist，不能解释为完整行为覆盖。
- 原版选择调用方产生 card/bundle/reward/alternative/proceed 描述；保留真正的卡包分组、奖励引用与原始 `Skip`/`REROLL` 键。Hash 和 snapshot 纳入描述。GameState 未增加动态成员，描述在 Env/Sim 侧的 pending request 中。
- 可选 `PreviewOptions.capture_run_results` 与 `preview_with_results`：普通 owned 后继加源/后继 run-query 投影，`sts2env.preview` v2。普通 preview 保留 v1。投影捕获有额外 clone/getter/JSON 成本，本批没有量化性能。
- Answer 的 before 是当前部分前缀状态；trace 会从静止根重放全部答案。`run_results.semantics=source_to_successor_projection` 明确其含义，`trace_complete=false` 保留；没有把投影宣称为完整因果账本。refs 仍限定对应 owned 状态。
- C ABI 新增 `sts2_env_export_run_queries` 与 `sts2_env_preview_with_results`；Python 包装已接入。flags 只接受 0/1，C action kind/坐标在窄化之前检查。失败不交付后继；普通 Step 的可变失败合同保持原样。
- logical snapshot 更新 v3，旧 v2 明确拒绝；blank template envelope 仍 v1，内部 snapshot 为 v3。恢复仍限定 loaded native runtime。
- 新增原版 Driftwood、Outmaneuver、Flechettes；后者沿用完整 CalculatedVar 公式，只补其手牌 Skill 倍率。现有 Dagger Throw 复核并差分，未重写。简单实现由 GPT-6 Luna xhigh 完成；后续子任务遇到额度限制，root 接手统一构建及验收。

API 用法与寿命合同：[Preview 与查询 API](../env-preview-api.md)。

## 独立真值对拍

新增 `tools/check_run_queries.py`，读取 **OracleRun 原版字段** 与 CLI `replay --run-queries 1` 调用公共 WriteRunQueries 的结果。严格拒绝缺失决策、错误和 Unported；没有用 C++ 的另一份 dump 作为期望值。

比较范围：非战斗边界的 HP/MaxHP/金币、牌组 entry+upgrade 顺序、遗物顺序、药水槽和幕/层数；地图动作；奖励字段与候选；商店 stocked 索引、item/价格/可买；事件模型/文本键/locked；休息 key/enabled；宝箱候选；选牌/卡包/alternative 顺序。战斗 oracle 没有 DumpRun，只比较其直接输出的 HP/MaxHP/block/energy/stars、遗物和药水槽；金币/牌组/层数在其他边界验证。原版未输出的动态变量、附件金额及更多子类字段不属于本工具差分范围。

### 实际命令与原始输出

```bash
python3 tools/check_run_queries.py \
  --seeds SEED0001,SEED0002,SEED0003,SEED0004 --policy-seeds 1,2 \
  --no-neow --relics DRIFTWOOD \
  --save-trace /tmp/mcc-env-run-query-driftwood-oracle.jsonl
```

```text
Run query differential: 8 runs, 335 decisions, 0 differences, 0 Unported
Observed kinds: {"card_select": 8, "combat": 269, "event": 3, "map": 28, "rewards": 27}
Observed original REROLL candidate lists: 5
```

8 条都战死。`tests/l4/l4_run.py --seeds ... --policy-seeds 1,2 --no-neow --relics DRIFTWOOD --require-complete --verbose` 对常规状态/RNG 对拍：8 identical、0 Unported、0 divergence、335/335 decisions。

```bash
python3 tools/check_run_queries.py \
  --seeds SEED0001,SEED0002,SEED0003,SEED0004 --policy-seeds 1,2 \
  --no-neow --hp 1000 --relics DRIFTWOOD,THE_COURIER \
  --save-trace /tmp/mcc-env-run-query-rooms-oracle.jsonl
python3 tests/l4/l4_run.py \
  --trace /tmp/mcc-env-run-query-rooms-oracle.jsonl --require-complete --verbose
```

```text
Run query differential: 8 runs, 1703 decisions, 0 differences, 0 Unported
Observed kinds: {"card_select": 66, "combat": 1284, "event": 19, "map": 128, "relic_select": 9, "rest": 22, "rewards": 165, "shop": 10}
Observed original REROLL candidate lists: 36
L4 run: 8 runs: 8 identical, 0 stopped at unported content, 0 diverged; 1703/1703 decisions matched
```

这 8 条都停在第一幕 Boss 前，不是通关；HP 1000 是 oracle 与 C++ 两边一致设置的测试夹具。实际发生 6 次商店购买和 4 次离开；Courier 后续库存与价格在随后商店 decision 对拍。66 次选择来源：奖励 36、FromCombatPile 15、FromDeckForUpgrade 8、FromSimpleGridForRewards 3、FromDeckForEnchantment 3、FromDeckGeneric 1。没有观察到 bundle 或 FromDeckForTransformation。

原始日志：`/tmp/mcc-env-run-query-diff.log`、`/tmp/mcc-env-run-query-rooms-diff.log`、`/tmp/mcc-env-driftwood-l4.log`、`/tmp/mcc-env-run-query-rooms-l4.log`。这些本机原始产物未提交。

### 对拍发现并修复的问题

最初工具误认为所有 oracle decision 都有 run 字段；combat 只有 combat dump。修正工具使用真实存在的字段，没有补造原版期望值。

随后出现真实查询错误：`SEED0002` policy 2、decision 32（最后奖励领取后的奖励界面），`run.rewards.active=false`，公共 queries 错误输出 null。原版此时仍展示 reward entries，等待 Proceed。已按实际 decision 保留这个界面的 entries 和 `active=false`，加入合同夹具，再重跑上述 2038 个 decision 全部通过。

进一步完成 Cauldron 拾取选择时发现旧 `LegalActions` 对 nested rewards 枚举了已领取 reward；执行会报 `rewards: the reward was already taken`。已按原版 `NRewardButton.GetReward` 的禁用状态及 OracleRun.SelectRewards 的 SuccessfullySelected 过滤，使用原始 option/reward 引用排除这些答案。保留描述中的完整列表，合法答案仍用原 index。它没有改奖励效果或随机抽取。Cauldron 原版给五瓶、默认三槽，夹具取三瓶后选择 Proceed，没有通过改变游戏数量让测试通过。

## 合同、内容与门禁

### 公共接口合同

`tests/test_run_queries.cpp` 覆盖查询源/私有分支不变、默认/显式 future；owned before/after 与普通 Predict 一致；Courier 购买补货/价格；NewLeaf 变牌、ScrollBoxes 卡包分组与入牌组、Cauldron 嵌套奖励/proceed 与药水槽最终结果、Smith count/index；Driftwood 执行 Reroll 后的奖励标志与恢复后继；最后奖励领取后的未关闭界面。拾取夹具走完所需答案，校验实际结果投影。房间/遗物由夹具选定后走普通入口与 Step。它们是合同测试，不代替原版差分。

`python/test_env.py` 使用真实共享库，检查 v1/v2、trace on/off、源 Hash、普通 Step/Predict 后继、0/1 flags 和非法 flags、kind=258/-1 与坐标 256/-129 拒绝且失败后继为 null。首次测试的旧 kind=255 用例因现在提前校验而失败，已用 in-range kind=0 保留动作非法路径；修正后通过。

Reroll 合同最初错误要求下一轮保存的 alternatives 移除 REROLL；原版 OnSelect 在循环前保存列表，Populate 刷新 UI 是另一条路径。读原文后改为检查实际 reward.can_reroll=false 和 has_been_rerolled=true，执行循环没有擅自变更。普通 oracle 策略本批只选卡，没有执行 REROLL action，UI 与 headless 保存列表差异仍需专门原版验证。

### 内容验收

Outmaneuver、Flechettes 与既有 Dagger Throw：原版 L3 50/50 场、702/702 状态对齐，零 divergence/Unported；普通/+1、A0/A10，Flechettes 0/1/2 Skill 真实分支均覆盖。详细配置与日志见[内容报告](2026-09-27-env-silent-draw-discard.md)。Driftwood 原版 owner/Card-only Hook、CanReroll 与 41 个 REROLL 候选列表的证据见[遗物报告](2026-09-27-env-driftwood.md)。

`python3 tools/coverage/env_inventory.py`：Card 596 official / 254 headers / 252 whitelist；Relic 299 / 183 / 180。其余类计数同起点。后续按正式卡池复核，Silent 卡池头文件只增加 Flechettes 一张至 27/91；Outmaneuver 属于 EventCardPool，虽在 Silent 自定义牌组中测试，不能算入该角色卡池。库存数字不代表方法、条件或完整轨迹验收比例。

### 最终门禁

执行 `python3 tools/gen/gen_dispatch.py`、`python3 tools/gen/gen_state_fields.py`，然后 Release CMake 与 .NET Oracle build，均退出 0。最终 source 的 `scripts/check.sh` 日志 `/tmp/mcc-env-run-query-full-check-final.log`：

```text
100% tests passed, 0 tests failed out of 11
smoke: death=94 unported=6
smoke: death=100
smoke: boss=3 death=97
L2 newrun: 200 runs, 0 mismatched
L3 combat: 40/40 combats identical, 0 unported, 0 diverged, 784/784 states matched
L3 combat: 40/40 combats identical, 0 unported, 0 diverged, 827/827 states matched
L4 run: 40 runs: 40 identical, 0 stopped at unported content, 0 diverged; 1958/1958 decisions matched
L4 run: 40 runs: 40 identical, 0 stopped at unported content, 0 diverged; 2131/2131 decisions matched
all checks passed
```

`git diff --check` 与 state-field schema check 通过。共享库有 29 个普通 sts2 导出；活动 Env/engine/CMake 中搜索 `CombatPrediction|Beam|prediction_terminal|sts2_combat_prediction` 无命中。没有恢复旧移植。

## 尚未证明的内容

- 完整三幕、最终胜利、所有角色/事件/先古/修饰器、真实档案与所有容量/寿命边界仍未完成。
- bundle、NewLeaf 变牌、Cauldron 嵌套选择及执行 Reroll 在本批只有合同夹具；原版候选的有限对拍没有覆盖这些动作结果。更多 alternatives Hook 尚缺。
- trace 仍不完整；投影只能描述源/后继状态。更细的生成/变牌/移除因果与跨分支实例身份、一般停止边界、悬停操作上下文还需补齐。
- snapshot v3 绑定当前 native runtime；独立 C# SDK、真实游戏捕获与跨进程协议还需实现。
- Oracle 仍有 TestMode/headless 偏离，完整 TestMode Off / 真机验证没有由这批解决；默认门禁仍允许 Unported，虽然此次 L3/L4 恰为零，smoke 仍有 6 条缺口。
- RF 全功能矩阵、1 万条零缺口轨迹与性能门槛没有完成。完整目标保持在[通用 Env 完成路线](../plans/2026-09-27-generic-env-completion.md)，不因本批通过而缩小。
