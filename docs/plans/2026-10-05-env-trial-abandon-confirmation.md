# Trial.DoubleDown 放弃确认移植

- 日期：2026-10-05；分支：`fix/trial-abandon-confirmation`；初始基线：main `2a25f3e`；并发任务协调后接入独立 Entropic Brew 提交 `03673ec`，Trial 单独提交。
- 所属：M4 事件内容与 NoSL E01/E02、F07/F10；既有事件、Choice 前缀重放、战斗外死亡结算和 oracle scripted replay 已具备。
- 已读：AGENTS、总体设计、路线图、Silent A10 需求、pitfalls 与 deviations。
- 原故障：`artifacts/nosl-q4g/kf1/failures.jsonl` 四条 Silent A10 / actor-filter 1 正常资源轨迹，act index 2、37–44 层。

## 目标与源事实

按 v0.111.0 `Models/Events/Trial.cs` 的 Reject → DoubleDown，表示
`Nodes/CommonUi/NAbandonRunConfirmPopup.cs` 的确认/取消。DoubleDown 本身不结束事件、
不抽玩法 RNG；取消只关闭弹窗，保留 Reject 页。确认调用 `RunManager.Abandon()`，
单人 `AbandonInternal` 标记 IsAbandoned 后 `GuaranteeKillAllPlayers` 对玩家执行
`CreatureCmd.Kill(force: true)`，最终为失败终局。检查死亡 Hook 和强制死亡语义。

不添加 UI、协程或手写状态机；用现有 `ChoiceRequest(Option)` / `Sim::Choose` /
`NeedChoice` 静止点前缀重放。确认按钮顺序与本地化 key 按原版。
任何刻意省略（表现层、存档/统计结算或 oracle 的 UI 输入适配）登记 deviations，
oracle 保留原生事件方法和 Abandon/Kill 逻辑，不以模拟器结果编造期望值。

## 执行步骤

1. 追踪 Trial、确认按钮、Abandon、Kill、EventSynchronizer；确认 Env 描述和恢复合同。
2. 实现确认请求与单人 Abandon；替换旧 Unported 测试。
3. 在 Sts2Oracle 提供保留原生行为的无节点输入适配，增加差分用例：Reject、DoubleDown、
   取消、再次打开、确认，以及取消后 Accept；比较事件页、选择描述、资源、RNG 与终局。
   覆盖 Env 克隆/快照恢复，重放四条原始 action-row 历史并分别取消/确认。
4. 更新 pitfalls（如发现新坑）、deviations、README/路线图；记录命令与原始结果于报告。
5. 完整门禁通过后，显式路径提交并合入 main；不推送。

## 涉及文件

- `game/Models/Events/Trial.h`、`game/Runs/RunManager.{h,cpp}`；必要的现有 choice 描述枚举。
- `tools/Sts2Oracle/`、`tools/Sts2Headless/Headless.cs`（仅确有必要的无 UI 适配）。
- `tests/test_trial_event.cpp`、新增 oracle 差分脚本/夹具、`scripts/check.sh`。
- 本计划、执行报告、`docs/audits/deviations.md`、`docs/audits/pitfalls.md`、生成的 `docs/audits/map-source-inventory.json`、`docs/roadmap.md`、`README.md`。

## 验收与资源限制

- oracle 差分必须实际执行原生 DoubleDown、取消/确认；TestMode 关闭，不放宽比较。
- 四条故障均进入 Choice 而非 Unported，取消可继续，确认合法结束；逐点 oracle 证据与
  Env 故障回放证据分别记录，不能混称全部原版验证。
- Python 统一经 `.venv`，C++ 最多两个构建 job，.NET 单节点；一次一个重任务。
  当前 Q4-k 训练仍运行；用户后续允许短时构建重叠、不长期高负荷，初次构建采用三个 job，完整门禁两个 job、对拍串行。
- 合入门槛：`PATH="$PWD/.venv/bin:$PATH" STS2_BUILD_JOBS=2 STS2_SRC='/home/wst/Desktop/MCC/Slay the Spire 2/src/Core' taskset -c 0-5 nice -n 10 scripts/check.sh`。
- 保留当前 `CLAUDE.md` 未提交删除；只按显式文件路径 stage，禁止 commit -a/restore；不 push。
