# Q4-z：独立战斗终结回报学习

**两组遭遇回报实验均低于原X1，继续门槛失败，不扩训本配置、不替换正式模型。** 五策略各1024个相同新tune，全部5120次真实死亡、0故障/截断、0通关；R1/R3比X1低2.046/2.431层，配对区间全在零以下。11条最终候选正常规则整局、4675个正版决策和所有快照/终点一致；归档全部文件已取回验证，API已确认实例shutdown。高水平Silent A10整局/Linux NoSL仍未达到。

计划：[Q4-z](../plans/2026-10-05-nosl-q4z-encounter-credit.md)。基线main1022c676a612e06056bfcc6bb756529482f03887，分支feat/nosl-q4z-encounter-credit，云端学习/资格源码b70b5a6ab8c46a251038ac199729d5f9a8eb0053，本地独立最终资格/结果工具d639746/f1163fc，产物artifacts/nosl-q4z。

## 实现与已完成资格

复制X1为战斗actor/critic，局外X1独立存储并冻结；公开global[11]决定路由。C只对战斗多合法选择用现有整局实际回报，R对相同选择用实际遭遇结束、HP变化和EndTurn成本；actor只接收公开四表，不读取奖励日志、种子或隐藏未来，不运行搜索。R的“存活退出”包含原版逃离，不称战斗击杀/整局胜利。两种学习器均先完成一批32个新正常整局再更新，所有非terminal整局行全部排除。

```bash
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/check_encounter_credit.py --out artifacts/nosl-q4z/qualification2
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/oracle_replay.py \
  --trajectories artifacts/nosl-q4z/qualification2/pilot-trajectories.jsonl \
  --out artifacts/nosl-q4z/pilot-normal --jobs 1 --oracle-test-mode off
# 同样依次回放qualification2/C-updated-trajectories.jsonl、R-updated-trajectories.jsonl。
taskset -c 4,5 nice -n 19 env STS2_BUILD_JOBS=2 scripts/check.sh
```

- 实际8个新正常开发局、1733策略决策，每臂1343条战斗学习行；C/R初批全部动作与旧回报序列相同。R每行按真实遭遇末HP、剩余实际EndTurn数量、真实边界奖金的独立公式核对；C逐项后续奖励求和核对。公开表int32，初始路由输出与X1严格相同。
- 两臂各12个真实优化step、有限梯度，战斗参数确实改变。更新后实际C397/R371个局外观察的每个logit/value严格保持冻结X1输出，1420/1259个真实战斗观察仍走新actor，合法mask保持，局外所有参数摘要不变。
- 修正后本地采样C1.695/R1.626秒，更新C4.299/R4.152秒，仅是8局资格成本；不按CPU数字冒充云吞吐。初批approx_kl C.1211/R.0997，明显高于目标.01；既定自适应将后续LR降低，没有按资格结果另改登记超参。正式日志另列后文。
- 4个正常新局以实际80步上限作非terminal资格输入，存在真实战斗行；所有故障局行全部排除，包括其中已经结束的遭遇，不伪造死亡或bootstrap。证据qualification2/partial-outcomes.json。
- 第一资格尝试的计算/路由证据通过，但原型smoke checkpoint把参数digest写进outside_checkpoint_sha256，元数据不合格。该输出全部保留于qualification与qualification-metadata-mismatch.log，不用于正式学习。修正为实际X1文件SHA并另存outside参数摘要，新qualification2实际load回模型核对摘要及元数据，全部通过；未改旧记录或按策略结果挑选资格尝试。
- 8开发轨迹、C更新后2条、R更新后2条共12条正常TestMode=false整局全部identical，分别2160/517/518个原版决策。独立开启verify_snapshots再重放：12/12完整合格、3195/3195决策、所有end.run字段一致，证据qualification-normal-verification.json。仅证明这些实际路径，不扩称全部未来策略分布。

## 完整门禁与云端运行

学习源码b70b5a6上的完整scripts/check.sh已退出0，日志check.log末行all checks passed，60 CTest与脚本要求的L2/L3/L4、快照、跨幕/完整终点全部通过。之后未改变学习/公共运行时代码；本地新增的只读核验工具另在全部正式数据和登记轨迹上实际通过。

云API于2026-10-06 03:59:48.299825UTC确认shutdown。先启动并确认独立保护PID1278009，起点04:07:13.776914UTC、截止 **05:47:13.776914UTC**（6000秒，不延长）；04:07:15.708004UTC API接受开机指令。新SSH实际旧HEAD为c1ac444，RTX4080SUPER/32760MiB，缓存源码干净且Q4-z目录不存在。7文件34423字节增量payload另加manifest上传，逐文件SHA/大小、bundle与原生来源差异检查通过，再切到b70b5a6；X1仍为6fb5f195...266703，云原生库仍dff862e8...9cb18。

实际Python3.12.3、Torch2.8.0+cu128、NumPy2.3.2、CUDA12.8，可见显存33794359296字节，float32、Torch/OMP/MKL/OpenBLAS各1线程。CUDA资格通过：初批8局1542决策/1202行，C/R动作一致、独立公式/模型元数据/冻结局外/合法mask/实际非terminal排除全部通过；各10优化step有限。更新后C386/R370实际局外观察输出严格不变，1390/1338实际战斗观察走更新actor。CPU/CUDA随机轨迹不同，分别保留，不称跨设备轨迹一致。

正式launcher **1295** 为独立5400秒timeout进程，driver另有5100秒上限；四臂各900秒，固定评价总1200秒，均未延长。四臂全部完成各64更新/2048正常新局，全部8192次真实死亡、0故障、0胜。记录如下：

| 臂 | 新策略决策 | 战斗MC行 | 优化step | 训练秒数 | 训练均值层数 | 第二/第三幕 |
|---|---:|---:|---:|---:|---:|---|
| C1 | 389652 | 304212 | 2448 | 487.986 | 15.777832 | 318/2 |
| R1 | 339471 | 264410 | 2134 | 428.651 | 13.943359 | 254/0 |
| C3 | 398720 | 312941 | 2506 | 491.625 | 15.875000 | 309/2 |
| R3 | 356302 | 278055 | 2238 | 448.029 | 14.568359 | 250/0 |

共1484145个新策略决策、1159618条可信战斗学习行、9326个真实优化step；每个seed的C/R均从独立X1副本与新Adam开始，不继承前一臂训练权重。训练期战斗随机选择、局外冻结贪心，不能和最终全贪心评价混作同一强度指标。实际summary与final-stats.json保存全部计数。

本地自动回收PID1278495实际确认launcher终止后归档，但首次SCP在04:59:32.631443UTC发生180秒TimeoutExpired，自动重新打包并重试。仅在确认这一真实传输失败后接管；首次attention状态断言遇自动重试而未执行任何动作，随后明确停止旧回收及重试SCP子进程，保留当时8878080字节部分归档与失败日志，manual-recovery-control.json。

替代独立取回PID1285485先核对新归档整体SHA和云/本地前缀SHA完全相同，再从8878080字节处按SSH流续传；前缀SHA为5e37319810b0ce06a57c2cd9551823ee6ae9e4df34dc7d2c4283382cadf296af。仅改变失败的传输方式，未重新训练、评价或延长原关机期限。最终55文件/136366370原始字节、29864190压缩字节，归档SHA **78e5da1865d1a05ee4ccfb5fd83132fa744d1f644cf7c0e41a3ae9f2b80d4b92**；整体及每个文件SHA/大小全部通过，retrieval-verification.log。原始job.exit=1存在，未编造部署成功或缺失退出记录。

验证取回后立即power_off，API在 **2026-10-06 05:07:28.814726UTC** 返回Success/shutdown，之后才解除原保护。旧回收、替代取回、PID1278009保护和云launcher均已结束，保护日志为disarmed_after_verified_shutdown。开机指令至确认关机3613.106722秒（60分13.107秒），包括资格、训练、评价与较慢取回；不是取得的平台账单。

独立结果checker已逐条审计全部实际HP/动作kind/遭遇边界与MC标签、失败排除、固定预算、冻结参数、最终模型scope/hash和四项配对bootstrap，返回independent_verification=true。两个seed的C/R首批各32条实际轨迹除不同标签之外全字段相同，模型只能与冻结局外X1路由配合。C1/R1先前首批32轨迹及5955行/臂的限定早期证据也保留，early-completed-arms-proof.json；最终verified-results.json覆盖全记录。已取回结果与先前只读观察的live-final-results、completed-training-summaries完全相同。

另对相同首批32条冻结训练动作作CPU只读诊断：7537实际决策、5955战斗学习行的动作kind/HP前后/原生终点全部重放一致，未重采样策略。旧X1 value均值.13594；C标签均值.19215、对旧value RMSE .18853，R标签均值.57472、RMSE .89362（约4.74倍）。这是initial-value-audit.json限定的首批新回报校准线索，不是最终策略评价、原版全部资格或共享critic导致退步的因果证明。四项旧Q4-v/w/x/y冻结manifest及全部3174文件已逐一复核不变，previous-evidence-verification.json。

## 固定评价与独立复核结果

五策略各1024个相同NQ4ZEVAL000000–001023新tune，贪心、CUDA float32、64槽，只比较登记最终模型，全部5120次真实死亡、0故障/截断/排除、0通关；未补局、换种子或挑中途checkpoint。job.exit=1是在写出完整结果后的预设门槛失败返回值，不是训练崩溃。配对区间以跑局seed为样本单位、10000重抽、固定rng20261005；本地全记录独立重算全部一致。

| 策略 | 平均层数 | 第二幕 | 第三幕 | 真实通关 |
|---|---:|---|---|---|
| X1 | 18.253906 | 286/1024 | 1/1024 | 0/1024 |
| C1 | 15.152344 | 133/1024 | 0/1024 | 0/1024 |
| R1 | 16.208008 | 180/1024 | 0/1024 | 0/1024 |
| C3 | 16.537109 | 185/1024 | 2/1024 | 0/1024 |
| R3 | 15.823242 | 171/1024 | 1/1024 | 0/1024 |

| 配对实验−参照 | 平均层数差 | 95%配对bootstrap区间 | 登记门槛 |
|---|---:|---|---|
| R1−X1 | −2.045898 | [−2.399414,−1.695313] | 失败 |
| R1−C1 | +1.055664 | [+.732422,+1.376001] | 通过 |
| R3−X1 | −2.430664 | [−2.795898,−2.075195] | 失败 |
| R3−C3 | −.713867 | [−1.047852,−.380859] | 失败 |

两个实验臂均低于原X1；相对整局回报对照的正向结果也未跨学习seed复现。R3第三幕数达到本批X1，不改变主门槛失败。故本配置不追加预算、不替换X1；本批tune不是最终独立留出测试，高水平整局/Linux NoSL仍未达到。

## 登记候选正常规则资格

评价冻结后按每候选前2条和最深真实终点选取，重合不重复；R1最深已是第一个种子，故共11条。verify_encounter_followups.py独立重新核对选择输入与全部评价记录，然后逐条冻结动作回放，不重采样策略。

| 臂/seed尾号 | 实际终点 | 原版决策匹配 |
|---|---|---|
| C1/000000 | 第一幕17层死亡 | 333/333 |
| C1/000001 | 第一幕6层死亡 | 113/113 |
| C1/000144 | 第二幕33层死亡 | 463/463 |
| R1/000000 | 第二幕33层死亡 | 583/583 |
| R1/000001 | 第一幕6层死亡 | 110/110 |
| C3/000000 | 第一幕17层死亡 | 389/389 |
| C3/000001 | 第一幕17层死亡 | 347/347 |
| C3/000744 | 第三幕44层死亡 | 787/787 |
| R3/000000 | 第二幕33层死亡 | 601/601 |
| R3/000001 | 第一幕7层死亡 | 137/137 |
| R3/000623 | 第三幕48层死亡 | 812/812 |

11/11实际TestMode=false、无HP/牌组/遗物注入、script complete，严格原L4/逐决策快照、全部end.run字段相同，4675/4675决策，0失败。加上开发12条/3195决策，本项共23条完整正常规则/7870决策，不扩大为全部5120条正版资格；没有正式胜利路径或Linux实机部署验收。

实际复核命令：

```bash
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/verify_encounter_credit.py \
  --checkpoint artifacts/nosl-q4r/start.pt --experiment artifacts/nosl-q4z/cloud/experiment \
  --out artifacts/nosl-q4z/verified-results.json
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/verify_encounter_followups.py \
  --experiment artifacts/nosl-q4z/cloud/experiment --out artifacts/nosl-q4z/final-normal
taskset -c 4 nice -n 19 python3 artifacts/nosl-q4z/final_stats.py
```

## 成本、判断与封存

资源275次采样覆盖04:09:45.996722–04:55:49.926394UTC，跨度2763.929672秒，含资格/训练/评价。driver及子进程采样RSS峰值1787.0625MiB、显存峰值1041MiB、GPU利用率峰值37%/均值9.484%；不是连续监控的绝对上界。四臂记录训练wall总1856.291秒，实际约799.522新策略决策/秒，含局外/战斗和优化，不当作纯Env或纯推理吞吐。没有独立记录精确评价耗时，不以资源跨度减训练时间冒充评价成本。

首更新approx_kl C1/R1/C3/R3为.09095/.09013/.08932/.06523，结束记录的熵为1.147/1.326/1.079/1.321（首更新.641/.619/.646/.632）。这些与旧value对新R目标误差的首批诊断，提示先检验策略更新漂移、actor/critic梯度耦合与价值校准；不是因果证明，不以加长本失败配置代替独立实验。后续先在冻结训练样本上分解梯度并限定策略变化，再登记新正常整局对照。整局胜率和Linux实机NoSL目标保持不变。

官方X1 SHA与本地原生库SHA仍分别为6fb5f195...266703、6296b128...9e231，unchanged-runtime-and-x1.json；游戏规则与旧Q4-v/w/x/y全部3174冻结文件不变。所有云/本地训练、取回、保护和正常回放进程已结束后，于 **2026-10-06 05:14:43.078352UTC** 冻结本项372个文件、213355702字节，并逐一核对SHA/大小。`artifacts/nosl-q4z/manifest.json` SHA为 **e0c4989f5a9bd1a6af2ad3e9f158629f7a9678ea81864a05b5443d7c255b28f2**，包含失败资格、传输超时/部分归档/续传、原始训练评价与完整最终资格证据。

README/路线图仅将本项新增前缀写入index；去除新增前缀后工作树原字节相同，其他chat未提交的性能段落、文档和既有CLAUDE.md删除均保留，foreign-worktree-preservation.json与foreign-files-preservation.json。不把记录完成解释成高水平AI完成。
