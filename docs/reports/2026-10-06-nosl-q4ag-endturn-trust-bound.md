# Q4-ag：有上限EndTurn更新报告

状态：固定七策略各1024新局全部真实死亡、0故障/截断/通关。硬上限跨两初始化减轻了无上限更新的退步，但两候选仍显著弱于其初始E头和原X1；机制门通过、净收益门失败，不扩训本配置、不替换X1。四头训练、完整优化器独立重放、3209公共状态合同、24条正常原版资格、18条最终正常原版整局及完整主线门禁已通过。全程本地CPU，云不启动；完整Silent A10高水平NoSL及Linux成品尚未达到。

## 前置、学习与实际约束

前置main `b5ed355d34f20028c6f20f6325a717e479f2f3d2`，按[固定计划](../plans/2026-10-06-nosl-q4ag-endturn-trust-bound.md)保持原Q4-ae数据/网络/预算，只加硬上限。Q4-ac/ad/ae/af的263/352/360/171个封存文件逐一验证；不使用Q4-af旧评价种子训练。缓存gate146/action128仅由四公共表、冻结X1重建的原缓存，原450root/352训练anchor/177方向label/49开发留出，原margin权重不变。所有未来reward/种子仍只作离线标签/诊断。

```bash
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/endturn_trust_bound.py --out artifacts/nosl-q4ag/learning
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/verify_endturn_trust_bound.py \
  --learning artifacts/nosl-q4ag/learning --out artifacts/nosl-q4ag/learning-audit.json
```

两命令实际退出0。学习source `94c49c2a828d617f3400608651511241fb6be951`，PID1378410，2026-10-06 14:54:32.202248–14:54:51.885482UTC，宿主wall19.683232秒，训练/合同内部18.485733秒≤180。CPU4/nice19/Torch1/float32，kernel child maxrss922.082031MiB、user/system17.269671/2.368762秒。实际本地Torch2.14.1+cu130使用CPU，不占云GPU。

每seed1/3的U/T同初始原E、同27138学习参数、同seed NumPy minibatch顺序、fresh Adam .0003/gradnorm1、128epochs/256优化器尝试；固定最终checkpoint/完整Adam，没有中途/留出挑权重。U直接调用原学习代码，最终checkpoint所有tensor/metadata及完整Adam全部逐位重现旧C；checkpoint文件SHA也相同：U1 `b450551a73c2311fd14a795a12d9b7497d6ee52685c493d939ab4b12b8a3d612`，U3 `d834d3ff6fdb0d13b4a92fa14913cb7c80da3b5562e233ab6baa2d05a2a0c373`。

T仍优化原margin-weighted BCE+.1初始Bernoulli KL，仅在Adam proposal后从旧参数向proposal回溯alpha1,1/2,...,2^-20；不合格alpha0恢复参数，Adam moments保留proposal更新。按352训练root initial||new约束mean≤.02、max≤.2，实际工作余量.0199/.1999。记录全部proposal/accepted KL、alpha、minibatch和参数差；约束不覆盖未见状态，也不是整个贪心策略KL。

| 头 | 训练balanced acc | 留出balanced acc | 最终训练root mean KL | max KL | 投影/拒绝尝试 |
|---|---:|---:|---:|---:|---:|
| U1 | .897616 | .505682 | .634940 | — | — |
| T1 | .519705 | .565341 | .009426 | .199900 | 250/219 |
| U3 | .873422 | .553030 | .615474 | — | — |
| T3 | .532118 | .612689 | .009421 | .199900 | 250/224 |

T1/T3最终weighted BCE11.832543/11.960376，初始13.105367/13.156159，均至少降.001、有实际参数改变，因此按本项独立资格可评价；未因旧“.8拟合门槛”另加epoch或放宽约束。其多数proposal拒绝，说明本协议的方向受max限制瓶颈影响，不能称完全解决受约束优化。T学习含保存约2.17秒各一个、U约.6/.4秒；训练拟合和留出均不代替整局效果。

## 正常新局资格与完整门禁

```bash
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/endturn_trust_experiment.py pilot \
  --learning artifacts/nosl-q4ag/learning --out artifacts/nosl-q4ag/pilot
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/endturn_trust_experiment.py normal \
  --experiment artifacts/nosl-q4ag/pilot --out artifacts/nosl-q4ag/pilot-normal
taskset -c 4,5 nice -n 19 env STS2_BUILD_JOBS=2 scripts/check.sh
```

三命令实际退出0。pilot source `fdb7824a4ee0a5f464bb6a36b699f010addb134c`，六臂各4相同新NQ4AGPILOT正常Silent A10，全部24真实死亡、0故障/截断/胜利；E1/T1/E3/T3均17层，U1/U3均8层，只作为系统资格，不以4局均值宣称强度。**24/24完整正常TestMode=false原版回放，4555/4555决策、逐步快照和全部end.run字段一致**，failures=[]；重复seed跨模型是24次执行，不是24个独立质量样本。normal实际wall51.884423秒，2026-10-06 15:04:04.761929UTC终止。

完整门禁source `fffe4997b51dbc9a2e7e2e7a632d1cf809db8ae3`，2026-10-06 15:05:25.333300–15:10:20.578577UTC，wall295.245283秒，实际退出0，末行all checks passed。其后只改文档，运行源码未改。

T1/T3 checkpoint SHA分别`f52b405cde358114243c773da627445618ff22faf64207fc201db6c1689854c6`、`f37a182e90d05bfe786f667205c1d4fbb6976ca01dd3c721d78b6d3b029beee1`。沿用显式Python实验加载格式，generic-loader拒绝，尚无C++/Linux加载适配。

独立学习审计wall6.373097秒、实际退出0：从900原终局分支重算450label对/权重/划分缓存；独立重新计算512次T梯度、proposal、所有回溯试点，最终完整tensor/Adam及逐epoch指标逐值相同。U旧对照也独立重核，不只是相信summary。完整X1 tensor/value、mask、非适用输出及非EndTurn argmax在3209公共状态（611不适用/368局外）保持；错误format/schema/baseline/nonfinite与通用加载器全部明确拒绝。

## 固定新种子的实际整局结果

```bash
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/endturn_trust_experiment.py evaluate \
  --learning artifacts/nosl-q4ag/learning \
  --normal-qualification artifacts/nosl-q4ag/pilot-normal/summary.json \
  --learning-audit artifacts/nosl-q4ag/learning-audit.json \
  --check-result artifacts/nosl-q4ag/check-result.json --out artifacts/nosl-q4ag/experiment
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/verify_endturn_trust_bound.py \
  --evaluation artifacts/nosl-q4ag/experiment --out artifacts/nosl-q4ag/verified-results.json
```

正式采样source `10a884a8a741a77fd4c837811a91de53c48691da`，实际PID1382987，2026-10-06 15:11:53.219580–15:37:13.318761UTC，wall1520.099184秒，未超全局1800秒。CPU4/nice19、单Torch线程、CPU float32、原64槽贪心；kernel child maxrss711.566406MiB，user/system1474.744935/44.168116秒。七组使用相同的新`NQ4AGEVAL000000–001023`，正常Silent A10，无HP/牌组/遗物注入，保留每条完整动作和真实终局。全部7168实际终止、1,349,411策略决策、0故障/截断；所有最终checkpoint SHA在采样前后相同。

| 策略 | 平均层数 | 第二幕到达 | 第三幕到达 | 完整A10胜利 | 显式EndTurn决策 |
|---|---:|---:|---:|---:|---:|
| X1 | 18.533203 | 327 | 2 | 0/1024 | 3 |
| E1 | 17.410156 | 257 | 1 | 0/1024 | 6814 |
| U1 | 9.388672 | 50 | 0 | 0/1024 | 8835 |
| T1 | 16.988281 | 228 | 1 | 0/1024 | 7705 |
| E3 | 17.300781 | 242 | 2 | 0/1024 | 6802 |
| U3 | 9.468750 | 50 | 0 | 0/1024 | 8524 |
| T3 | 16.925781 | 231 | 1 | 0/1024 | 7355 |

显式EndTurn不包含原生仅剩EndTurn时的自动动作，不能从X1计数3推断它只结束过3次回合。每策略观测胜率0%，零成功的双侧Clopper–Pearson 95%区间为[0%,约0.360%]；同seed跨策略相关，不能把7168条合并成单一模型的独立胜率样本。这是开发比较，不是高水平或Linux成品验收。

| 固定配对 | 层数差 | 配对bootstrap 95%区间 | 预设门 |
|---|---:|---:|---|
| T1−U1 | +7.599609 | [+7.192383,+8.014673] | 机制通过 |
| T3−U3 | +7.457031 | [+7.043945,+7.879907] | 机制通过 |
| T1−E1 | −.421875 | [−.599609,−.248047] | 净收益失败 |
| T3−E3 | −.375000 | [−.549805,−.202148] | 净收益失败 |
| T1−X1 | −1.544922 | [−1.846680,−1.237305] | 净收益失败 |
| T3−X1 | −1.607422 | [−1.911133,−1.298828] | 净收益失败 |

固定10000次bootstrap/rng20261006、同1024seed配对。机制门要求两T−U均≥2层且区间下界>0，实际通过；净收益要求两T分别超过E/X1至少.5层且下界>0、第三幕到达不少于X1，实际失败。正式driver实际退出1表示已预设的质量门失败，全部终局真实完成，不能记成运行故障。独立统计命令实际退出0、wall1.426279秒，从全部raw另行重算终局/动作/层数/选择与配对区间，结果完全相同。

这轮证据支持此固定硬上限协议减轻无上限更新造成的整局退步；不证明标签方向正确、约束优化已充分学习或候选比原模型强。两T对初始E仍跨seed显著下降，不能把“比退步更严重的U好”算作策略提升。保持X1，不按结果追加epoch、种子或调上限。后续路线需要另立问题，针对标签与实际访问状态的匹配、公开动作效果/历史以及整局学习信用做有效比较；这些尚未实施，最终高水平目标继续未完成。

## 最终原版验证、资源及交付

```bash
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/endturn_trust_experiment.py normal \
  --experiment artifacts/nosl-q4ag/experiment --out artifacts/nosl-q4ag/final-normal
```

实际退出0、wall160.165877秒，2026-10-06 15:37:33.955832–15:40:14.121703UTC。本项预先固定每个E/U/T臂首2局+最深实际终局去重+全部胜场（实际无）；独立从全部raw重新核验选择，六臂各3条，共**18/18完整正常TestMode=false、6571/6571决策、逐步快照和全部end.run字段一致**，failures=[]。E1/U1/T1/E3/U3/T3各1375/740/1229/1356/722/1149原版决策。每臂首2seed000000/000001；最深分别000342/000160/000342/000342/000037/000342，终局48/33/46/46/33/36层死亡。

包括pilot，共42次正常原版整局执行、11126个原版决策；跨模型/阶段重复种子，不能当42个独立强度样本，也不能把18条验证说成7168条全部Oracle认证。完整自然终局身份、无TestMode、无HP/牌组/遗物注入、全部终点无first_diff/native_error及权重不变由独立收尾再次核对。

本项只使用本地CPU：采样RSS每5秒记录，最大采样676.371094MiB/1线程，kernel child maxrss711.566406MiB，二者口径不同。学习/评估/原版回放真实结束，未以工具轮询超时重启任务。云pro-7912450e5767于2026-10-06 15:35:22.703189UTC经只读API确认Success/shutdown；没有power_on、SSH部署、云训练或云评价，云使用时间0。

生产actor、游戏、奖励、原生库及旧学习工具不改，沿用实验Python格式，C++/Linux部署尚未完成。完整门禁之后仅改报告/进度文档。Q4-ac/ad/ae/af的263/352/360/171个旧封存文件收尾逐一大小/SHA复核不变；外来CLAUDE.md删除、README/roadmap性能段落及两份性能文档全部保留，进度只stage自有前缀。按完整门禁合入main，保留X1，完整高水平Silent A10 NoSL目标仍未完成。

证据于2026-10-06 15:42:03.826799UTC封存：`artifacts/nosl-q4ag/manifest.json`列出515个文件、40,726,016字节，manifest SHA256 `f42c5f5042d4c479c9863e819e6d785dd7d916b3964a0df963e5c6fc5d9c5597`。全部文件大小/SHA验证通过；原生库也与正式评价记录SHA相同。封存后本目录不再写入，下一工作项使用新目录。
