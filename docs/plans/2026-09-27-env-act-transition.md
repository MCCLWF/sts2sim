# 通用 Env：正常跨幕与最终胜利流程

日期：2026-09-27。分支 `feat/generic-env-rf-api`；起点 `8a93c1f`。属于通用 Env E3 主流程，E0/E1/E2 随每条路径验收；CombatSolver 保持冻结。

## 目标与真值

接通原版单人普通跨幕、最后幕终点及相关恢复/结果合同，使公共 Env 能执行原版主流程。最终胜利不得被最后一个 Boss 死亡或空奖励替代。保留完整 Env/RF 范围，不因某条跨幕回归通过而标记整体完成。

真值 v0.111.0：`Runs/RunManager.cs` EnterAct/EnterNextAct/WinRun/ExitCurrentRooms、`Multiplayer/Game/ActChangeSynchronizer.cs`、`Nodes/Screens/NRewardsScreen.cs`、`Models/Events/TheArchitect.cs`、`Rooms/EventRoom.cs` 和正式 oracle。

## 工作

1. 补普通 EnterAct：退出旧房间、SetActInternal、MapRoom 或首次 Neow、AfterActEntered；保持原版调用及 RNG 顺序，跨幕不重新生成已预选 RoomSet。
2. Boss 奖励 Proceed 按原版处理：双 Boss 首场回本幕地图，其余 Boss 经单人 ready 的 ActFloor++ 后进入下一幕。普通房间奖励继续地图流程。
3. 最后一幕进入真实 TheArchitect 事件，核对档案胜场、事件 RNG、事件布局/对话及 WinRun 的结算顺序。缺失行为继续明确报告，不伪造 victory；这项仍须完成。
4. 扩展 oracle 的正常 Boss 后推进和明确停止边界，保留默认 Boss 前记录；真实终点必须区分 before boss、after act、死亡、胜利及执行上限。
5. 通过公共 Preview/查询/快照恢复检查跨幕后继、根隔离和失败合同；正版差分包含幕/层数、地图、房间、库存与 RNG。
6. 遇到实际后继中的先古/事件/其他内容缺口，按原版补齐；另行计划的独立简单内容批次由 GPT-6 Luna xhigh 处理。
7. 正版长 Boss 战实际达到 497 张战斗牌、单堆 421 张，原 160 临时列表/牌堆与 319 卡槽不足。把临时列表容量与卡池上界对齐，卡池扩至 1023 可用槽、单堆 512、怪物历史 1024；不删除原版生成牌或历史，不截短轨迹。保持 POD，重新生成 schema 并报告状态尺寸/性能影响；容量仍有限，溢出继续显式失败，后续公共 API 需完善有类型容量结果。

## 分工与文件

- 主代理：RunManager/RunFlow、主流程集成与原版验证、快照/查询、统一生成/构建和报告。
- GPT-6 Luna xhigh：OracleRun 及 L4/公共查询参数与终点解析，不修改 C++，不运行生成器/构建或提交。
- 所有共享生成/构建由主代理顺序执行。

涉及 `game/Runs/RunManager.*`、`game/Runs/RunFlow.*`、事件/状态的必要依赖、`tools/Sts2Oracle/OracleRun.cs`、`tests/l4/l4_run.py`、`tools/check_run_queries.py`、合同检查及对应文档。

## 验收

- 实际原版第一幕 Boss 胜利 → 奖励 → Proceed → 第二幕地图的动作和状态/RNG 零分歧；随后扩展第二幕/第三幕和最终胜利的真实路径。
- 双 Boss 首场不得跨幕，最终房间必须执行原版终点；AfterActEntered、旧房间清理和事件父房间恢复有具体证据。
- 公共 owned 预览/恢复后继与普通执行相同，错误不改变源；新增 state 字段保持 POD 并纳入 schema/hash/快照。
- 回归 `scripts/check.sh`，报告实际终点、Unported 和 TestMode/表现层边界。当前默认门禁允许 Unported，不称其为完整验收。
- 报告逐项标明已实现/有限验证/实际失败/待完成；最终胜利或原版内容尚缺时计划与完整目标保持进行中。
