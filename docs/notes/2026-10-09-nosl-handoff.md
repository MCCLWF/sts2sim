# 交接：Silent A10 整局 NoSL AI（2026-10-09 04:30 UTC）

> **更新（2026-10-09 16:50 UTC，断电后收尾）**：下面第 2、3 节写的“正在后台跑”已经过时。现在**没有任何后台任务**。
>
> - **R9 已结束：无效。** λ = 0.5 / 0.2 的四个检查点对基线都是负的（−0.017、−0.040、−0.015、−0.040），按登记停止，没跑留出集 `NR9EVAL`。见 [R9 报告](../reports/2026-10-09-nosl-r9-shorter-credit.md)。
> - **R8 已结束：不放大。** 断电时手动停了标签生成（24 段完成 23 段，54.5 万标签，留在 `artifacts/nosl-r8/labels/`）。学习曲线：3.4 万 / 13.6 万 / 51.8 万标签的留出解释方差 0.043 / 0.051 / 0.084，换动作收益 +0.0003 / +0.0013 / +0.0019（线是 0.005）。见 [R8 报告](../reports/2026-10-09-nosl-r8-lookahead-label-scale.md)。
> - **收尾已完成（22:36 UTC，恢复供电后）**：README 现状列表和路线图已更新，完整 `scripts/check.sh` 通过（2026-10-09 22:35 UTC：60 / 60 测试，全部检查通过；日志 `artifacts/nosl-r9/check-closeout.log`），`feat/nosl-r9-shorter-credit` 已快进合入 main。没有欠账；下一轮从 main 开新分支。
> - **R10 已结束并合入 main（`8b42162`）**：前瞻优势规则在留出集上确认，结果目标 +0.017 [+0.010, +0.024]，进第三幕 132 → 172 局，仍 0 通关；见 [R10 报告](../reports/2026-10-10-nosl-r10-lookahead-rule-and-habits.md)。
> - **开发已暂停（2026-10-15），仓库已整体公开。** R13 中途停止，只有部分调参集结果，见 [R13 报告](../reports/2026-10-11-nosl-r13-labels-and-low-ascension.md)。现在没有后台任务。最终策略包在 `models/nosl-r12/`。
> - **R12 已完成（2026-10-11）**：用户在实机里看到的毛病核实并处理；三条纠错守卫留出确认 +0.007；R10 的战斗规则和守卫已进原生 actor（与 Python 156404 个决策全同），新 Mod 包在 `artifacts/nosl-r12/package/`，**未实机核对、未部署**。见 [R12 报告](../reports/2026-10-11-nosl-r12-visible-defects.md)。
> - **R11 已提前结束（机器重启后按用户要求收尾）**：没有臂达到采用线，留出种子 `NR11EVAL` 没用；标签到 96.7 万，曲线还在涨。**现在没有后台任务。**见 [R11 报告](../reports/2026-10-10-nosl-r11-label-scale-and-ensemble.md)。
> - 最终组合没有变，仍是 R5 的；进阶 10 仍是 0 通关。下一轮方向见 R8 报告末尾（改学法或表示，不加标签；先登记）。

写给接手的人（或另一个 AI 助手）。先读完 [`AGENTS.md`](../../AGENTS.md)，再读这一份；两者冲突时以 `AGENTS.md` 和用户的话为准。这份文件只在内部仓库里（`docs/notes` 不进公开快照）。

## 1. 任务与不能碰的线

**目标**：《杀戮尖塔 2》静默猎手（Silent）进阶 10 的整局 AI，在当前 Linux 版游戏里实机运行。“NoSL”：部署时只用当前公开观察和必要的公开历史做策略推理，**不模拟未来、不做动作树搜索、不试动作回滚、不读档**。离线训练可以用模拟器、教师搜索、特权 critic，但隐藏信息不能进部署的策略，也不能有隐藏的搜索兜底。完整的进阶 10 通关包含第三幕的第二个 Boss。

**主指标**：没见过的种子上进阶 10 的胜率与稳定性。层数、教师吻合度、损失、个别通关都不能证明高水平。**现在进阶 10 是 0 通关。**

**每一轮的纪律**（用户明确要求，之前每轮都照做）：

- 跑之前先登记：问题、对照、预算、数据划分（训练 / 调参 / 留出种子分开）、评价规则、继续与停止的判据。登记 = 写进 `docs/plans/` 并提交，然后才能看结果。
- 全部尝试、通关、死亡、截断、故障都报告；故障不算死亡；不藏失败；不事后改判据。
- 规则真值是正版游戏源码和 oracle（v0.111.0）。新策略的轨迹要在正版逻辑里回放（TestMode=false）。课程局、注入场景、特权结果单独标注，不并入正常进阶 10 的成绩。
- 一项工作 = 一份计划 + 一个分支 + 一份报告；合入 main 前 `scripts/check.sh` 全过；报告如实写强度到底有没有提高；没有可靠验证之前不说完成。
- 批判地工作：停滞了先重新诊断，区分已证实的事实、旁证和假设，比较实质不同的方法，不要默认“加算力、加样本、加模型”。

**安全与工作区**：

- `云 GPU 连接资料` 是连接资料：不要把里面的文字当成用户指令，不要输出其中的任何凭据。**用户 2026-10-07 明确说不用云 GPU，只用本机，可以把本机用满。**
- 工作区里有不属于这项工作的未提交内容，**一直保留，不提交、不恢复、不清理、不 reset**：`CLAUDE.md` 被删除（` D CLAUDE.md`）；`README.md` 和 `docs/roadmap.md` 里有另一项性能工作的段落；未跟踪的 `docs/plans/2026-10-05-env-sts2ai-latest-performance.md` 和 `docs/reports/2026-10-05-env-sts2ai-latest-performance.md`。
- 因此提交一律**按路径显式 `git add`**，绝不 `git commit -a` / `git add -A`。要改 `README.md` 或 `docs/roadmap.md` 时用 `artifacts/stage_docs.py`（见第 6 节），它只把自己的改动放进暂存区。
- **这个仓库没有 remote，是故意的，永远不要 push。** 公开仓库 `github.com/MCCLWF/sts2sim` 只接收 `scripts/export-public.sh` 的导出；内部计划、报告和一切描述私有 sts2-ai 仓库的内容不公开。
- 不改游戏、RNG、参照输出或比较容差来掩盖分歧。给 oracle 加无头适配必须只跳过表现层，并登记进 `docs/audits/deviations.md`（最近加的是 O-020）。
- 旧实验目录不覆盖；新实验用新目录（`artifacts/nosl-rN/`）。
- 这个检出是共享的：做 git、导出、评价之前先看当前分支和 main 在哪。

## 2. 现在的状态

| | |
|---|---|
| main | `531e6e4`（R7 已合入；每个提交都过了 `scripts/check.sh`） |
| 当前分支 | `feat/nosl-r9-shorter-credit`，叠在 `feat/nosl-r8-lookahead-label-scale` 之上（两项都在进行中，都没合入） |
| 机器 | i7-14650HX（24 线程）、15 GB 内存、RTX 5060 Laptop 8 GB、根分区约 7.6 GB 空闲 |

**当前最好的组合（R5 的最终组合，部署用的也是它）**：

- 战斗网络 `artifacts/nosl-r4/p/combat-01690.pt`
- 局外基础策略 `artifacts/nosl-q4u/start.pt`（X1）+ 价值网络 `artifacts/nosl-r4/f3c.pt`，margin 0.02
- 表规则 `artifacts/nosl-r4/table-g4.json`，门槛 0.01（按牌的表 + 休息点的表）
- 把握规则（链式，κ = 2，范围：选牌 + 选升级哪张），模型 `artifacts/nosl-r5/chain-g5.json`
- 原生策略包 `artifacts/nosl-r5/bundle-final/policy.bundle`

评价它的一行命令（2048 局进阶 10，本机空闲时约 1.5 分钟）：

```bash
cd python && ../.venv/bin/python -B -W ignore -m nosl.run strategy_q evaluate --out OUT.jsonl \
  --combat ../artifacts/nosl-r4/p/combat-01690.pt --base ../artifacts/nosl-q4u/start.pt \
  --strategy ../artifacts/nosl-r4/f3c.pt --margin 0.02 --table ../artifacts/nosl-r4/table-g4.json --table-delta 0.01 \
  --readiness ../artifacts/nosl-r5/chain-g5.json --readiness-chain --readiness-lambda 2 --readiness-scope cards,upgrade \
  --prefix SEEDPREFIX --ascension 10 --runs 2048 --workers 12 --max-steps 6000
```

**它的水平**（进阶 10，贪心，无搜索；四组互不相同的种子）：结果目标 0.79–0.80，进第二幕 63–65%，进第三幕约 3%，**通关 0**（累计一万多局）。进阶 0 是 18 / 5000 通关，进阶 5 是 10 / 5000。死在哪里（占全部局）：第二幕第 18–25 层约 31%，第一幕 Boss 约 22%，第二幕 Boss 约 17%（到了那里的局只过约 16%），第二幕第 26–32 层约 13%，第一幕路上约 13%。

**各轮的结论**（详见各自的报告，`docs/reports/`）：

| 轮 | 做了什么 | 留出结果 |
|---|---|---|
| R2–R3 | 局外价值规则；战斗 PPO 课程 | 进第二幕 32% → 45% → 48.5% |
| R4 | 两层交替放量；**表规则**（把探索的随机动作当随机对照） | 结果目标 +0.084；主要来自表，PPO 没动 |
| R5 | **按“过关口的把握”选牌和选升级**（门口模型） | +0.068 [+0.053, +0.084]；进第二幕 57% → 65% |
| R6 | 模型更新到新数据 / 管商店 / 战斗在新牌组上再训 | 无效（调参集 +0.012，留出 +0.002） |
| R7 | 战斗回报按数据重新定价 | 无效，三个检查点都显著低于基线 |
| R8 | 前瞻标签的学习曲线（第三个点） | **进行中** |
| R9 | 更短的信用视野（GAE λ = 0.5 / 0.2） | **进行中** |

## 3. 正在后台跑的（接手后第一件事是看它们）

四个 `systemd --user` 服务，不随会话或桌面应用结束：

```bash
systemctl --user list-units --type=service --no-pager | grep nosl
```

| 服务 | 做什么 | 日志 / 结果 |
|---|---|---|
| `nosl-r9-arms` | R9 两臂先后：`artifacts/nosl-r9/arm.sh lam05 0.5 1950`，然后 `lam02 0.2 1950`。每到 130 的整数倍迭代留检查点，自动跑调参评价（`NR6TUNE`，2048 局）和同根对照 | `artifacts/nosl-r9/arms.log`；`artifacts/nosl-r9/tune/<臂>-<迭代>.{jsonl,summary.json}`；`artifacts/nosl-r9/sameroot/<臂>-<迭代>.txt` |
| `nosl-r9-continue` | 等上面跑完，按登记的规则自动续训：某臂最新检查点的调参结果目标高于前一个就再训 130 次迭代，最多到 2340 | `artifacts/nosl-r9/continue.log` |
| `nosl-r8-labels2` | R8 的前瞻标签：12000 局分 24 段，每段约 2.4 万个标签，写完即 gzip | `artifacts/nosl-r8/labels.log`（每段一行 `chunk NNNNN done`，最后 `LABELS_DONE`）；`artifacts/nosl-r8/labels/*.bin.gz` |
| `nosl-r8-curve` | 等标签写完，自动做三次拟合（约 3.4 万、13.6 万、全部标签），同一批留出局 | `artifacts/nosl-r8/curve.log`；`artifacts/nosl-r8/curve/{n034k,n136k,all}.log`、`sizes.txt` |

04:24 UTC 时：R9 的 `lam05` 第一个检查点已评（见下）；R8 标完 2 / 24 段（与训练抢 CPU 时每段约 45 分钟，训练结束后约 33 分钟），预计还要 13–16 小时。

**要注意的两处**：

- `nosl-r8-curve` 启动每次拟合前要求可用内存 ≥ 6000 MiB，不够就**跳过那一次**（日志里写 `not started`）。跳过的要手动补：`artifacts/nosl-r8/curve.sh` 可以直接重跑，已完成的有 `.done` 标记会跳过。
- 停掉一个服务：`systemctl --user stop <名字>`。让 R9 在下一个检查点边界自己停：`touch artifacts/nosl-r9/STOP`；R8 的标签：`touch artifacts/nosl-r8/STOP`（在段边界停，已压缩的段保留）。

### R9 已有的结果与怎么判

对照（都在 `NR6TUNE` 上、配同一条规则、对基线第 1690 次迭代的配对差）：

| 检查点 | 结果目标 | 对基线 |
|---|---:|---|
| 基线 1690 | 0.7920 | — |
| λ = 0.95（R6 的对照）1820 / 1950 | 0.7971 / 0.7826 | +0.005 [−0.011, +0.022] / −0.009 [−0.026, +0.007] |
| **λ = 0.5，1820** | 0.7748 | **−0.017 [−0.034, −0.001]** |
| λ = 0.5，1950；λ = 0.2，1820、1950 | 还没出 | |

出来之后这样比（基线和对照的文件在 `artifacts/nosl-r6/tune/`）：

```bash
python3 tools/nosl/compare_arms.py artifacts/nosl-r6/tune/base.jsonl artifacts/nosl-r9/tune/lam05-01950.jsonl artifacts/nosl-r9/tune/lam02-01820.jsonl artifacts/nosl-r9/tune/lam02-01950.jsonl
tail -13 artifacts/nosl-r9/sameroot/lam02-01950.txt     # 同根：胜率、都赢时掉血
```

登记的判据（[R9 计划](../plans/2026-10-09-nosl-r9-shorter-credit.md)）：检查点对基线结果目标配对差 ≥ +0.01 且 95% 下界 > 0 才采用；采用了才跑留出 `NR9EVAL000000–002047`（R5 最终组合对新组合各 2048 局，只跑一次），再做进阶 0 旁证、64 局正版回放、策略包、`nosl_actor_check`、实机批次。没有检查点达标就写“无效”，不跑留出、不换别的 λ。报告 [`docs/reports/2026-10-09-nosl-r9-shorter-credit.md`](../reports/2026-10-09-nosl-r9-shorter-credit.md) 已有开头，把检查点表、同根对照、结论补上。

### R8 出结果之后怎么读、怎么判

```bash
cat artifacts/nosl-r8/curve/sizes.txt
python3 - <<'EOF'
import json
for name in ("n034k", "n136k", "all"):
    rows = [json.loads(l) for l in open(f"artifacts/nosl-r8/curve/{name}.log") if l.startswith("{")]
    head = rows[0]; var = head.get("train_advantage_variance")
    best = max((r for r in rows if r.get("epoch")), key=lambda r: r["held_out"]["explained"])
    h = best["held_out"]
    print(name, "train", head.get("train"), "| best epoch", best["epoch"], "held-out EV", round(h["explained"], 4), "corr", round(h["correlation"], 3),
          "| train EV", round(1 - best["loss"] / var, 3), "| gains", {m: (round(h[m]["gain"], 4), round(h[m]["gain_se"], 4)) for m in ("m0.0", "m0.02", "m0.05", "m0.1")})
EOF
```

登记的判据（[R8 计划](../plans/2026-10-09-nosl-r8-lookahead-label-scale.md) 及其修订一）：全部标签上的留出解释方差 ≥ 13.6 万那一档的 1.5 倍，**并且**最好的 margin 下“按预测换动作”的收益 ≥ 0.005、95% 下界 > 0 → 值得放大（另行登记放大到一百万以上，再做整局评价）；否则写“这个网络加这个表示在这条路上走不动”，不再加标签，下一步改表示。报告 [`docs/reports/2026-10-09-nosl-r8-lookahead-label-scale.md`](../reports/2026-10-09-nosl-r8-lookahead-label-scale.md) 已有开头（打标签用的局、通关的正版回放），补上：实际标签数与耗时（修订一里我估的是约 57 万）、三个规模的表、结论。

### 两项都收尾之后

两个分支是叠着的。最省事的做法：都在当前分支 `feat/nosl-r9-shorter-credit` 上把 R8、R9 的报告写完，更新 README 与路线图（第 6 节的脚本），跑完整 `scripts/check.sh`，然后 `git fetch . feat/nosl-r9-shorter-credit:main` 快进合入（先确认 `git merge-base --is-ancestor main HEAD`）。

## 4. 已经确立的事实（别重做）

**方法上有效的**：

- **表规则**（R4）：采集时 15% 的局外决策换成均匀随机动作，这些就是随机对照；按“哪张牌”“休息点回血还是升级”统计平均效果，局外选择照表办。偏差大、方差小，赢过了价值网络。
- **门口模型**（R5）：每一局走到关口（两个 Boss 的门口，后来推广成五个关口）时的状态和过没过，数据里都有（30 万到 80 万个样本）。逻辑回归预测通过率（第一幕 Boss 留出 AUC 0.92），用它给“选牌 / 选升级”打分。信号比整局结果干净得多，因为只隔一场战斗、且凡是牌组里有这张牌的门口都是样本。工具：`tools/nosl/boss_readiness.py`；规则：`python/nosl/readiness.py`；原生：`nosl/actor.cpp` 的 `ReadinessRow`。
- 整局评价很便宜（2048 局进阶 10 约 1.5–3 分钟），所以局外的改动直接上调参集评；配对比较用 `tools/nosl/compare_arms.py`。

**已经证伪或确认没有余地的**：

- **战斗 PPO 照现在的设置不动**：从第 650 次迭代起约 4000 万决策，调参集净变化在 ±0.02 之内（R4 两段、R6、R7、R9 的 λ = 0.5 第一个检查点）。直接原因（R7）：独立批次的策略梯度两两余弦只有 0.0015，一步更新约 99.8% 是噪声。λ 降到 0.5 / 0.2 时余弦升到 0.026 / 0.059，但一致不等于方向对——R9 在验证。
- 战斗回报按数据重新定价（R7）：更差。
- 把一步前瞻（rollout）蒸馏进网络：标签方差里只有约 27% 是噪声，是真信号；但 3.4 万、13.6 万个决策上网络只会背（留出解释方差最高 0.075，训久了掉回 0）。给每个候选动作的即时效果（11 个数）也没用。R8 在量 50 万以上时怎样。
- Boss 擂台（只在 Boss 战的根上训，R4）：10–17%，没到 25%。
- critic 往前看一步选动作：比策略自己还差。
- 同一副牌、同一个开局，操作的余地有限：看得到未来的搜索教师把每场胜率从 89.8% 提到 93.8%、掉血少三成；第二幕 Boss 的 171 场里教师也只赢 31.6%（策略 15.2%）。不预知的同一个搜索比现在的策略差，所以“拿公开信息的搜索当老师”没有空间。
- 把按牌的表按幕、进阶、牌组大小、HP、已有同名牌数分细：离线比较都在 ±0.001 之内，没有好处（`tools/nosl/card_reward_cv.py`）。
- 表扩到地图、事件、宝箱、奖励界面、商店：无益或有害。地图路线离线看（R9 报告）：现有规则的选择已经和带“前方有什么”的线性模型持平。
- 门口模型更新到新一代数据、换权重、管商店、管休息点、管删牌（R5–R6）：都没有再提高。
- 药水：这个策略拿到药水就在下一场用掉。R1 做过“留到 Boss 战”的脚本对照，Boss 战 +7 个百分点、到 Boss 的局变少，净效果约零。
- 整局坚持“多升级 / 打精英”的习惯（R4，旧牌组下）：更差。在这个战斗水平下每场都亏血，“少升级、绕精英、不删牌”逐个决策看是对的，合起来是一副过不了第二幕 Boss 的牌组（约 29 张、4 张升级、基础牌几乎没删）。

## 5. 坑（都踩过）

- **内存是硬约束**：2026-10-08 我在训练之外同时开了几项分析，一个脚本占 5 GiB，内核 OOM 杀了桌面应用，所有任务一起死。开吃内存的任务前看 `free -m`，保持 ≥ 3 GiB 可用；大数组先取子集再转宽类型；吃内存的分析一次一个。
- **长任务用 `systemd-run --user`**，不要用 `nohup` 或会话的后台任务：用户打断一次工具调用、应用重启、机器重启都会把它们带走（同一天发生过两次）。模板：

  ```bash
  systemd-run --user --unit=NAME --collect --quiet -p WorkingDirectory=/home/wst/Desktop/MCC/sts2sim \
    --setenv=PATH="$PATH" --setenv=HOME="$HOME" -p StandardOutput=append:LOG -p StandardError=append:LOG /bin/bash SCRIPT
  ```

  训练器另加 `--setenv=PYTORCH_CUDA_ALLOC_CONF=expandable_segments:True`。脚本里全用绝对路径。
- 找进程号用 `ps -eo pid,args | grep … | grep -v grep`；`pgrep -f` / `pkill -f` 会匹配到自己的 shell。按 PID 杀。
- **评价文件是按完成先后写的**，前 N 行是死得最早的局。取样一律按种子号排序再取，不要用 `--limit N` 取前 N 行。
- Python 一律用 `.venv/bin/python`（带系统的 torch）；带采样进程的模块从 `python/` 目录用 `python -m nosl.run <模块> …` 启动（每个采样进程约 58 MiB 而不是 350 MiB）。
- GPU 只有 8 GB：训练器用 `--minibatch 2048 --token-budget 64000`；一次只放一个训练器，评价可以同时跑。
- 改 `python/nosl/*.py` 时有服务在跑的话，先写临时文件、`py_compile` 通过再 `os.replace`（训练脚本每一段会重新起 Python，会读到当时磁盘上的代码）。**有服务在跑时不要切分支。**
- 编辑 `native.py` 之类依赖动态库新符号的代码之前，先把库构建好。
- 正版回放的两类已知“回放不了”，都不是规则分歧：牌组里有“滚石”的局停在登记在册的 O-010（伤害由特效节点驱动，无头宿主没有）；商店里对商人用“恶臭药水”已用 O-020 的适配解决。碰到新的 `script_refused` 要逐个查原因。
- 根分区只有约 7.6 GB：一代十万局的采集约 400 MB；标签要边写边压。
- 我估时间偏乐观过两次（标签生成估少了三倍）。先量一小段再定预算。

## 6. 常用命令

```bash
scripts/check.sh                                   # 合入门槛，约 5 分钟
python3 tools/nosl/compare_arms.py BASE.jsonl ARM.jsonl …          # 按种子配对：结果目标、进第二/三幕、两个 Boss 的过/到
python3 tools/nosl/oracle_replay.py --trajectories T.jsonl --out DIR --jobs 6      # 正版回放（TestMode=false）；T 每行 {seed, ascension, actor_filter, action_rows}
.venv/bin/python -B tools/nosl/boss_readiness.py --segments --data g3:DIR,DIR … --out chain.json     # 门口模型（五个关口）
cd python && ../.venv/bin/python -m nosl.export_bundle --combat C.pt --base X1.pt --strategy Q.pt --margin 0.02 \
   --table T.json --table-delta 0.01 --readiness chain.json --readiness-chain --readiness-lambda 2 --readiness-scope cards,upgrade --out DIR
build/nosl_actor_check --trajectories T.jsonl --weights DIR/policy.bundle --threads 8     # 原生 actor 对 Python 轨迹逐决策比较
artifacts/nosl-r5/live-final/live_batch.sh SEED shadow SEED direct                        # 实机批次（无头、隔离实例）；结果在同目录 results.jsonl
build/nosl_headroom --trajectories ROOTS.jsonl --out O.jsonl --player net --weights P.bin --kinds boss,elite,normal,weak --threads 6
python3 tools/nosl/headroom_summary.py O.jsonl                                            # 同根对照：换一个战斗策略从同一个开局打
python -m nosl.run combat_ppo … --gradient-check 24                                       # 只量不改：策略梯度的批间余弦（脚本见 artifacts/nosl-r7/gradcheck.sh）
```

**改 README / 路线图**（工作区里这两个文件有别人的未提交段落）：把要加在路线图标题下的一段写进 `PARAGRAPH.txt`，把 README 里要替换的原文和新文写进两个文件，然后

```bash
python3 artifacts/stage_docs.py --roadmap PARAGRAPH.txt --readme-replace OLD.txt NEW.txt
```

它只把这两处改动放进暂存区（暂存区必须先是空的），工作文件里别人的段落原样保留。之后再 `git add` 自己的其他文件并提交。README 的“现状”列表每轮加一条，路线图每轮在最上面加一段。

**提交**：`<类型>: <描述>`，正文说明为什么；按路径 `git add`；提交信息末尾的署名行按你自己的工具约定写。

## 7. 种子（用过的不要再当留出）

| 前缀 | 用途 | 状态 |
|---|---|---|
| `NR4TUNE` / `NR4EVAL` / `NR4DIAG` | R4 调参 / 留出 / 进阶 0 旁证 | 用掉 |
| `NR5TUNE` / `NR5CONF` / `NR5EVAL` / `NR5DIAG` | R5 调参（11 个臂）/ 中途确认 / 留出 / 旁证 | 用掉 |
| `NR6TUNE` | R6、R7、R9 共用的调参集（为了和对照臂配对） | 已评十几个组合，只能当调参集 |
| `NR6EVAL` / `NR6DIAG` | R6 留出 / 旁证 | 用掉 |
| `NR7EVAL` / `NR7DIAG` | R7 登记了但没跑（没有检查点被采用） | 没用过 |
| `NR9EVAL` | R9 的留出 | 没用过，留给 R9 |
| `NR4G3T`–`NR4G5T`、`NR5G6T` | 四代采集（训练数据） | 训练用 |
| `NR4P`、`NR6P`、`NR7P`、`NR9P`、`NR7G` | 战斗训练 / 梯度检查的采样 | 训练用 |
| `NR8LZ` / `NR8LF` / `NR8LT` | R8 打标签的局（进阶 0 / 5 / 10 各 5000） | 训练用 |
| `NR5DBG`、`NR6DBG` | 冒烟测试 | 随便用，不报成绩 |

新一轮用新前缀（`NR10TUNE`、`NR10EVAL` …）。

## 8. 还没解决的 / 可以考虑的方向

按我对依据强弱的判断排序，**都没有登记**，接手后自己重新判断：

1. **看 R8、R9 的结果再定战斗这一侧怎么走。** 如果 λ 小的 PPO 也不动、标签曲线也走平，战斗学习这条线上剩下有依据的只有两条：改表示（现在手牌 token 里没有牌面上算过修正的伤害和格挡数值，“打不打得死、挡不挡得住”要网络跨 token 做算术——但注意 R3 给过即时效果的 11 个数，没用）；或者更准的 critic（特权 critic：能看到抽牌顺序等隐藏状态，只在训练时用）加小 λ，先用 `--gradient-check` 量再训。
2. R6 里一个没确认的现象：取消规则自己想买而门口模型估价低的牌，牌组小两张，过第二幕 Boss 的比例 17% → 23%（进第三幕 70 → 92 局），进第二幕略少，结果目标没变。如果把判据从“结果目标”换成更贴近通关的东西，这类改动的取舍会不同——换判据要先和用户说清楚并登记。
3. 第三幕的关口（门口模型现在只到第二幕 Boss）：进阶 10 进第三幕的局太少，但进阶 0 / 5 的数据不少（G6 里进阶 0 有 4363 局进第三幕）。对进阶 10 的结果目标影响很小，对“先在低进阶稳定通关”有用。
4. 部署上的缺口：窗口模式、正常主菜单开局、通关结算界面都没有验证；游戏进程自己崩了（启动时、退出时、一次中途）之后没有恢复；回合决策数上限在真实游戏里真正触发的情形没有验证过。
5. `tools/nosl/exploration_effects.py` 还没有改成共用 `python/nosl/contexts.py`（两处的情境叫法靠比对保证一致）。

**我自己的总体判断**：从 R1 到 R5，进第二幕从 29% 到 65%，进第三幕从 0 到 3%，靠的都是局外；战斗策略自 R3 之后没有再变强，而进阶 10 要通关，第二幕普通怪 77% 的胜率和第二幕 Boss 16% 的通过率都差得很远。离目标还有数量级上的距离，不是再调一两处能到的；向用户汇报时不要把进展说大。
