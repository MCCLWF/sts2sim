# v0.111.0 四方内容集成

日期：2026-09-28。计划：[M4 四方内容集成](../plans/2026-09-28-m4-four-way-integration.md)。分支：`feat/integrate-v0110-four`。

## 输入与处理

从 `b192e25` 出发，分别合并 Regent `945832b`、Defect `d4fa001` 和 Silent `15dee0a`。非英雄遗物任务在 `feat/all-nonhero-relics` 的共享工作区留有与其它 run 级内容交织的未提交改动；只提取该任务拥有的模型、运行时、Oracle、测试和文档变更，原工作区未清理或重置。

共享代码冲突按原版单人语义整合：职业卡池的 epoch 过滤、玩家人数约束和源模型的卡牌属性同时生效；共享派发和状态遍历从合并后的源重新生成。Crossbow 的当回合免费标记经 `CardModel` 接口设置；Toy Box 的测试选择前缀包含奖励选择。Oracle 地图 JSON 与 Env 一样输出 `spoils_map_quest` 的 false 值，以比较完整字段。Defect 源清单的 `GetUnlockedPotions` 条件标记为当前全解锁 profile 的路径；该清单不宣称任意 Defect epoch 档案已通过验收。

集成后首次逐件矩阵发现 10 条失败路线，其中 7 条涉及未落地的 Curse of the Bell、Doubt、Folly、Poor Sleep 原版属性/效果，2 条在多次费用变化后超出单卡四条费用修正容量，1 条是 Punch Dagger 的 Momentum 未累积伤害。按原版源码补齐四张诅咒牌与 Momentum，并将可复制的单卡费用修正容量提高到 32；针对 Calling Bell、Neow's Bones、Punch Dagger、Fake Snecko Eye 的失败路线重新对拍通过。第二轮矩阵 257/258，剩余 Distinguished Cape 路线生成 Debt 和 Apparition；补齐 Debt、Normality、Regret、Writhe 的原版行为及 Apparition 后，定向路线 249/249 决策与逐点快照一致。最终逐件矩阵结果见下方验证表。

## 集成验证

以下命令在此集成分支运行，使用 v0.111.0 `src/Core` 与原版 DLL。结果是集成后的重新验证，不沿用各任务分支的通过状态。

| 命令 | 结果 |
|---|---|
| `scripts/check.sh`（补齐依赖后的最终重跑） | 32/32 CTest；非英雄遗物目录 259/259、1172 源覆写；L2 200/200 新局；L3 80/80 场、1611/1611 状态，新增遗物 1 场、14/14 状态；常规 L4 80/80 条、4089/4089 决策，新增遗物 188/188 决策；严格二幕、三幕胜利、定向遗物与献祭路线全通过；最终 `all checks passed` |
| `tools/coverage/silent_source_inventory.py` | 154 模型、314 源控制流点；0 未解析引用 |
| `tools/coverage/defect_source_inventory.py` | 91 卡、8 遗物、3 药水；146 模型、715 覆写、274 条件、52 次迭代、128 生成边；0 缺失模型、0 未核对行 |
| 新增非英雄遗物 L3，14 件同场 | 1/1 战斗、14/14 状态、0 Unported、0 分歧 |
| 新增非英雄遗物 L4，11 件同局，逐决策快照恢复 | 1/1 路线、188/188 决策、0 Unported、0 分歧 |
| 非英雄遗物逐件单人 L4，`SEED0002`、无 Neow、Act 1 后停止 | 258/258 可用路线通过、0 失败；`MASSIVE_SCROLL` 原版仅允许多人，单人矩阵跳过 1 件 |
| Silent、Defect、Regent 各一场开局战斗 L3 | 分别 32/32、10/10、17/17 状态一致，0 Unported、0 分歧 |
| Silent、Defect、Regent 各一条第一幕 L4，逐决策快照恢复 | 分别 331/331、189/189、289/289 决策一致，0 Unported、0 分歧 |
| Regent `SEED0002` 完整胜利 L4，逐决策快照恢复 | 790/790 决策一致，0 Unported、0 分歧 |

新增遗物 L3/L4 命令已加入 `scripts/check.sh`；补齐矩阵发现的依赖后，更新后的完整门禁已再次运行并通过。

## 剩余边界

- 非英雄遗物范围 259/259 可用；生成目录仍有 7 件英雄专属遗物标记不可用。这是整个内容库的剩余数量，不属于这次非英雄遗物任务的遗漏。
- 四个任务报告中的职业模型/源码检查与定向差分并非所有角色的随机三幕整局覆盖；通用 Env、真实档案及 RF API 仍以路线图中的未完成项为准。
- 无头 Oracle 编译有一条 `OracleRun.cs` 的 nullable 警告；构建和差分均成功。
