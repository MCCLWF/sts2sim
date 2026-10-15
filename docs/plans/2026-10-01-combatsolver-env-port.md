# CombatSolver 0.47.3 → Env 移植

## 用户目标与约束

2026-10-01 用户明确重新启动 CombatSolver 移植。源项目为
`Torch1230/CombatSolver`，重新克隆得到 `d7b46203593beb51f792628a2587b04f0850d1c2`
（0.47.3）。`iRyougi/sts2-ai` 重新克隆得到
`<私有提交>`，只用于对照，不复制其搜索实现。

保留全部已有 Env API，不修改 `game/`、`engine/` 的游戏行为。搜索评分、状态投影、
历史账本、风险、预算与协调器属于 `search/combatsolver`。旧归档保留，不能恢复其
专用终局旁路。使用公共 Action operation 的 `safe_outcome` 停点，Resume 仍执行原版结算。

分支：`feat/combatsolver-env-2026-10-01`，基线 `5bd4da1`。
用户后续澄清：不必完全一致，效果差不多即可。因此搜索层不再要求逐项复刻全部上游
保路、调度或实验机制；仍需可运行的跨回合战斗规划、选择处理、合理评分与用药政策，
并通过实战效果对照验收，不能以可编译骨架冒充完成。

以同起点、同信息、同预算下的胜率、战损、死救消耗、用药与耗时作为最终证据。
先核对根状态可比性，再逐场报告差值；没有匹配根或完整结果时不宣称效果接近。
实现方法可以不同，质量退化须定位后处理；Env 与游戏规则的原约束不放宽。

## 职责与逐项验收

| 阶段 | 上游职责 | 目标与验收 | 状态 |
| --- | --- | --- | --- |
| P0 | 来源、许可、政策值 | 固定最新源；MIT 通知；直接链接上游 C# 的差分工具 | 完成；34082例零差异 |
| P1 | 根捕获、动作与选择续用 | 独立拥有 Env；纯分支；安全胜负停点；重放/结算一致；未知路径明确失败 | 核心合同完成；legacy无停点Choice明确拒绝 |
| P2 | StateEvaluation、Threat、StateFingerprint、历史 | 实际EndTurn威胁、简化HP/易伤/集中击杀/持续buff/杂质评分；原生Hash正确性转置与独立近似多样性桶 | 实用首版；完整上游设置/成长估值不包含 |
| P3 | Expansion、Choices、Replay、Opening、Candidates | 牌、目标、药水、嵌套选择、预算与原序；完整选中路线重放核对 | 核心展开完成；有明确分支/动作上限 |
| P4 | Retention、Pareto、Transpositions、Cycle/OrderedMutation | 六维转置标签；多样性/药水/目标与进攻保路；重复牌动作近似去重 | 实用保路；不复刻专用循环/有序变异调度 |
| P5 | Phases、Terminal、FinalPlanOrdering | 跨回合搜索与停止；完整/部分路线；胜负、HP、死救与用药排序 | 核心排序完成；盗取/成长目标与死救HP精细账本未包含 |
| P6 | Coordinator、Continuation、Audits、Portfolio | 共享请求预算；集中击杀/宽度组合；真实无药反事实 | 实用组合完成；不包含并行协调器/跨请求缓存 |
| P7 | 客户端入口与性能 | 独立搜索库/工具；保留 Env ABI；固定根质量与性能对照；全门禁 | 完成；21/21胜利、总战损349对350；完整57CTest/L2/L3/L4门禁通过 |

上表保留上游职责地图，按最新用户要求用不同实现覆盖实用首版功能，不要求逐文件完备。
每批记录采用/翻译的上游文件及范围。纯值政策对拍只能证明计算/排序一致，不能代替
Env 字段映射或整搜质量验收。实战效果验证完成前不得报告“已完成”。

## 本批实现边界

- `SolverWeights` 常量、`RouteQualityPolicy`、`SolverInterimResultOrdering`、
  `ActEndingBossPolicy` 的纯值部分、`PotionUsePolicy` 的纯值部分、
  `CombatBeamSolver.Transpositions` 及预算窗口。
- 上游游戏对象有关的登记、根字段捕获和策略设置转换另行验收，不能用零值占位。
- Env 动作适配只认已证明的公共停点；未带 safe-outcome 政策的历史 choice 根需另行
  转换/重放支持，不悄悄把战后回血当作战中终局 HP。
- `ContextualRanking` 等默认关闭的实验、并行 lane 调度与正式默认政策分别登记，
  不因默认关闭就宣称全部移植。

## 检查与证据

1. C++ Release 构建；直接链接固定上游 C# 源码的政策/转置差分。
2. Env 根纯度、独立分支、选择前缀、SafeOutcome 与 Resume 对照普通 Env 执行。
3. 后续每项机制与上游同根、同动作验证，再比较完整搜索的战损、用药、成长与胜负。
4. 有完整搜索后再作性能测试：相同根、政策、节点/时间预算，记录实际工作量。
5. `git diff --exit-code 5bd4da1 -- game engine env sdk python` 证明约束保持；
   合入前运行 `scripts/check.sh`。

用户随后强调：当前专注 CombatSolver 移植，nosl 等其他方案暂不推进；清理较早的
`.local` 数据释放空间。清理仅删除旧批次成功对拍的大型 stdout，保留失败证据、
重放输入、种子、清单和汇总，并在 `artifacts/legacy/storage-cleanup-2026-10-01` 记录删除清单。
