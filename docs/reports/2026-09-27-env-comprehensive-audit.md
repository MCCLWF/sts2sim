# 通用 Env 与 CombatSolver 移植：当前全面复核

日期：2026-09-27。分支：`feat/generic-env-rf-api`。固定原版：游戏 v0.111.0；Random Foreseer 需求基线为 v0.13.17。下文原审计基线提交：`ded23f2`。

**后续进度（同日）**：CombatSolver managed 工具已移入冻结档案。事件战斗返回父事件、Battleworn Dummy、Vakuu、Whispering Earring、Grave of the Forgotten、Reflections 和 The Architect 已接入；一条既有铁甲战士路线 1138/1138 决策到真实胜利，另四条三幕路线 4/4 完整一致、3520/3520 决策逐点快照恢复，其中一条真实胜利。完整 `scripts/check.sh` 通过 16/16 CTest 及 L2/L3/L4 门禁。下文的 814/1123、13/13 和 Battleworn 首停等数字保留原审计时点；最新命令和限制见[The Architect 胜利闭环](2026-09-27-env-architect-victory.md)与[通用 Env 完成路线](../plans/2026-09-27-generic-env-completion.md)。

## 结论

继续开发通用 C++ Env 有意义，而且应当先完成它。CombatSolver 核心没有完成替换，也不应在通用 Env/RF 公共合同完成前继续移植。当前冻结边界是正确的：原版规则、状态、随机数、Hook、选择和结果事实进入通用 Env；搜索评分、剪枝、历史投影、风险偏好和 UI 由客户端负责。

这不是已经可放入 `mods` 的最终产品。当前 Env 可以稳定恢复和预测一批真实单人战士路线，但仍会在明确的 `Unported` 边界停止；Random Foreseer 所需的类型化局外操作、完整结果追踪、真实捕获和独立 SDK 也未完成。

## 当前已经能用的通用 Env

| 层 | 当前状态 | 证据/限制 |
|---|---|---|
| 执行 | `Reset/Step/LegalActions/Clone/Hash/Predict/Observe/Determinize/VectorEnv` 已有；普通选择可保存静止根并回答前缀 | 跨房间和嵌套选择仍不全；容量/停止边界合同仍需扩展 |
| C ABI | 当前通用库导出 29 个普通 Env/快照/预览/查询接口 | `nm -D build/libsts2env_c.so` 未发现旧 `combat_prediction` 或 `solve_beam`；独立 C#/Python SDK 仍未完成 |
| 查询/预览 | 局外 run/map、选择、owned 后继、before/after、结果投影、地图标记查询已有 | `trace_complete=false`；来源实例、父子因果、全部 RNG 抽取和对象退休解释尚不完整 |
| 状态 | POD 快照 v3、字段/schema/hash/引用校验、克隆和恢复已在有限路径通过 | 真实 live 捕获、跨进程持久化和全部房间静止边界仍缺 |
| 原版差分 | 第一幕 12 条/3906 决策逐点恢复；严格二幕路线 3246 决策；Pael's Wing 219 决策；完整 quick/check 门禁通过 | 这些是固定语料和高生命值夹具，不等于全部内容或自然胜利覆盖 |

原审计时点的第三幕战士路线：`SEED0004` policy 1 共 1123 个决策，接入 Fur Coat、TinkerTime 和 Mad Science Attack/Sapping 后通过 814 个，随后在 `Events.BattlewornDummy.GenerateInitialOptions` 明确停止。该阻断现已解除；TinkerTime 的两次事件 RNG、`ChosenCardType` 和 Mad Science 实例字段已进入通用快照。该路线在当时停止前没有实际打出 Mad Science，因此 Attack/Sapping 仍需要专门出牌夹具。Skill、Power 及其他 Rider 目前明确抛出 `Unported`，没有空实现。

原审计时点的 `cmake --build`、`ctest`（13/13）和 `scripts/check.sh --quick` 已通过。静态库存只是实现文件计数，不是行为完成率：

| 类型 | 正式模型 | 具体头文件 | 静态 `IsUsable` |
|---|---:|---:|---:|
| 卡牌 | 596 | 262 | 260 |
| 能力 | 265 | 140 | 147 |
| 怪物 | 120 | 110 | 110 |
| 遗物 | 299 | 191 | 188 |
| 药水 | 65 | 55 | 56 |
| 普通事件 | 59 | 53 | 42 |
| 先古事件 | 9 | 5 | 5 |

五个角色卡池的具体头文件覆盖仍为战士 90/90、静默 32/91、机器人 31/91、储君 8/91、亡灵 10/91；跨职业和事件卡池仍有大量未完成规则。

## RF 公共 API 还差什么

现有 Env 已足以作为 RF 类客户端的执行底座，但尚不能完全替代 RF 自己复制的预测逻辑。需要继续补齐：

1. 所有单人可达卡牌、能力、遗物、药水、事件、先古、修饰器和档案条件；先贯通铁甲战士三幕、最终事件和胜利，再扩展其他角色。
2. 类型化的局外/悬停操作：变牌、遗物获取、事件奖励、商店购买和补货、重掷、休息、献祭；每个操作复用原版 Cmd/Hook/RNG，并表达完成、需要选择、截断和不支持。
3. 事件进入战斗、战后返回父事件、特殊奖励和延迟异步边界；当前 Battleworn Dummy 正好暴露这个通用缺口。
4. 结构化执行结果：生成/移除、目标、伤害、资源变化、来源实例、父子因果和稳定顺序；诊断 RNG 与隐藏事实和面向智能体的 observation 分开。
5. 独立 C#/Python SDK、能力发现、真实 live 捕获、档案/解锁规则和跨进程协议。
6. 最终验收：固定范围内至少 1 万条原版轨迹零分歧、零 `Unported`，并单独测 Clone/Step/ABI/trace 成本。

## CombatSolver 当前状态

旧移植是冻结档案，不是正在运行的 Env 后端：

- `archive/combatsolver/managed-tools/CombatSolverEnv/Runtime/EnvBackendSettings.cs` 的门禁固定拒绝原因 `live_root_and_same_policy_adapter_incomplete`；该项目现已移出活动工具目录。
- `archive/combatsolver/managed-tools/Directory.Build.targets` 拒绝构建依赖 CombatSolver 的归档项目；旧 Beam、旧 prediction ABI 和专用状态键不属于公共 Env。
- `EnvSearchBranch.RequireExecutionReady` 仍只定义未来恢复所需的完整 state key/history/policy/threat 条件；它不构成当前执行成功证明。

因此现在继续补 CombatSolver 只会把搜索特例和未完成状态键重新塞回 Env，无法降低当前主要风险。待通用 Env/RF 的内容、操作、结果、捕获和 SDK 验收完成后，是否恢复移植仍需用户另行明确要求；Env 完成不会自动解冻它。

## 推荐后续顺序

1. 继续铁甲战士三幕多种子与条件路线，补 Mad Science Attack/Sapping 独立出牌差分夹具；已解决 Battleworn Dummy 的事件战斗/父事件恢复以及 The Architect/WinRun 胜利闭环。
2. 建立内容覆盖清单与所有可达条件门禁，不以数条高生命值定向路线代替全量验收。
3. 按四条 RF 代表路径垂直补齐类型化操作、停止边界和逐笔结果：抽牌/生成、变牌、遗物获取、商店补货。
4. 稳定 ABI 后迁出独立 SDK/live 捕获，最后做大规模零分歧和性能验收。
5. 在上述完成前保持 CombatSolver 冻结。

相关路线：[通用 Env 完成路线](../plans/2026-09-27-generic-env-completion.md)、[跨幕进度](2026-09-27-env-act2-third-act-progress.md)、[公共预览 API](../env-preview-api.md)。
