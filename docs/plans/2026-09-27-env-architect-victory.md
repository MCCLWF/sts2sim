# 通用 Env：The Architect 胜利闭环

日期：2026-09-27。分支：`feat/generic-env-rf-api`。阶段 E1/E3；CombatSolver 继续冻结。

## 目标

按 v0.111.0 原版接入第三幕 Boss 后 The Architect 事件的展示布局、对话选项与 `WinRun`，让另一条真实铁甲战士路线从战斗奖励走到完整胜利终点。

## 工作

- 使用 `TheArchitectEventEncounter` 预建事件的战斗布局；它不是可操作战斗。
- 用角色历史胜场数选择原版对话，明确区别于先古访问次数。选择单例对话也按原版消耗事件 RNG；对话选项逐页进入 `PROCEED`。
- `PROCEED` 经 `RunManager::WinRun` 记录胜利，调用原版强制死亡效果，把结束原因作为普通 Env 结果暴露。
- 终局状态、房间和预建布局可导出/恢复快照。无头 oracle 路线逐决策及最终 run 投影对齐。
- 加入聚焦测试、严格四路线门禁与完整 `scripts/check.sh`。

## 边界

当前 C ABI/Python 新局参数尚未暴露非零角色胜场数；这仍属于真实档案接入工作。无头 Env 忽略 The Architect 的美术演出和得分上传，保留影响规则与决策的流程。
