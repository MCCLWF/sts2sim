# Q4-w：正常规则下的重复休息选项回放

**18 条冻结轨迹全部与原版正常规则一致，共 5,779 个决策。此前只能验证 301 决策前缀的深路径，现在完整验证到第三幕第 44 层死亡。完整门禁通过。** 本项修复离线脚本和回放接口，没有训练新策略，也没有改善或替换 X1。

计划：[Q4-w](../plans/2026-10-05-nosl-q4w-rest-continuation.md)。基线 main `aeb22f41adb050166b7a3b0648a46c2b27d5eef0`；分支 `fix/nosl-q4w-rest-continuation`。实现 `60978f4`，损坏嵌套选牌用例修正 `22c3008`。原始产物在 `artifacts/nosl-q4w/`。

## 原因与修复

Q4-v 的 P3-full / `NQ4VEVAL000330` 在第二幕第 27 层持有 Miniature Tent。原版 v0.111.0 的 `MiniatureTent.ShouldDisableRemainingRestSiteOptions` 返回 false；`RestSiteSynchronizer.ChooseOption` 成功后删除已选项，保留并重新编号其余选项。`NRestSiteRoom.AfterSelectingOptionAsync` 再展示剩余选项及离开按钮。Env 已按这些规则实现，旧离线转换器却拒绝第二次选择；普通 L4 动作适配器还会在第一次选择后自动离开。

实际冻结轨迹先选择索引 0 的治疗，HP 25→55；剩余锻造变为索引 0。第二次选择位于脚本动作 301，随后动作 302 选择第 14 张牌升级。没有更换种子、重新采样或修改玩家资源。

- `nosl_script.cpp` 允许合法的后续休息选择，使用当前 Env 索引；休息中使用药水后，仅在剩余选项为空时沿用旧的地图屏绑定。
- `OracleRun.cs` 在脚本下一项仍为 rest 时继续选择。每次重新读取原版 Options/IsEnabled，并调用原版同步器。旧的单次选择及非脚本策略仍在原来的位置离开。
- L4 根据原版实际下一决策为 rest，给前一个动作附加显式 `rest_hold`。公共 CLI 的离线动作适配器仅据此省略旧的自动 Proceed，支持选择和嵌套奖励返回休息屏。未知或位置错误的标记仍拒绝。

没有改 Env 游戏规则、RNG、actor 公开输入、模型、在线推理或比较字段；`rest_hold` 不是学生动作。公开学习根的导出器也复用 L4 的动作解析。原生动态库 SHA256 仍为 `6296b12846d035ba598cb3beea10d2eaa649c241c468136cf8d51e67a099e231`。

## 冻结回放与拒绝测试

```bash
nice -n 19 taskset -c 4 .venv/bin/python -B tools/nosl/rest_continuation_check.py \
  --old artifacts/nosl-q4v --out artifacts/nosl-q4w/qualification
```

使用 Q4-v 已保存的首两个种子 × 八个 hybrid，以及两条深路径；没有重跑 6,656 次策略评价。每条原版 trace 的实际 begin 均为 TestMode=false。原 L4 对状态、合法动作、资源、RNG 和真实终点的检查保持不变。

| 检查 | 实际结果 |
|---|---|
| 冻结正常规则整局 | 18/18 identical，5,779 个决策，全部真实 death |
| 原已成功的 17 条 | script.jsonl 和 oracle.jsonl 逐字不变 |
| 修复路径原前缀 | 301 个决策对应的原版事件不变 |
| 修复后的完整深路径 | 538 个原版决策 / 560 个 Env 动作，第三幕第 44 层真实 death 一致 |
| 深路径逐决策 snapshot restore | 538/538 identical，实际 TestMode=false |
| 原 Q4-v 产物 | 424 文件 SHA/大小全部不变 |

正式冻结资格总耗时 44.733 秒，见 `qualification/summary.json`、`results.json` 及各种子的实际 trace。

三个明确的正常规则拒绝边界均取得证据：

1. 将剩余锻造索引改为 1，原版在动作 301 拒绝：`the rest site option is not enabled`。
2. 无 Miniature Tent 的普通休息之后插入第二次选择，原版在动作 106 拒绝同类失效选项。
3. 将第二次锻造的嵌套选牌改为 `[99999]`，原版在动作 302 拒绝：`the card selection is not a legal one`。

首轮工具误选了更早的奖励选牌，实际拒绝发生在动作 26，不能算第三项证据。该失败记录完整保留；随后只补跑正确的动作 302 损坏计划，证据在 `negative-rest-card-selection/`。工具已修正为在第二次休息之后定位 card_select；合并摘要为 `audited-summary.json`。原版错误可能由异步任务包装，拒绝判据核对实际错误终点及异常原因，不能只凭宿主进程退出码。

## 构建、门禁与资源

C++ 转换器/CLI、原版 oracle 构建通过。完整 `scripts/check.sh` 已在实现 `60978f4` 上通过，60 CTest、L2 的 200 个角色/难度/种子组合及脚本要求的 L3/L4、最终胜利、快照和定向休息/奖励/遗物路径均通过，日志以 `all checks passed` 结束。后续 `22c3008` 仅修正独立负例工具的定位与坑点文档；游戏及回放运行时代码未再变化，不重复完整门禁。

云实例 `pro-7912450e5767` 本轮未启动。云 GPU 平台 API 在 **2026-10-06 00:21:33 UTC** 返回 Success / shutdown。没有新训练、搜索或标签采集。官方 X1 SHA256 仍为 `6fb5f1955a11d55175d348659460ce459017baa018f7cf9242a671ba19266703`。

按用户最新要求，已再次 fetch CombatSolver 默认分支和 tags；远端最新仍为 **0.50.0 / `9a4489d840a3772aef451d5c6dc42dd79719f380`**，与当前锁定、构建及 Q4-o/Q4-s 实际使用的原始教师相同，记录在 `upstream-status.json`。该版本已在 Q4-o 的正常规则真实学生根中使用，104 条原版路线及公开标签合格；这些是历史直接证据，不声称本轮新增搜索。旧本地 Env 桥接分支不是最新版教师。

本轮冻结清单共 **147 文件、19,961,239 字节**；`manifest.json` SHA256 为 `18799ecbfc3def70cdcb917837d45572a55e11a8ca1b5bb5361e4a641a37c9fd`。Q4-v 原清单 SHA256 `de6a63127dbbfb5b6109d7cc1c5fc887901ed03ad136b5a55002cd49e872b5b3` 及其 424 个文件保持不变。凭据未写入提交或报告。

## 结论与剩余工作

修复使这条实际第三幕路径获得完整正常规则资格，不再把前缀当整局成功。18 条输入仅涉及四个独立游戏种子；没有建立完整 F08 覆盖。冻结样本的重复休息处药水槽为空，药水介于两次休息选择之间的路径未单独验证。Linux 可见实机、公开输入部署和高水平整局策略仍未验收。

Q4-v 的学习阴性门槛不重新计算，诊断种子不能改作训练集。X1 和全部候选的既有零胜结果保持原结论；拟合少量战斗标签没有稳定整局收益，继续扩训该路线仍停止。后续学习应以新的整局覆盖或可泛化的训练信号作独立实验，并用独立完整跑局检验实际收益。本项没有证明高水平 NoSL 已达到。
