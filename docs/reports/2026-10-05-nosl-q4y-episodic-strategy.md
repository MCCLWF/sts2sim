# Q4-y：冻结战斗的整局回报战略学习

**两组固定训练均完成，但双学习seed继续门槛失败，不扩训本配置、不替换正式X1。** S1在1024新tune种子上增加0.8691层、配对95%区间全在零上；S3仅增加0.3213层、区间跨零，训练另有1条步数截断。三策略各1024次完整原生评价均死亡，0故障/0通关；S1仅1次第三幕。第一组的正向结果值得保留研究，但没有跨学习seed复现，高水平Silent A10整局/Linux NoSL仍未达到。

计划：[Q4-y](../plans/2026-10-05-nosl-q4y-episodic-strategy.md)。基线 main `40edfe03633ce54db2f903a5baf2123ff6b011f6`，分支 `feat/nosl-q4y-episodic-strategy`；学习源码 `c1ac44450a5eb89348543750a973b5b2d2c52d05`，独立结果核验工具 `d6c188079e386e9bbc35b5e6488a2bcfbf317fbb`。产物 `artifacts/nosl-q4y/`。

## 实现与信息边界

新增独立局外 actor/critic，复制冻结 X1 后拥有完全独立的参数存储。战斗及战斗内 Choice 全部使用原始 X1 贪心推理；局外采样仅更新战略模型。公开 global[11] 决定路由，两模型只读既有 global/tokens/actions/counts，不读取种子、RNG或隐藏未来，不调用在线搜索或试动作。

每批固定32局，在同一未更新策略下从正常新局到真实终点。多合法动作的局外选择保留无损 int32 公开观察、动作、采样 logp 和 value。使用既有楼层/HP势/Boss已观察伤害/胜利奖励，gamma1，后续实际奖励求和至终局；不按中间战斗出牌次数截断，不 bootstrap 未完成终局。故障或截止的整局行保留，全部学习样本排除，不伪造死亡。

两学习seed1/3各登记128批×32局、2epochs、minibatch256、LR.000133、clip.2、entropy.05、value系数.5、grad norm.5；固定终点权重才能评价。战略checkpoint必须与冻结战斗X1及公开路由一起使用，不能把它单独加载为全阶段策略。官方X1 SHA仍为 `6fb5f1955a11d55175d348659460ce459017baa018f7cf9242a671ba19266703`。

## 已完成资格

```bash
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/check_episodic_strategy.py --out artifacts/nosl-q4y/qualification
taskset -c 4 nice -n 19 env PYTHONPATH=python .venv/bin/python -B tools/nosl/oracle_replay.py \
  --trajectories artifacts/nosl-q4y/qualification/pilot-trajectories.jsonl \
  --out artifacts/nosl-q4y/pilot-normal --jobs 1
taskset -c 4 nice -n 19 env PYTHONPATH=python .venv/bin/python -B tools/nosl/oracle_replay.py \
  --trajectories artifacts/nosl-q4y/qualification/updated-trajectories.jsonl \
  --out artifacts/nosl-q4y/updated-normal --jobs 1
taskset -c 4,5 nice -n 19 env STS2_BUILD_JOBS=2 scripts/check.sh
```

- CPU独立开发8局，1996个策略决策/390局外学习行，采样1.973秒；实际奖励suffix与独立逐项求和一致，公开表均int32，初始双模型路由与X1一致。
- 更新后实际1604战斗观察/133战斗Choice、493局外观察；战斗全部参数和每个logit/value严格不变，合法mask保持。4个真实优化step，战略参数改变、梯度有限。
- 8条开发正常规则整局/2478决策及2条更新后混合策略整局/329决策全部与正版一致，实际TestMode=false、终点一致。共2807决策，不能据此宣称全部新策略分布均已资格。
- 实际截止的两个未完成局已有170决策/31战略行；全部MC学习行排除，状态仍为running，未伪装terminal。证据 `partial-return-proof.json`。
- 完整 `scripts/check.sh` 在学习源码c1ac444上通过，60 CTest、L2的200组合及脚本要求的L3/L4、快照与终局路径，末行 `all checks passed`。后续只新增独立离线checker，未修改学习/运行时代码。
- 云端CUDA定向资格通过：8局1505决策/301学习行；实际1404战斗观察/132战斗Choice的输出不变，4个优化step有限。CPU/CUDA随机采样结果不同，分别记录，不声称两设备采样轨迹一致。

## 正式试验与资源状态

云运行命令及只读本地复核：

```bash
.venv/bin/python -B tools/nosl/episodic_strategy_experiment.py run \
  --checkpoint artifacts/nosl-q4u/start.pt --out artifacts/nosl-q4y/experiment --device cuda
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/verify_episodic_strategy.py \
  --checkpoint artifacts/nosl-q4r/start.pt --experiment artifacts/nosl-q4y/cloud/experiment \
  --out artifacts/nosl-q4y/verified-results.json
python3 artifacts/nosl-q4y/final_stats.py
```

| 学习seed | 更新/尝试 | 新策略决策 | MC局外学习行 | 优化step | 真实死亡/截断 | 训练秒数 |
|---|---|---:|---:|---:|---|---:|
| S1 | 128/4096 | 855276 | 171816 | 1476 | 4096/0 | 967.8842 |
| S3 | 128/4096 | 868060 | 169386 | 1454 | 4095/1 | 1050.2383 |

共1723336个实际新策略决策、341202条MC战略学习行、2930个真实优化step。S1所有171816局外选择均为可信终局行；S3实际169414局外选择中，故障局28行全部排除。两组从独立X1拷贝和新Adam开始，未沿用第一组权重训练第二组。真实终局采样的训练均值16.2937/16.2217层、第二幕730/765、第三幕0/1、胜利0/0；这些为训练期随机局外选择，不和贪心评价直接比较。

本地独立checker逐条核对固定seed与128×32预算、全部失败/terminal语义、历史/路由与计数、配置一致、模型有限性与hash、战斗参数冻结，并重新计算全部配对统计与继续判据，返回independent_verification=true。两组战斗参数digest均为 `6996897c9904d11036f9c89ec00866b05981753e7a2a05a97cf35772ee4f8080`。正式候选文件SHA：S1 `c8e5458a68c3c716ef1827f76df4cb85db0f5f5b54df024d7c81c8792b7f6ba4`，S3 `91f7c1c5d97153fc5d8077479590248d76bcebf2f74aaea47a5559f01eaa6314`；均只可作为独立局外模型和冻结X1路由使用。

固定评价使用NQ4YEVAL000000–001023，三策略完全相同1024种子、CUDA float32、64槽、贪心，只评价登记最终模型。全部3072尝试均真实死亡、0故障/截断/排除，未换种子、补局或挑中途checkpoint。

| 策略 | 平均层数 | 第二幕 | 第三幕 | 真实通关 |
|---|---:|---|---|---|
| X1 | 18.396484 | 322/1024 (31.45%) | 0/1024 | 0/1024 |
| S1 | 19.265625 | 392/1024 (38.28%) | 1/1024 | 0/1024 |
| S3 | 18.717773 | 328/1024 (32.03%) | 0/1024 | 0/1024 |

| 配对候选−X1 | 平均层数差 | 95%配对bootstrap区间 | 单臂继续门槛 |
|---|---:|---|---|
| S1 | +0.869141 | [+0.423828,+1.314502] | 通过 |
| S3 | +0.321289 | [−0.122095,+0.751953] | 失败 |

区间以同一个跑局seed为样本单位，10000次配对重抽、固定rng20261005，本地独立复算与云结果相同。S3既未达到+.5层/下界>0，也有1条训练步数截断，故双seed门槛失败。S1第三幕进入数达到本批X1，但不抵消第二组失败；不事后将“至少一组有效”改作继续标准。程序job.exit=1是结果文件写入后的预设门槛失败返回值，非训练崩溃；全部登记预算与评价已完成。此tune结果也不作为独立最终留出测试。

正式采样源码c1ac444，原生Env源码未变，云实际动态库SHA `dff862e8328998f3262d65a24b1355b434504f39cccd216a20c6e94921e9cb18`。Python3.12.3、Torch2.8.0+cu128、NumPy2.3.2、CUDA12.8，RTX4080SUPER可见显存33794359296字节；Torch/OMP/MKL/OpenBLAS均1线程，训练/评价float32。

实例pro-7912450e5767启动前API确认shutdown，独立6000秒保护先确认live再开机。原截止 **2026-10-06 04:27:27 UTC** 不延长；每臂1800秒、评价1200秒，整个driver外层4800秒、部署外层5400秒。凭据只在本机读取，未上传。独立本地回收进程等待实际云launcher终止，归档取回后校验全部文件，再停机；API确认shutdown才解除保护。

正式launcher1279已结束后，由已登记本地回收进程取回归档。39个文件、原始22468237字节、压缩11035529字节，归档SHA `691e35e124d49d478887faa2db129b8b33626c5d3c36e2eb9eb64a96ce165e21`；全部文件大小和SHA通过，retrieval_verified=true。随后API在 **2026-10-06 03:34:46.929796 UTC** 返回Success/shutdown，才写disarm；回收PID1261629和保护PID1257793均已退出，保护日志记录disarmed_after_verified_shutdown。

开机指令记录02:47:29.502414UTC，至关机确认47分17.427秒。资源262次采样覆盖02:49:27.006300–03:33:19.468704UTC（43分52.462秒，含资格/训练/评价）；driver及子进程RSS峰值1624.85MiB，显存峰值1907MiB，采样GPU利用率峰值64%/均值8.48%。训练总2018.123秒、实际总约854决策/秒；含所有局外/战斗决策与故障长循环，不能当纯推理或仅可信MC样本的吞吐。未独立记录精确评价计时，不将剩余资源跨度全归因于评价，也不把这些时刻换算为未取得的平台账单。

旧Q4-v/w/x的424/147/2356个冻结文件及manifest SHA已逐个复核不变，记录 `previous-evidence-verification.json`。官方X1与原生Env未替换，其他chat的工作树修改保留。本配置不按结果追加预算；下一项应围绕已实际复现的战斗药水/收尾信号开展独立小实验，再决定如何复现局外正向结果。

## 登记候选的正常规则整局资格

评价冻结后只回放每组的前2个seed及最深真实终点，未重采样策略或更换动作：

```bash
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/oracle_replay.py \
  --trajectories artifacts/nosl-q4y/cloud/experiment/S1-normal-inputs.jsonl \
  --out artifacts/nosl-q4y/S1-normal --jobs 1 --oracle-test-mode off
# 同一命令将S1改成S3，串行运行第二组。
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B artifacts/nosl-q4y/verify_followup.py
```

| 候选/seed尾号 | 实际终点 | 正版决策匹配 |
|---|---|---|
| S1/000000 | 第二幕20层死亡 | 243/243 |
| S1/000001 | 第一幕17层死亡 | 243/243 |
| S1/000340 | 第三幕48层死亡 | 788/788 |
| S3/000000 | 第二幕21层死亡 | 284/284 |
| S3/000001 | 第一幕17层死亡 | 267/267 |
| S3/000030 | 第二幕33层死亡 | 464/464 |

6/6实际TestMode=false、正常HP/牌组/遗物初始条件、脚本complete、整局真实死亡、L4 identical。另一次独立只读核验按预设选择规则从全部评价重新确认这6条输入完全相同，再全部开启verify_snapshots重放：6/6完整合格、2289/2289决策、全部end.run字段一致，记录 `followup-verification.json`。S1最深路线确实到第三幕48层，但仅1/1024进入第三幕、0通关，不当作胜利或稳定深层表现；S3最深仍为第二幕33层。

本项新路径的完整原版证据共开发8条+更新后2条+最终候选6条=16条整局/5096决策，另有上述故障的511决策前缀。不得把原生评价中的mc_qualified/真实terminal字段解释成全部3072条均经正版验证。

## 新分布的战斗停滞诊断

S3实际训练种子 `NQ4YTRAIN3002718` 在第一幕12层遇到 Haunted Ship，达到原生30000步限制；历史含20009个策略决策（28局外/19981战斗），结束仍为step_limit。该局28条局外候选行全部排除MC学习，reward_sum为null；没有改成死亡、补种子、延长预算或改变当前训练。

冻结历史在本地正常新局重放到相同的30000步/20009决策，玩家70/76HP、敌人3/67HP。公开快照显示后期手牌循环为Survivor、3张Defend与Ricochet；基本攻击已在消耗堆。相同公开观察送入冻结X1，8个登记快照的CPU argmax全部与云历史相同。第200个决策Ricochet合法，但Survivor概率97.60%、Ricochet0.15%；后期快照Survivor约93.55%–99.15%，不能将抽样快照扩称全部动作的概率统计。

原版正常规则前缀进一步确认：战斗开始的Glowwater Potion使用前，手牌含Backstab、Ascender's Bane、4张Strike、Neutralize和2张Defend；药水使用后，这9张全进入消耗堆，手中只剩Survivor、3张Defend和Ricochet。`Core/Models/Potions/GlowwaterPotion.cs` 的真值是先消耗当前整手，再抽10张；`Core/Models/Cards/Ricochet.cs` 为2费、3伤×4、随机敌人攻击，带Sly。该局药水选择与后续收尾选择都有实际策略弱点，不归因于Precise Scissors的不存在效果，也不据单局证明所有退步首因。

原版前511个脚本动作以稳定end_turn边界结束，实际TestMode=false，逐决策/快照511/511一致、end.run全部字段一致，正版第82回合也为玩家70HP/敌人3HP。证据 `step-limit-prefix511-verified.json` 的scope仅normal_combat_prefix_only；L4状态保持truncated，不能计作完整终局资格或全部30000步的正版验证。未改原L4判据。

完整脚本转换因最后停在选牌中而inexpressible。先尝试的512动作静态前缀也停在Survivor所需的card_select内，原版明确error；这一失败保留，未解释成规则分歧或终局。511前缀第一次本地CLI路径写错而启动失败的proof也保留，只有正确`build/sts2sim_cli`的实际回放计入上述511动作证据；空样本辅助解析不计资格。所有诊断只消费冻结历史/公开观察，未重新采样策略、运行教师搜索或干预当前实验。

## 最终封存

所有训练/评价、取回、API停机、正常回放和快照核验进程均已结束后，于2026-10-06 03:40:29.704907UTC冻结本地247个文件、51455916字节。`artifacts/nosl-q4y/manifest.json` SHA为 `93db54aa2c87e5a805044409f613accf68b908c5f4b727d5afbb04f1ed31f83e`；包含所有失败、有限前缀、完整候选结果、原始云日志和独立复核，不修改先前结论或记录。当前本地原生库SHA仍为 `6296b12846d035ba598cb3beea10d2eaa649c241c468136cf8d51e67a099e231`。
