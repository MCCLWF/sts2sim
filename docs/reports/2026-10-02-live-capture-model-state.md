# 正式战斗 unmapped 修复与开发目录整理

计划：[正式战斗 Env 捕获修复](../plans/2026-10-02-live-capture-model-state.md)。平台 Linux，正版游戏 v0.111.0。接手时 Env 主工作树已迁回 `MCC/sts2sim`，在 `fix/live-capture-model-state`；Mod 已有 `da3f2b8e` 字段桥接提交及一批尚未验证的后续修改。本轮保留这些工作并完成验证、目录整理与本地部署。

## 首因与修复

旧安装版对遗物子类的任何私有字段直接报 `live_relic_subclass_fields_unmapped`。FishingRod、SilverCrucible、WingedBoots 等原版遗物有正常的持久计数，所以正式对局会拒绝捕获；先前测试选择安全的 Neow 遗物，未覆盖这些计数。

统一映射由 `tools/LiveFieldInventory` 反射正版程序集生成完整实例字段清单，再由 `tools/gen/live_state_map_src.py` 明确指定 C++ 槽位、动态变量、对象引用及纯表现字段。运行时按实际布局逐字段核对，所有组件共用本次捕获的身份表，包含离开牌堆/活动阵容但仍被状态引用的卡牌、生物、Power 和奖励。未认识的字段、非静止点和第三方模型继续显式拒绝。

后续修改补齐事件战斗房间栈、排队的额外战斗奖励、持久引用以及历史派生槽位。原生修正包含遗物显示状态、部分 Power 的 null applier、Ceremonial Beast 必须执行的眩晕行动、Sovereign Blade 初始值和等于表基线的动态变量表示。它们经状态差分及完整原版回放门禁验证；没有改变搜索评分或预算来掩盖错误。

## 本轮直接证据

产物位于 `artifacts/live-capture-fix/`，原始日志、JSON 汇总和 Godot 请求/结果均保留。

先构建 `dotnet build tools/CombatSolverModChecks/CombatSolverModChecks.csproj -c Release`。字段清单检查 `python3 tools/gen/check_live_state_map.py --coverage` 输出：`291/295 declared fields of vanilla models classified; 4 models left unclassified on purpose`，退出 0。四个剩余模型明确为仅多人卡牌/Power，不属于单人范围。

差分命令均为 `python3 tests/live/live_capture_diff.py`，固定 `--timeout 120`、游戏与 Env 同 seed/同动作；结果如下：

| 输入 | 命令关键参数 | 捕获且比较一致 | 限制 |
|---|---|---:|---|
| 五角色基础 | `--seeds CAPTURE01,CAPTURE02 --characters IRONCLAD,SILENT,DEFECT,REGENT,NECROBINDER` | 188 | 初始牌组 |
| 15 种有状态遗物 | 上述两 seed；`--matrix relics --only FISHING_ROD,SILVER_CRUCIBLE,WINGED_BOOTS,ART_OF_WAR,BEATING_REMNANT,KUNAI,SHURIKEN,ORNAMENTAL_FAN,PEN_NIB,POCKETWATCH,PAELS_LEGION,UNSETTLING_LAMP,IRON_CLUB,POLLINOUS_CORE,SWORD_OF_JADE` | 505 | 原版获取及跨回合 |
| 全遗物扫描 | `--seeds CAPTURE01 --matrix relics --allow-setup-failures` | 4,208 | 269 组已建局；38 个获取/选择流程未能建局，不计为通过；4,216 次观察中 8 次未比较 |
| 五角色第一幕 | `--runs --seeds CAPTURE01 --characters ... --hp 1000 --stop after-act1` | 1,409 | 一 seed；记录未运输的历史房间数据差异 |
| 遭遇扫描 | `--seeds CAPTURE01 --matrix encounters --allow-setup-failures` | 1,819 | 88 组；Deprecated 与 Architect event 两夹具未建局 |
| 卡牌扫描 | `--seeds CAPTURE01 --matrix cards --allow-setup-failures` | 10,463 | 557 组；小牌组及充足起始能量，非任意组合证明 |
| 五角色整局 | `--runs --seeds CAPTURE02 --characters ... --hp 1000 --stop victory` | 3,076 | 实际每条 trace 的完整决策均消费；高 HP 不是正常难度胜率 |
| 药水扫描 | `--seeds CAPTURE01 --matrix potions --allow-setup-failures` | 1,095 | 65 组，无建局排除 |

合计 **22,763 个捕获比较点，0 捕获拒绝、0 意外字段差异**。比较的是按稳定身份归一化的可达战斗状态，区分进程局部句柄、惰性显示缓存及明确不会在战斗结果边界前读取的旧房间/事件数据；不声称每个完整快照的每个字节都相同。比较器还检查 oracle/replay 退出码和完整决策数量，结果 JSON 保留建局排除项。

真实 Godot 无头 `ENV-COMPARISON`，Ironclad/Cultists、`LIVEROOT003`，显式注入计数遗物：

```text
EnvComparison:RelicCounter:FISHING_ROD:2,0,0,0
EnvComparison:RelicCounter:SILVER_CRUCIBLE:2,1,0,0
EnvComparison:RelicCounter:WINGED_BOOTS:1,0,0,0
EnvComparison:SharedBudget:Serial:LiveUnchanged:Native=1111.5ms:Env=1098.0ms:HP=79/80
EnvComparison:BudgetPersistence:LocaleRoundTrip:Cancellation:NoEnvDeployment
```

结果 `Passed`；中英文往返通过；实例已由启动器输出 `UNATTENDED_INSTANCE_REMOVED` 并删除。测试只有比较，没有执行 Env 路线。

`scripts/check.sh` 退出 0，**59/59 CTest**、L2/L3/L4 及固定整局恢复回归全部通过；`python3 tools/combatsolver/check_policy.py` 为 **34,082 例、0 mismatch**；Mod Release 构建 **0 warning/0 error**，`tools/verify-refactor-boundaries.sh` 退出 0。原生库和 Mod 均在当前仓库构建。

## 目录与空间

`MCC/.local` 已迁入本仓库忽略的 `artifacts/legacy`；旧源码副本确认干净后以五份 Git bundle 保存全部 refs，再删除重复 checkout。4,029 份大日志无损 gzip，逻辑字节减少 10,783,395,880；停放 API 工作树的历史产物再压缩 88 份，减少 163,237,815 逻辑字节。逻辑字节统计可能包含原先硬链接，实际磁盘空间以 `df` 为准。

原仓库 `.local`、停放 API 工作树 `.local` 和 Mod 的旧 scratch 也迁入 `artifacts/legacy`。两份旧游戏缓存的可重建 executable/PCK/程序集删除，保留配置、Mod 和证据。当前源码在仓库及 `external/CombatSolver`，构建在 `build`/标准 .NET 输出目录；工具默认输出改为 `artifacts`。历史报告路径已更新，原始 JSON 的输入来源文字保持原样。

## 交付边界

本次修复覆盖原版单人、受支持游戏版本、全解锁档案、稳定玩家出牌边界；部分档案、第三方游戏规则和多人仍有明确拒绝。尚未建局的遗物夹具不冒充验收。当前 Linux 本地 DLL/原生库部署到已确认游戏 `mods/CombatSolver`，旧 ZIP 属于历史构建，不代表此次修复；不上传渠道、不改变既有原版自动部署路径。
