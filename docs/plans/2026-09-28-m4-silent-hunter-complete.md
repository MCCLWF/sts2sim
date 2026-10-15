# 静默猎手全量内容移植计划

日期：2026-09-28。分支：`feat/silent-hunter-v0110`。基线：`add6186`。

## 目标

以 STS2 v0.111.0 反编译源码和 `Sts2Oracle` 为真值，把单人通用 Env 可达的 Silent 内容和行为逐项移植完整。范围包含 `SILENT_CARD_POOL` 的全部 91 张卡、这些卡可生成的卡与其专属 Power/机制、`SILENT_RELIC_POOL` 的全部 8 件遗物、`SILENT_POTION_POOL` 的全部 3 瓶药水、三个池子的解锁/过滤条件，以及这些模型的所有 override 和源内条件分支。

生成物不能以静态头文件或 `IsUsable` 计为完成。Outmaneuver 等其他池卡只有在 Silent 专属卡或遗物可生成它时，才作为生成卡的依赖纳入；无关事件卡不计入 Silent 卡池。

## 约束

- 每项先登记原版源路径、override、条件/分支位置，再标实现状态和验证证据。
- 修改仅在独立 worktree/分支中；不合并。
- 不改 `CombatSolver`，不在 Env 放搜索策略。
- 共享 Engine/Cmd/State/Env 文件先向 `env开发` 主任务提合同与文件清单，协调后再动。
- POD 状态、RNG、Hook 顺序和 `NeedChoice` 前缀重放遵照仓库约定。对原版的有意偏离需登记。

## 工作批次

1. **源码分母与分支清单**：从 `data/content.json` 的三个池模型和原版 C# 源码导出全部池成员、生成模型闭包、每个模型的 override 清单、源码条件分支和行号；与 C++ 具体模型/注册/Unported 状态比较。清单工具可以重跑，提交生成结果。
2. **机制与接口盘点**：按源调用图标记共享现有行为、Silent 专属 Power、缺失命令/状态/Hook。共享缺口先提案协调，之后按小批实现。
3. **实现缺口**：按模型和机制批次补充完整行为，不以 `IsUsable`、头文件存在或单种子首个差异收尾。升级、目标选择、空集合、持有者/目标状态及 RNG 分支按原版处理。
4. **原版验证**：使用 `Sts2Oracle` 的自定义牌组/遗物/药水场景，覆盖代表性 Poison、Shiv 生成与消耗、弃牌/保留、敌我 Hook、药水、遗物以及 `NeedChoice` 选择恢复；记录命令输出和原始 trace 路径。把这些检查纳入本次工作可重放的验证脚本。
5. **收口**：运行 `scripts/check.sh`；更新 README、roadmap、清单和测试报告；报告逐项已完成、未完成和阻塞项；提交本分支且不 merge。

## 完成标准

- 原版源码清单中所有范围内模型/override/条件分支都有已实现和验证状态，不留未处理项。
- 生成卡及其行为依赖均闭包覆盖；共享接口已经协调。
- Silent 定向 Oracle 逐状态/逐选择恢复验证通过，且完整 `scripts/check.sh` 通过。
- 报告包含执行命令、原始结果、限制；Git 工作区干净且仅有独立分支提交。

## 文件范围（初始）

`game/Models/{Cards,Powers,Relics,Potions}`, 按协调结论可能涉及的通用 Hook/Cmd/状态文件，相关测试与 `tools/coverage/` 验收脚本，`docs/audits/` 清单，`docs/plans/` 本计划，`docs/reports/` 报告，以及 README/roadmap。
