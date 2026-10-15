# v0.111.0 地图内容源码清单与当前审计

日期：2026-09-28。分支：`feat/generic-env-rf-api`。本清单服务于通用 Env 的战士 run 闭环；CombatSolver 不在工作范围内。

## 方法

`tools/coverage/map_source_inventory.py` 读取：

1. `data/content.json` 的 v0.111.0 模型定义与 `modeldb` 集合；
2. `../Slay the Spire 2/src/Core` 中每个 Act、Event、Ancient、Encounter、Monster 的原始 C# 文件；
3. 当前 C++ 具体头文件和 `SIM_NOT_PORTED`/`throw Unported` 标记。

每项保存源码 SHA-256、原版 override 列表、源码机制信号（事件选项、选择、RNG、战斗、奖励、卡牌、遗物、玩家状态等）、所在幕/遭遇引用和当前 C++ 文件。头文件、生成分发或 `IsUsable` 只作为定位信息，不能作为行为完成证据。

重建和一致性检查：

```bash
python3 tools/coverage/map_source_inventory.py \
  --write docs/audits/map-source-inventory.json
python3 tools/coverage/map_source_inventory.py \
  --check docs/audits/map-source-inventory.json
```

当前清单固定到游戏 commit `41cef1ea`、版本 `v0.111.0`。

## 清单规模

| 内容 | 原版模型数 | 有 C++ 文件 | 文件仍有未移植标记 |
|---|---:|---:|---:|
| Act | 5 | 0 | 0 |
| 普通事件 | 59 | 58 | 2 |
| 先古事件 | 9 | 8 | 0 |
| 遭遇 | 90 | 90 | 0 |
| 怪物 | 120 | 110 | 45 |

其中 Deprecated、测试辅助和未挂到幕集合的模型仍保留在清单中，避免把“当前路线未抽到”误认为“不属于原版范围”。完整明细在 `docs/audits/map-source-inventory.json`。

按实际地图集合的源码覆盖定位如下：

| 幕集合 | 普通事件（文件/总数） | 先古（文件/总数） | 遭遇（文件/总数） | 怪物（文件/总数） |
|---|---:|---:|---:|---:|
| Overgrowth | 13/13 | 1/1 | 22/22 | 29/29 |
| Hive | 10/10 | 3/3 | 20/20 | 26/26 |
| Glory | 7/7 | 3/3 | 18/18 | 27/27 |
| Underdocks | 10/10 | 1/1 | 20/20 | 22/22 |

Trial 已有具体 C++ 文件；普通事件唯一没有具体文件的是不挂在地图集合的 `DeprecatedEvent`。Hive 的 `TheLanternKey` 和 Glory 先古 `Tanx` 已有具体实现，但仍需真实路线差分。另有 The Architect 等终局/展示内容只通过事件或终局流程引用，清单将它们标为 scope-less 但仍审计。

共享头文件检测现能将 `MysteriousKnight` 对应到 `FlailKnight.h` 内的具体类型定义。清单的 `cpp_has_unported_marker` 按整个头文件扫描，因此共享头中的表现层标记可同时记到多个类型；它仍是人工源码审查队列，不是独立模型完成率。

## 目前实现边界

- 遭遇模型文件已全部存在，但这不表示每个遭遇的每条条件、怪物状态机和特殊奖励都已完成；怪物头文件中的大量 `GenerateAnimator`、音效等标记多为表现层，战斗逻辑必须另行按源码检查。
- Trial 的弃局确认 UI 尚未表达，Crystal Sphere 的失败重排仍有 `Unported`。Orobas 的附魔 Bash → Break 路径有 2 个原版直接探针，其它起始牌组合尚未对拍。Fake Merchant 的库存与定价通过 8 个原版事件黄金样本，十件假遗物合并通过 4 场直接战斗 oracle，Foul Potion 转战与奖励只有源码及定向 C++ 检查，尚无真实整局 oracle 路径。Symbiote 已按源码完成首批对拍，Crystal Sphere 已有原版小游戏黄金样本但真实 UI 路径尚未验收。Doll Room、Field of Man-Sized Holes、Relic Trader、Stone of All Time、Zen Weaver、The Lantern Key、Tanx、Trial 和 War Historian 等批次只有源码及定向 C++ 检查，尚无各自真实路线对拍；History Course 另有直接战斗 oracle 证据。
- 先古 Darv 已按标准 run 逻辑实现并由五条二幕路线对拍；Tanx 的遗物抽样已接入，部分 profile/dialogue 规则仍未进入通用 Env。现有先古实现不能推导其它先古已完成。
- The Architect、Grave of the Forgotten、Reflections、Pumpkin Candle/Kindle、Regal Pillow 已有定向原版对拍，但它们只是已完成批次，不代表地图内容总清单完成。

## 接下来按源码批次推进

1. 继续按事件源码逐项审查可达条件及完整行为；Fake Merchant、Trial、War Historian、Symbiote、Corrupted 和 Darv 已完成定向实现，Crystal Sphere 的小游戏状态已接入。每个事件先读完整 C#、列出 RNG/选择/奖励/状态副作用，再实现，再做 oracle 对拍。
2. 给 The Lantern Key 与 Tanx 的新增分支补真实路径证据；同时核对各幕 `AllEvents`、`AllAncients`、`AllEncounters`、`AllMonsters` 与 C++ 进入房间路径。
3. 逐项清理遭遇/怪物的逻辑标记，区分表现层 hook 与影响战斗状态的 hook；完成地图层后才扩大卡牌、能力、药水和遗物的全量源码批次。
4. 内容接近完整后，用大量不同 seed 做错误收敛；最终再开发 RF API、真实游戏 mod 捕获桥和 100+ 长对局零分叉验收。

当前 `scripts/check.sh` 仍是有限回归门禁，允许默认路线遇到 `Unported`，所以不能作为地图全闭环证明。
