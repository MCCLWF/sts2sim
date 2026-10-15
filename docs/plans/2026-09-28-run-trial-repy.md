# Trial 与 War Historian run 闭包

## 目标

对照 STS2 v0.111.0 源码补齐 Glory 的 Trial 事件，以及 Lantern Key 在 War Historian Repy 事件中的消耗、双奖励和 History Course 遗物效果。通用 Env 只存原版状态与选择事实；所有状态保持 POD。

## 任务

1. 完整核对 Trial、WarHistorianRepy、HistoryCourse、LanternKey、CardPlayFinished 历史规则及奖励/选择顺序。
2. 将 Trial 的可模拟选择与奖励接入事件派发；明确弃局确认 UI 在无头 Env 中的动作语义。
3. 将 War Historian 的钥匙完成、遗物/奖励、第二把钥匙路径和 FreedRepy 写入 Env；为 History Course 保存最小原版战斗历史事实并实现回合开始自动重打。
4. 增加能指出具体行为回归的定向测试，重新生成分发和源码清单，运行完整门禁与可用的正版差分。

## 验收

- Trial 的普通判决路径、War Historian 的单/双钥匙路径与 History Course 前一回合攻击复打有源码对应的定向验证；未表达的 UI 分支明确报告。
- `scripts/check.sh`、地图清单检查和 `git diff --check` 通过；报告区分定向 C++ 测试和直接 oracle 证据。
