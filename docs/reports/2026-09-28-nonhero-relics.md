# 非英雄专属遗物完成报告

日期：2026-09-28
源码基线：STS2 v0.111.0 (`41cef1ea`)
分支：`feat/all-nonhero-relics`
计划：[M4 非英雄遗物计划](../plans/2026-09-28-m4-nonhero-relics.md)

## 范围与完成情况

覆盖原版共享池、事件池（含先古遗物）、回退池和废弃池。五个角色专属遗物池（Ironclad、Silent、Defect、Regent、Necrobinder）仍由对应英雄工作负责。

| 项目 | 数量/结果 |
|---|---:|
| 范围遗物 | 259 件 |
| 修复前不可用、此次补齐 | 24 件 |
| 范围内生成器确认可用 | 259/259 |
| 源码覆写审核计数 | 259 个模型、1172 个覆写 |
| 英雄专属池遗物 | 40 件，范围外 |
| 在原非英雄任务分支仍不可用的遗物 | 23 件，全部属于范围外英雄专属池；四方集成后为 7 件 |

原版四个范围池共列 262 个槽位；`LASTING_CANDY`、`RAZOR_TOOTH` 和 `SPARKLING_ROUGE` 同时出现在共享池和事件池，去重后为 259 件。`tests/coverage_nonhero_relics.py` 会从 v0.111.0 池源码读取名单，核对内容导出、范围分界及生成的 `IsUsable` 表。生成器对所有源覆写逐项检查是否有模拟器实现或登记过的专用端口，因此该脚本同时阻止范围内模型漏实现。

此次补齐的 24 件为：`BLACK_STAR`、`BLESSED_ANTLER`、`BLOOD_SOAKED_ROSE`、`BRILLIANT_SCARF`、`CIRCLET`、`CROSSBOW`、`DAUGHTER_OF_THE_WIND`、`DEPRECATED_RELIC`、`DIVINE_DESTINY`、`GIRYA`、`GLITTER`、`INFUSED_CORE`、`IRON_CLUB`、`LORDS_PARASOL`、`PANDORAS_BOX`、`PHYLACTERY_UNBOUND`、`POLLINOUS_CORE`、`RING_OF_THE_DRAKE`、`ROYAL_POISON`、`SAI`、`SHOVEL`、`SPIKED_GAUNTLETS`、`VELVET_CHOKER`、`WAR_HAMMER`。

## 实现要点

- 为 22 件补上具体遗物模型；将 Girya 和 Shovel 接入既有休息点选项路径，并由生成器登记其手写覆写入口。
- 为 Blood-Soaked Rose 增加其选择流程依赖的 `ENTHRALLED` 永恒诅咒模型；Crossbow 使用 `SetToFreeThisTurn`，正确保留当回合免费标记。
- Lords Parasol 自动购买商店全部存货及删牌服务。商店购买路径现在支持忽略价格及不可取消的选牌，并继续执行原版购买钩子和库存处理。
- 无头 Oracle 在 Lords Parasol 入店自动删牌后补调原版 UI 的 `OnCardRemovalUsed` 状态回写；否则删牌已完成但无 UI 节点将让快照继续显示删牌服务可用。
- 运行地图 JSON 固定输出 `spoils_map_quest` 布尔字段，避免终点快照漏掉 false 值而产生字段级差异。

## 对拍与门禁

以下验证均使用原版 v0.111.0 oracle；计数为原样输出。

| 验证 | 结果 |
|---|---|
| `python3 tests/coverage_nonhero_relics.py` | 原分支 259/259；1172 个源码覆写；英雄池外当时仍有 23 件不可用。四方集成后重跑为 259/259，英雄池剩余 7 件不可用 |
| 非英雄遗物逐件 L4（`SEED0002`，Ironclad，无 Neow，止于 Act 1 后） | 258/258 单人适用路线逐决策一致；`MASSIVE_SCROLL` 源码限定多人，单人 Oracle/L4 不适用 |
| Massive Scroll 拾取 L4（临时启用多人卡池，止于初始遗物处理后） | 1/1 决策一致；候选卡、选牌及 RNG 终态一致 |
| `relics_common_m4` | 通过；覆盖 `MASSIVE_SCROLL` 单人 `IsAllowed == false`，并核对其候选卡筛选只返回多人卡 |
| 新增战斗遗物 L3（14 件，`SEED0002` / `CULTISTS_NORMAL`） | 1/1 战斗，14/14 状态一致，0 Unported，0 分歧 |
| 非英雄遗物路线 L4（11 件，`SEED0002`，无 Neow，止于 Act 1 后） | 1/1 路线，188/188 决策一致，逐点 snapshot 验证，0 Unported，0 分歧 |
| 无遗物路线 L4 终点快照基线 | 1/1 路线，243/243 决策一致 |
| `scripts/check.sh --quick` | 32/32 CTest；目录覆盖检查通过；三组 100 局 smoke 完成 |
| `scripts/check.sh` | 全部通过：L2 200 局；L3 80 场/1611 状态；L4 常规 80 局/4089 决策；Act 2、胜利、献祭等严格路线与新增遗物路线均零分歧 |

完整门禁中的严格续跑结果：Act 2 路线 5/5、3246/3246 决策；胜利路线 8/8、7689/7689 决策；Pael's Wing 献祭路线 219/219 决策；已有 relic 专项 4/4、948/948 决策；本次新增 L4 遗物组 188/188 决策。最终输出为 `all checks passed`。

新增遗物直接 L3/L4 命令已加入 `scripts/check.sh`。其它 235 件先前已实现的范围内遗物由源码覆写生成门禁、既有单元/定向对拍以及完整差分门禁覆盖；本次逐件 L4 矩阵为每件单人运行可获得的遗物单独执行完整路线。`MASSIVE_SCROLL` 只允许多人局，常规 L4 启动器仅创建单人局，因此不进入单人整局矩阵；另用临时双玩家卡池上下文对其拾取和多人卡选择执行独立 L4 对拍，验证候选卡、选择、牌组回写和 RNG 终态一致。模拟器回归测试同时核对其单人不可获得以及候选卡筛选仅返回多人卡。
