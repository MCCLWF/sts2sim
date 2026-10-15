# Trial.DoubleDown 放弃确认

- 日期：2026-10-05；计划：[本批计划](../plans/2026-10-05-env-trial-abandon-confirmation.md)。
- 分支：`fix/trial-abandon-confirmation`，初始基线 main `2a25f3e`；协调后接入另一任务的
  独立 Entropic Brew 提交 `03673ec`，Trial 另行提交。v0.111.0 单人全解锁老档案。
- 前置实际状态：已有 Choice/NeedChoice 前缀、克隆/快照、战斗外死亡终局、公开 run queries；
  Trial 判决可用，但 DoubleDown 会报 Unported。四条原始故障未改写。
- 当前状态：Trial oracle 专项、四条历史回归及完整合入门禁全部通过；在本任务提交后
  将已验收的组合分支快进合入 main，不推送。

## 源码与实现

`Trial.Reject` 原版 EventOption 参数顺序是 `(disableOnChosen:false,isProceed:true)`；
之前漏了这两个标记。`NEventRoom.OptionButtonClicked` 的 IsProceed 分支直接执行 Chosen，
不会经过同步器投票/page increment，也不等于回地图。已补标记、直接回调路径及离页判断。

DoubleDown 仅打开 NAbandonRunConfirmPopup。Env 使用 `ChoiceKind::Option`，source 为
`NAbandonRunConfirmPopup`，prompt 为 `ABANDON_RUN_CONFIRMATION.body`；选项 0 为
`GENERIC_POPUP.confirm`，1 为 `GENERIC_POPUP.cancel`。既有 Env 静止点快照 + prefix replay
处理暂停，未新增 UI、协程或手写状态机。取消不改页、不抽玩法 RNG，可以重新打开或 Accept。
确认执行 `RunManager::Abandon`：先 `is_abandoned=true`，再 `CreatureCmd::Kill(force=true)`，
保留死亡 Hook，最后由既有失败终局路径收尾。新字段进入状态 Hash/本地及 portable 快照
和 run queries；同构建快照 fingerprint 已更新，旧构建快照按现有协议拒绝。

有意把独立 modal 输入串成嵌套 Choice，见 D-013；oracle 的纯节点/音乐/结算界面适配见 O-018。
原生 DoubleDown、OnYes/OnNo、AbandonInternal、GuaranteeKillAllPlayers、Kill 与 OnEnded 仍执行。
oracle prompt/按钮 key 从正版 `_Ready` IL 读取，未以模拟器结果编造真值。

## 验收

### TestMode=false oracle 差分

命令（exit 0）：

```bash
taskset -c 0-5 nice -n 10 .venv/bin/python tests/api/trial_confirmation.py \
  --out artifacts/trial-confirmation-validation-2 \
  --failures artifacts/nosl-q4g/kf1/failures.jsonl
```

两种 seed × 三条操作路径 × 有/无 Lizard Tail，第三幕事件根为显式注入。
路径是确认；两次取消、第三次确认；取消后 Accept。每个动作同时验证直接推进、Clone
后继、JSON 快照恢复后继及原状态未变，终点再恢复。比事件描述/选项标记/page index、
确认按钮 key、HP/金币/牌组/遗物/药水、全部有种子 RNG、IsAbandoned 与胜负结束。
额外要求原版 OnEnded 只在真实终局调用一次。

原始结果：`{"cases":12,"transitions":56,"clone_restore":true,"oracle_test_mode":false}`。
12/12 用例、56/56 转换全部严格一致；包含 12 个初始根的 68 个状态点，全部有种子 RNG
状态/计数一起比较。每次转换分别验证 Clone 和恢复后继与原版一致，再与直接推进的
快照逐字段相等；12 个终点再恢复一致。原始 oracle、stderr、逐用例 native 输出与 summary
在 `artifacts/trial-confirmation-validation-2/`，命令 stdout 在同名 `.log`。

首次组合运行的 oracle 专项也通过；随后在同一 Python 进程同时加载独立 Env/sampler DSO，
触发已有静态 callback registry 的符号互相覆盖，历史回放报 `unknown registered callback`。
测试驱动改为在独立子进程加载 sampler（符合训练实际运行方式），第二次组合全通过；
未修改引擎回调、历史动作或 oracle 期望。首次输出保留于
`artifacts/trial-confirmation-validation/` 和同名 `.log`。

### 四条 NoSL 原始失败

Silent A10、actor filter 1，按保存的 action_rows 原样重放，确认和取消各一次。
此项是正常资源、从新局出发的 Env 回归，不冒充完整历史的正版差分。
四条均准确复现保存的幕/层、HP/最大 HP、金币、牌组大小和遗物数，抵达 2 选项 Choice，
未再 Unported。确认四条均真实死亡，取消四条均继续运行，资源保持不变。

| seed | act / floor | 确认：status / 实际动作步数 | 取消：status / 实际动作步数 |
|---|---|---|---|
| NT100652010001317 | 2 / 37 | death / 533 | running / 534 |
| NT100652030003071 | 2 / 37 | death / 693 | running / 694 |
| NT100652030005423 | 2 / 44 | death / 588 | running / 589 |
| NT100652050007826 | 2 / 38 | death / 524 | running / 525 |

取消后的 sampler 继续使用 actor filter 1 / auto-forced：对返回相同观察的动作会按既有
策略过滤规则剪掉，再自动选择剩下的 Accept，所以比确认分支多一个动作。
直接 Env 的取消确实保留 Reject 页、可重复打开，已由原版专项验证。
原始 summary 中 before/after 与每个分支的步数全部保留。

### 完整合入门禁

```bash
PATH="$PWD/.venv/bin:$PATH" STS2_BUILD_JOBS=2 \
STS2_SRC='/home/wst/Desktop/MCC/Slay the Spire 2/src/Core' \
taskset -c 0-5 nice -n 10 scripts/check.sh
```

首次 gate 的 60/60 CTest、内容覆盖、三个 smoke 通过，随后生成的源码库存过期而退出：
Trial 已移除 Unported，但库存仍记为 true。用 `.venv/bin/python tools/coverage/map_source_inventory.py
--write docs/audits/map-source-inventory.json` 生成，diff 仅 `Trial.cpp_has_unported_marker:true→false`；
其 manual review 队列标记仍保留，不升级覆盖资格。第一次原始日志
`artifacts/trial-check-20261005.log` 保留，第二次完整日志在
`artifacts/trial-check-20261005-2.log`，退出码 0，末行 `all checks passed`。

第二次门禁的原始汇总：

- C++ 构建通过，CTest 60/60（26.54s）；非角色专属遗物 259/259 可用，1172 个源码 override。
- 三个 smoke 分别为 `death=100`、`death=100`、`boss=3 death=97`，无 error/hang。
- .NET oracle 构建 0 warning / 0 error；L2 200 局，0 mismatched。
- Trial TestMode=false 专项 12 用例、56 转换，Clone/恢复通过。
- L3 普通战斗 80 场、1611 个状态点；新增遗物 1 场、14 个状态点；
  Untouchable 2 个 bounded probe、4 个状态点，全部一致。
- L4 各组共 111 局、25143 次决策全部一致，0 Unported / 0 other failures；
  包含两幕延续、最终胜利、每决策快照恢复及既有专项回归。

check.sh 默认 C++ 两个 job、oracle 单 MSBuild 节点；
Python 通过 .venv PATH 运行。用户后续允许短时构建与既有 Q4-k 训练重叠，初次构建用三个 job，
对拍串行、限定 CPU 0–5 和 nice 10，没有启动训练或推送。

## 工作树与限制

接手时只有 CLAUDE.md 的未提交删除，保留未 stage。并发的另一项 NoSL 挂起修复随后修改
Entropic Brew、诊断工具及部分共享文档/CMake；其内容保留，提交只纳入本任务路径/修改块。
用户明确授权跨会话协调；另一任务先提交自己的修改 `03673ec`，Trial 分支快进到它后继续。
共享文件当前只剩 Trial 修改，最终合并使用通过完整门禁的组合状态；两项源码修复分开提交。

本批未接入实际游戏弹窗控制、未运行新训练、未证明所有第三幕事件/整局 NoSL 质量。
未重新加载既有训练 worker，其已加载旧库不宣称具有本批修复。
