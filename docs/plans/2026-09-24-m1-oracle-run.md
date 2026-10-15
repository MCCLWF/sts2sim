# 计划：oracle 整局记录 `run`（L4 的 oracle 侧）

执行者：Codex（sol）。审阅与合入：Claude。分支：`feat/oracle-run`。

## 目标

给 oracle 加一个 `run` 命令：从新局开始，用带种子的随机策略替玩家做**每一个**决策，一直走到第一幕 Boss 房间门口（或玩家死亡），每个决策点输出完整状态、可选项和所选动作。

C++ 模拟器会实现同样的决策点，按同样的动作序列重放、逐点比对（L4）。所以**本文件定义的协议就是两边的接口**：字段名、选项的顺序、动作的编码都必须严格照写。协议里没写清楚、需要你决定的地方，在报告里单列说明。

## 驱动方式

oracle 已经能打战斗（`tools/Sts2Oracle/OracleCombat.cs`）。整局驱动沿用它的结构：
- `Oracle.StartRun`；
- `SingleThreadContext` 泵；
- TestMode 打开，只用 `Headless.ProductionBranches` 恢复改变逻辑的分支；
- `--rng-ledger` 可选。

不能点 UI，所以要找到每个界面在模型层的入口：
- 地图：`RunManager.EnterMapCoord`；
- 开局：`RunManager.EnterAct(0)`，Neow 在起点；
- 战斗：`PlayCardAction` / `EndPlayerTurnAction`，同 `OracleCombat`；
- 奖励：`CombatRoom.OfferRoomEndRewards` / `RewardsCmd`。TestMode 下 `RewardsSet.Offer` 调 `RewardsSet.testSelector`，也可以直接用 `RewardsSetSynchronizer`。
- 选牌：`CardSelectCmd.UseSelector(ICardSelector)`，覆盖 `FromChooseACardScreen`、牌组选择、手牌选择等；
- 选遗物：`RelicSelectCmd` 的对应机制；
- 事件：`EventSynchronizer`（Neow 这类先古事件同样走事件）；
- 休息点：`RestSiteSynchronizer`；
- 商店：`MerchantRoom` 的库存条目购买、移除卡牌服务；
- 宝箱：`TreasureRoomRelicSynchronizer`。

先读这些类和 `AutoSlay/Handlers/`（AutoSlay 通过 UI 走同样的流程，看它就知道每个界面有哪些操作），再决定每个决策点的实现。任何为了无头运行加的补丁都放在 `tools/Sts2Headless/Headless.cs`，并写明为什么不影响游戏逻辑（AGENTS.md §4）。

## 命令

```
Sts2Oracle run --seeds S1,S2 --characters IRONCLAD --policy-seeds 1,2 [--ascension N]
               [--max-decisions 3000] [--stop before-act1-boss] [--rng-ledger]
```

每个 种子 × 角色 × 策略种子 跑一局，输出 JSON 行（批量格式同 `combat`）。

## 输出协议

```
{"event":"begin","seed":S,"character":C,"ascension":N,"policy_seed":P}
{"event":"decision","kind":K,"run":RUN,"options":OPTIONS, ...kind 相关字段}
{"event":"action","kind":K, ...动作}
...
{"event":"end","reason":R,"run":RUN}
```

- `reason` 取值：`boss`（下一步就要进第一幕 Boss 房间）、`death`、`max_decisions`、`error`。`error` 时加 `"message"`，并把栈顶信息写进 stderr。
- 动作之后下一个决策之前，游戏里发生的一切都由游戏自己执行。

### RUN（每个决策都带）

```json
{"act":0,"total_floor":3,"act_floor":2,"coord":[3,1],"room":"Monster",
 "hp":70,"max_hp":80,"gold":120,
 "deck":["STRIKE_IRONCLAD","BASH+1",...],
 "relics":[{"id":"BURNING_BLOOD"}],
 "potions":["FIRE_POTION",null,null],
 "rng":{...同 combat 的 12 条 RunRngType 计数器...},
 "player_rng":{"Rewards":0,"Shops":0,"Transformations":0},
 "odds":{"card_rarity":-0.05,"potion_reward":0.4,
         "unknown":{"Monster":0.1,"Elite":-1.0,"Treasure":0.02,"Shop":0.03}}}
```

- `coord` 是 `CurrentMapCoord`，没有时为 null。
- `room` 是 `CurrentRoom?.RoomType`，没有时为 null。
- `deck` 按 `Deck.Cards` 顺序，升级写成 `+N`，同 `combat`。
- `relics` 的 `counter` 规则同 `combat`。
- 浮点数照 `System.Text.Json` 默认输出。

### 决策种类

对每一种都要写清楚选项从哪里来、按什么顺序：**顺序必须是游戏里的自然顺序，或者写明排序键**。

| kind | 何时 | options | 动作 |
|---|---|---|---|
| `map` | 在地图上选下一个点 | `[[col,row],...]`：可去的点，按 (col,row) 升序 | `{"coord":[c,r]}` |
| `combat` | 战斗中玩家回合 | 不需要（`combat` 字段放 `OracleCombat.Dump` 的状态） | 同 `OracleCombat`：`{"type":"play","hand_index":i,"card":...,"target":id或null}` / `{"type":"end_turn"}` |
| `rewards` | 奖励界面 | 当前尚未领取的奖励，按 `RewardsSet.Rewards` 顺序：`{"type":"Gold","amount":n}`、`{"type":"Potion","id":...}`、`{"type":"Card","cards":[...]}`、`{"type":"Relic","id":...}`，其他类型照此格式补 | `{"index":i}` 领第 i 个；`{"proceed":true}` 放弃剩下的并离开 |
| `card_select` | 任何 `CardSelectCmd` 选择（卡牌奖励、锻造、移除……） | `{"cards":[...],"min":a,"max":b,"can_skip":bool,"source":"<调用它的方法名>"}` | `{"indices":[...]}`（跳过为空列表） |
| `relic_select` | `RelicSelectCmd` 或宝箱选遗物 | `{"relics":[...],"can_skip":bool}` | `{"index":i}` 或 `{"skip":true}` |
| `event` | 事件（含 Neow） | `{"event":"<id>","options":[{"key":"<选项的文本 key>","locked":bool},...]}`，按显示顺序 | `{"index":i}` |
| `rest` | 休息点 | `[{"id":"<选项 id>","enabled":bool},...]`，按显示顺序 | `{"index":i}` |
| `shop` | 商店 | 每个条目 `{"kind":"card"/"relic"/"potion"/"remove","id":...,"cost":n,"affordable":bool}`，按库存的自然顺序 | `{"index":i}` 购买 / `{"leave":true}` |

## 策略

`System.Random(policySeed)`：
- 在合法动作里均匀随机选一个；
- 战斗沿用 `OracleCombat` 的规则（85% 出一张可出的牌）；
- 事件排除锁住的选项和 `WillKillPlayer` 的选项（同 AutoSlay）；
- 不使用药水。

## 验收

1. 20 个种子 × 2 个策略种子 × A0/A10 的 IRONCLAD 整局全部以 `boss` 或 `death` 结束。没有 `error`，没有卡死。
2. 同一命令连续跑两次，输出逐字节相同。
3. `--rng-ledger` 与 `run` 一起用也能工作。
4. 报告 `docs/reports/2026-09-24-oracle-run.md`：
   - 每种决策的实现方式（调用了哪个入口、选项顺序怎么定）；
   - 新加的 Harmony 补丁及理由；
   - 协议里你做出的决定；
   - 验收命令和原始输出摘要：各种决策的出现次数、结束原因分布。

## 边界

- 只改 `tools/Sts2Oracle/`、`tools/Sts2Headless/`（需要时）和文档，不碰 C++。
- 遇到某类界面在无头模式下确实无法驱动：在报告里写清楚卡在哪里，给出你的建议，不要伪造数据。
