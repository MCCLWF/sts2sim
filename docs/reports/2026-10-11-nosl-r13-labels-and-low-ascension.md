# R13：标签继续放大；低进阶另用一套守卫——报告

状态：**按用户要求中途停止，没有结论**。2026-10-11 20:12 UTC 启动后不久机器重启；2026-10-12 02:22 UTC 重新启动，约两小时后用户以机器负担太大为由要求停止（`STOP` 已写，服务已停）。2026-10-15 用户决定暂停开发、公开仓库，本轮不再续跑。只有下面这些调参集上的部分结果，**都没有经留出确认**。进阶 10 仍为 0 通关，高水平 Silent A10 / Linux NoSL 目标仍未达到。

计划：[R13](../plans/2026-10-11-nosl-r13-labels-and-low-ascension.md)。分支 `feat/nosl-r13-labels-and-low-ascension`。原始产物在 `artifacts/nosl-r13/`（不提交）。

```bash
B=$PWD/artifacts/nosl-r13          # 重启后再起一次即可接着跑
systemd-run --user --unit=nosl-r13 --collect --quiet -p WorkingDirectory=$PWD --setenv=PATH="$PATH" --setenv=HOME="$HOME" \
  -p StandardOutput=append:$B/run.log -p StandardError=append:$B/run.log /bin/bash $B/run.sh
systemd-run --user --unit=nosl-r13-guard --collect --quiet -p StandardOutput=append:$B/guard.log -p StandardError=append:$B/guard.log /bin/bash $B/guard.sh
.venv/bin/python -B artifacts/nosl-r13/finish.py summary     # 随时看已完成的部分
```

结束标志：`run.log` 里的 `== R13_DONE`；汇总 `artifacts/nosl-r13/summary.txt`；三路日志 `lane-c.log`、`lane-g.log`、`lane-l.log`。任务在跑时不要切换 git 分支。

## 结果（部分，调参集）

标签：新增 1 段（`n-05500`），其余 18 段没有跑。拟合的新一期没有开始。

进阶 10（`NR13TUNE`，4096 局，参照 `ref` = R5 + R10 的战斗规则 + 三条纠错守卫）：`ref` 结果目标 0.8037，进第二幕 2638，进第三幕 148，全部真实死亡。R11 第二期两个拟合取平均（87.4 万标签）：margin 0.05 +0.0050 [−0.0034, +0.0135]，margin 0.02 +0.0050 [−0.0046, +0.0146]，都没有达到登记的线（R11 在另一组种子上是 +0.008，同样没过）。

进阶 0（`NR13ZTUNE`，4096 局）：`ref` 结果目标 1.388，进第三幕 1564（38.2%），通关 30，另有 1 局停在牌池容量上限（不算死亡）。

| 加一条守卫 | 结果目标的配对差 | 进第三幕 | 通关 |
|---|---|---|---:|
| `smith=0.9` | +0.0194 [+0.0115, +0.0273] | +2.0 个百分点 [+1.2, +2.8] | 34 |
| `hold=0.3` | +0.0176 [+0.0012, +0.0340] | +2.2 [+0.6, +3.9] | 36 |
| `hold=0.5` | +0.0085 [−0.0077, +0.0247] | +0.6 | 38 |
| `removefirst` | +0.0112 [−0.0037, +0.0262] | +2.4 [+0.9, +4.0] | 21 |
| `nopotionbuy` | 没有跑完 | | |

按登记的判据，`smith=0.9` 和 `hold=0.3` 会进低进阶那一套；拼出的组合、进阶 5 的评价和两处留出都没有跑，所以没有结论，`guards_low` 没有进策略包。代码改动（`smith` 守卫、按进阶切换的 `guards_low`，Python 与原生）已提交并通过合入门禁，进阶 10 的行为不变。
