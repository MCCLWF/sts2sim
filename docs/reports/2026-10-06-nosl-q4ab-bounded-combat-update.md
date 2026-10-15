# Q4-ab：有界战斗策略更新

**已发布的5120局固定评价全部真实死亡、0通关。完整训练的B1比原更新U1高.6455层，但比X1低1.2803层；B3训练截断，仅保存17批模型。本项未满足扩训门槛，不扩大当前配置、不替换X1。** 原始云文件已完整取回核验，最终独立审计及12条候选正常Oracle整局回放已完成，实例已确认关机；高水平Silent A10整局/Linux NoSL目标保持未完成。

计划：[Q4-ab](../plans/2026-10-06-nosl-q4ab-bounded-combat-update.md)。基线main 7bfc45f1bac137760e1b1159dc2acff6540fe299，分支feat/nosl-q4ab-bounded-combat-update，学习实现511f838、最终Oracle适配/门禁源码76f28f7；产物artifacts/nosl-q4ab。原版v0.111.0、全解锁正常Silent A10、公开features3/filter1、正式X1 SHA 6fb5f1955a11d55175d348659460ce459017baa018f7cf9242a671ba19266703不变。

## 实现与已完成资格

U是原episodic_strategy.update的只读指标包装；B初始/最高LR .000133/4、最低.000133/128，其他损失/两epochs/256批次/实际整局MC回报保持相同。训练仅更新战斗，局外独立冻结X1。B一次候选更新后在全部可信战斗多合法行上测old||new完整分布平均KL，≤.01才接受；超限恢复全部actor和Adam状态，下一批LR减半，不重跑本批。接受后的下批LR按计划登记阈值调整；只约束本批均值，不宣称每个状态或未来分布都有上界。

- Q4-aa实际冻结C1/C3首批5955/4899行上，U与原更新函数的参数和完整Adam状态严格相同。B首候选精确KL分别.021278/.021560，均正常拒绝，actor/Adam完整恢复；另在已有Adam moments/step的U上用固定极小cap验证拒绝，恢复通过。
- 8条新正常资格局、1312条战斗学习行，两臂首次原始历史/动作/回报完全相同，C标签独立后缀公式逐项一致。U实际12步、KL .131956；B实际12个候选步、KL .018080，拒绝后保留0步、KL0，不把计算过的步数冒充保留学习进展。公开int32、合法mask、局外参数及实际输出、保存模型元数据均通过。
- 两臂资格期间实际检查U局外477/战斗1683行、B局外477/战斗1686行。4个真实80步非terminal样本全部按结束类型排除，不伪造terminal。原资格用时291.379853秒。
- 补充正式cap检查使用拒绝后规则规定的LR .000133/8和另一批已冻结真实训练数据：完整平均KL **.008652141556**，实际接受且参数改变；其后2条新正常贪心路径保留。固定宽cap1另覆盖正向接受分支，不改正式配置。注入第二个真实优化step之后的宿主优化器异常，中途参数已改变，异常后参数和完整Adam状态严格恢复。补充用时37.823495秒；原资格加补充329.203349秒，低于480秒登记额度，未扩正式训练预算。

```bash
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/check_bounded_combat.py --out artifacts/nosl-q4ab/qualification
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B artifacts/nosl-q4ab/qualify_acceptance.py
```

两命令实际退出0。资格没有按改善胜率挑权重或更换种子；固定宽cap和异常注入只验证优化器控制流，不是正式策略配置或改游戏规则。

## 正常整局资格的原始失败与适配修复

第一轮14条回放实际12条通过、3185个匹配决策，2条明确失败，全部保留于qualification-normal：

1. NQ4ABPILOT000007在第二幕DARV选择Pandora后异步NullReferenceException。原版PandorasBox.AfterObtained已完成原CardCmd.Transform、获得Hook及Niche抽样，随后唯一的NSimpleCardsViewScreen.ShowScreen尝试创建纯检查界面。Headless仅跳过此查看工厂并返回调用方接受的null，不跳过变牌或吞任务异常，不启用TestMode。旧Q4-o也曾记录此未无头化界面，本项实际补齐。
2. NQ4ABSUPPUPD000000带仍有次数的WingedBoots到普通地图最后一行后，旧OracleRun直接用MapTravel.GetPointsInRow漏掉网格外Boss，拒绝正常Boss动作。原版NMapScreen.RecalculateTravelability明确先处理第二Boss/最后普通行→Boss，再调用MapTravel；输入适配按这个原始UI优先级补齐，不改DLL、原生Env或合法动作规则。

修复后对**相同14条冻结动作**另目录qualification-normal-fixed重放：14/14完整正常整局、3841/3841正版决策、所有快照和全部end.run字段相同，TestMode=false、无HP/牌组/遗物注入。没有改L4比较、补种子或丢弃失败。两条原始正常动作加入tests/l4/check_nosl_oracle_adapters.py并接入scripts/check.sh；坑点和源码依据写入pitfalls。

```bash
taskset -c 4,5 nice -n 19 /home/wst/.dotnet/dotnet build tools/Sts2Oracle -c Release
taskset -c 4 nice -n 19 env PYTHONPATH=python \
  .venv/bin/python -B artifacts/nosl-q4ab/oracle_qualification.py
taskset -c 4 nice -n 19 env PYTHONPATH=python \
  .venv/bin/python -B artifacts/nosl-q4ab/oracle_qualification_fixed.py
taskset -c 4,5 nice -n 19 env STS2_BUILD_JOBS=2 scripts/check.sh
```

Oracle build实际退出0、0错误；原回放退出1，修复后回放退出0。76f28f7上的完整scripts/check.sh实际退出0，check.log末行all checks passed；含新增2条完整正常A10/656决策及快照回归，不扩大为全部策略分布资格。

## 正式实验执行与故障记录

登记U1/B1/U3/B3各64次更新尝试×32个正常新局，NQ4ABTRAIN匹配种子；B至少48次接受并保持每次保留均值KL≤.01才满足机制门槛。X1及四个最终候选各1024个相同新tune NQ4ABEVAL，完整贪心整局，以跑局为统计单位；两个B均须比X1及对应U高至少.5层、95%配对bootstrap区间下界>0，且所有训练/评价完整无故障，才扩训。不按首候选拒绝改变初始LR或cap。

完整门禁通过后，API于2026-10-06 09:51:52.669654UTC再次确认shutdown；独立保护PID1314949在09:52:20.355688UTC启动并确认armed，原截止 **11:32:20.355688UTC**（6000秒，不延长）。09:52:22.265378UTC接受开机指令。SSH实际旧HEAD b70b5a6、tracked源码干净且新实验目录不存在；7文件57137原始字节/53823压缩字节payload逐项SHA/大小检查，通过bundle切至76f28f7bc809481df47fae289cf286d568a2f4f8。云原生源不变，库SHA仍dff862e8328998f3262d65a24b1355b434504f39cccd216a20c6e94921e9cb18，X1相同；不上传用户凭据。

实际RTX4080SUPER/33794359296可见显存字节、Python3.12.3/Torch2.8.0+cu128/NumPy2.3.2/CUDA12.8，float32、driver各1线程。CUDA正常资格24.425040秒通过：8局/1270可信战斗行、两臂首次历史相同、标签/公开表/合法mask/局外实际输出与元数据/真实非terminal排除正确；各10个候选优化step。U精确KL .128007、B .026770，B拒绝后参数/Adam严格恢复并保留0步；CPU/CUDA动作不同，各自保留，不称逐位跨设备复现。

训练运行期间只读取已经关闭的5个CUDA资格文件，16311压缩字节；归档及每文件SHA/大小验证后保存到本地cuda-qualification。另对其8初始/2 U/2 B轨迹进行完整正常Oracle/逐决策快照回放：**12/12、3285/3285决策**、全部end.run字段一致，cuda-qualification-normal/summary.json。与本地资格合计26次完整回放执行/7126匹配决策；共享种子及重复路径不记作26个独立强度样本，不扩大为正式8192训练/5120评价的全部正版资格。

云launcher **1261**独立5400秒timeout、driver5100秒，四臂各900秒/评价1200秒；自动回收PID **1315204**读取真实launcher终止后只打包一次，并逐次校验已下载前缀后有界续传，全部归档及文件验证后立即关机，确认shutdown才解除原保护。09:57:02.866684UTC实际launcher alive、job.exit不存在，U1已13更新/416局/85736决策/0故障；这是早期训练状态，不是最终强度评价。这是当时运行状态；最终终止、回收与封存见末节。

后续10:07:19.743775UTC只读观察实际launcher仍alive、job.exit仍缺失：U1已完整64更新/2048局/374707决策/291387战斗行/2338优化step/0故障，summary wall613.413025秒；B1已13次尝试/416局/90479决策/0故障，12接受/1拒绝，全部保留更新的平均KL最大.008402597827、最新.003861184241。仅是已发布训练日志与U1终止summary，当时全部原始数据/权重独立审计与固定强度评价尚未完成，最终核验见末节。

随后对已经关闭的U1目录做单次远端CPU只读独立早期审计：2048条原始C标签/完整动作与回报/计数预算、64更新和2338实际步数、最终actor/Adam文件重新加载后的完整digest及LR均一致，模型权重有限；U1/B1首批32条实际历史与标签全部相同。closed-U1-audit.json限定scope为本臂原始标签/预算/保存权重与Adam roundtrip/首批匹配，不代替四臂最终审计或第二次完整KL前向重算。U1 checkpoint SHA d93438b9adda4e1396413775d22ebe707f504037744ae730662aeb2e66864893，optimizer SHA 7be0111b49547ad4cba55c48ecf8e45c8223f5e9270354f64be9c451da14795a；未重新采样、训练、评价或改变活动云任务。

10:21:34.336866UTC只读观察实际launcher仍alive、job.exit缺失：B1已经完整64次尝试/2048局/413687决策/323158战斗行/0故障，63接受/1拒绝；实际候选2590优化step，撤回44步后保留2546步，wall645.332658秒、最终LR .000016625，保留平均KL最大.008402597827。U3已35次更新/1120局/0故障；没有更改或延长云预算。

B1关闭后，单次远端CPU只读审计实际退出0，closed-B1-audit.json记录：2048条C原始标签/完整历史、预算、保存actor/完整Adam roundtrip、U1/B1首次32局完全相同，以及全部64次已记录接受规则/LR/前候选后参数和Adam摘要链均正确；机制门槛通过。B1 checkpoint SHA 8b12163f1eef13f5befa7b56e6a2e4c30494f6f7788efe2a1a2ac3d88e3563c3，optimizer SHA 508f4c98999b96266109b2d557950ebcd71a0ca380f3ff4c36717e50364b75d5。此审计使用记录的KL，不是第二次完整前向重算；仅证明B1机制、文件与原始数据一致，不证明整局变强或覆盖尚未结束的另一seed。

10:26:31.769116UTC实际launcher仍alive、job.exit缺失：U3已完整64更新/2048局/393916决策/307118战斗行/2466实际优化step/0故障，wall600.328320秒；B3已5尝试/160局/0故障，3接受/2拒绝。10:27:23.543809UTC另只读核对已经发布的U3/B3首批：32条真实历史全部字段和标签完全相同，共7011决策/5432战斗行，规范JSON SHA c435da2239b2adddcbce862e6ab4a4d8370fd250af914d62319b5b19b85e01e4；matched-first-U3-B3.json保存限定scope，不重新采样或更新模型。

10:33:42.339155UTC实际launcher仍alive、job.exit缺失，X1固定评价已有448真实terminal。B3已停止：17尝试/15接受/2拒绝，累计576局尝试/1故障/157429决策/93981保留战斗行/750候选优化step/660保留step，wall391.439875秒、最终LR .00001246875，status=incomplete。故障NQ4ABTRAIN3001708在第一幕12层触及原生30000步限制，29948个策略决策，玩家26/70HP；learning_returns和reward_sum均null。第18批没有用于更新，不伪造死亡、补种子或延长预算；保存前17批真实权重，固定评价照原计划进行，B3已无法满足64次尝试及≥48接受的原门槛。

关闭的原始故障单行只读取回918632字节/70204压缩字节，SHA 83a73553485658d129ebe1df6c0d40542e7e84f97a9436b2b5dea7227283db28，B3-failed-trajectory.jsonl。原记录最后1000个策略决策均为PlayCard、动作行0，玩家HP保持26；此为原始日志描述，尚未证明是策略或Env首因，也不将segment的旧encounter字段（实际公开boss列）误认作当前战斗。有限公开前缀与正常规则核对另作故障诊断，不扩大为完整整局。

有限诊断实际退出0：冻结首512个策略决策到稳定Combat边界，**550/550原版决策**及全部快照/end.run字段一致，TestMode=false、L4仍为truncated/max_decisions；B3-fault-prefix-verified.json明确normal_combat_prefix_only，不计整局或全部30000步资格。公开快照实际战斗为OVERGROWTH_CRAWLERS。第186决策使用TouchOfInsanity，第187选择ParticleWall；第200决策时第1回合、0能量、95护甲，仍出ParticleWall；第511决策仍第1回合、2894护甲，敌人HP保持8/49、玩家26HP。

真值Core/Models/Potions/TouchOfInsanity.cs只对选择卡调用SetToFreeThisCombat；Core/Models/Cards/ParticleWall.cs原始星能费2、GainBlock9，默认结果为Discard时改回Hand。两者组合允许无限免费返手加护甲；无需虚构Echo附魔或修改游戏规则。这个已验证前缀支持“策略没有停止无益重复出牌”的具体弱点，尚不能说明全部训练退步的首因。后续应独立研究结束回合/零费返手与公开进展信号，不以放大量当前配置代替；原扩训失败结论保持不变。

同一批第190/200/256/511公开快照送入正式X1作CPU float32只读推理，4个argmax全部也是ParticleWall；第200/256/511的结束回合概率分别3.4572e-19/3.6920e-19/4.7755e-19，B3-fault-X1-probabilities.json。只证明原模型在这四个状态已存在相同选择弱点，不能称X1整局一定到达它们或覆盖全部失败动作。另对旧Q4-aa两份冻结首批作纯描述性标签/旧value统计，C标签逐位匹配原Q4-z，value的回报解释方差为.35182/.29956、优势方差中局间部分.59047/.66554；frozen-advantage-description.json。没有新采样、优化或强度评价，也不据此认定critic无用或建立退步因果。

只读核对旧Q4-o教师缓存（SHA 8d30a2c7e3ce51c7c0bb73551d062671a4892b5f5d3f241959864d0d74599f29）：1216训练行中1088行可结束回合，89行教师选择EndTurn，另32留出行含2个EndTurn标签。因此不能解释成“完全没有结束回合示范”。同一1248个冻结公开行对X1及Q4-o两个最终蒸馏模型作单次CPU float32推理：三模型训练EndTurn标签均0/89匹配，留出均0/2匹配；旧蒸馏候选即使这类训练CE从28.944降到16.132/16.153，仍没有学会贪心选择它们。teacher-action-support.json/teacher-endturn-fit.json保留各动作类、完整训练/留出统计和权重SHA。范围只有既有缓存、留出仅2根，不是新教师搜索、学习或强度评价。后续独立研究结束回合决策学习时应保留其他动作能力，并通过新正常整局检验，而不是把整体CE下降当作补齐。

## 固定评价及完成的独立审计

五策略各1024个相同新NQ4ABEVAL种子，全部真实死亡、0故障/截断/通关；原始source为76f28f7。下表来自关闭任务的results.json发布值，保存到published-results.json；取回后独立从全部原始记录重新计算，结果完全一致。

| 策略 | 平均层数 | 第二幕 | 第三幕 | 通关 |
|---|---:|---:|---:|---:|
| X1 | 18.262695 | 300/1024 | 3/1024 | 0/1024 |
| U1 | 16.336914 | 214/1024 | 1/1024 | 0/1024 |
| B1 | 16.982422 | 233/1024 | 1/1024 | 0/1024 |
| U3 | 16.367188 | 194/1024 | 0/1024 | 0/1024 |
| B3 | 18.289062 | 295/1024 | 1/1024 | 0/1024 |

以跑局种子为单位、10000次配对bootstrap、rng20261006：

| 对照 | 平均层数差 | 95%区间 | 登记门槛 |
|---|---:|---|---|
| B1-X1 | -1.280273 | [-1.591821, -0.969727] | 失败 |
| B1-U1 | +0.645508 | [+0.331055, +0.949219] | 通过 |
| B3-X1 | +0.026367 | [-0.217773, +0.277344] | 失败 |
| B3-U3 | +1.921875 | [+1.598608, +2.259766] | 失败 |

B1的原方法对照增益仅表明完整64批条件下这个有界组合减轻了退步，仍明确弱于X1；B3仅17批，而U3是64批，不把该配对正值解释成相同学习预算下的有界因果效果。B3接受次数不足、训练有1真实非terminal；两个B第三幕均1/1024，低于X1的3/1024，原继续条件没有全部满足。没有按中途权重、补种子或扩预算挑选模型。

10:49:18.790691UTC只读观察原launcher1261实际terminal/nonalive、job.exit=1；这是实验门槛阴性返回，全部固定评价已完成，不是遗漏driver退出记录。自动回收对同一个关闭目录只打包一次：59文件/158850759原始字节/47335815归档字节，archive SHA 674a4ed075103823806c1eb17b5cd2f81ba6609ade70ea5db06067f5eaca202e；全部文件和完整归档核验后才关机，原6000秒保护截止11:32:20.355688UTC保持不变。

本项正式训练与评价已终止、原始文件已完整取回，最终审计及成本见下节。


## 最终核验、成本与关机

```bash
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/verify_bounded_combat.py \
  --checkpoint artifacts/nosl-q4r/start.pt --experiment artifacts/nosl-q4ab/cloud/experiment \
  --out artifacts/nosl-q4ab/verified-results.json
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/verify_bounded_followups.py \
  --experiment artifacts/nosl-q4ab/cloud/experiment --out artifacts/nosl-q4ab/final-normal
```

两命令实际退出0。独立核验全部原始训练标签/非terminal排除、预算、首批配对、已记录完整KL接受规则/LR/参数与Adam摘要链、四个最终模型及完整Adam重新加载，以及全部5120评价终点和配对统计；verified-results.json的independent_verification=true、all_evaluation_runs_terminal=true、gate_passed=false。接受规则核验使用记录的完整KL，**不声称第二次独立完整前向重算KL**。原始发布结果与取回results.json完全一致，故障单行与完整B3流逐字相同，closure-verification.json。

| 臂 | 尝试局数 | 更新尝试 | 接受/拒绝 | 故障 | 决策 | 战斗行 | 候选/保留优化step | wall秒 |
|---|---:|---:|---|---:|---:|---:|---|---:|
| U1 | 2048 | 64 | 64/0 | 0 | 374707 | 291387 | 2338/2338 | 613.413025 |
| B1 | 2048 | 64 | 63/1 | 0 | 413687 | 323158 | 2590/2546 | 645.332658 |
| U3 | 2048 | 64 | 64/0 | 0 | 393916 | 307118 | 2466/2466 | 600.328320 |
| B3 | 576 | 17 | 15/2 | 1 | 157429 | 93981 | 750/660 | 391.439875 |

合计6720局尝试、1故障、1339739决策、1015644学习战斗行、8144候选/8010保留优化step、206接受/3拒绝；实际训练wall2250.513879秒、595.303594决策/训练wall秒。B3第18批32局均未用于更新，不能把6720尝试记作原登记8192完整训练局。

最终四候选按登记的每臂首2局及最深局去重选择，实际**12/12完整正常整局、3364/3364正版决策**，逐步快照及全部end.run字段相同，TestMode=false，failures=[]。资格26次/7126决策加最终12次/3364决策，合计38次完整回放执行/10490决策；包括共享种子和重复路径，不是38个独立强度样本，也不代表全部训练/评价均经过Oracle。故障诊断的550决策只算有限前缀，另列，不计整局。

同一关闭云目录只打包一次。四次180秒传输超时后校验已下载前缀并续传，另有三次短暂SSH观察失败，均继续同一任务/归档，未重启训练、重打包或延长6000秒保护。11:04:47.468299UTC实际传输退出0，11:04:48.048974UTC完成59文件及归档SHA/大小校验，retrieval-verification.log。归档47335815字节，原文件158850759字节，SHA **674a4ed075103823806c1eb17b5cd2f81ba6609ade70ea5db06067f5eaca202e**。

**首次确认shutdown为2026-10-06 11:04:56.649174UTC（纽约07:04:56）**，同一确认时间解除原保护；11:05:25实际/proc核对保护和回收进程均已消失，11:05:40.078992UTC另次API确认Success/shutdown。开机指令接受至首次确认关机4354.383796秒，即72分34.383796秒；这是可观察开机窗口，不冒充供应商最终计费金额。

325个10秒资源样本覆盖09:54:04.576247至10:48:33.049446UTC：driver及子进程RSS采样峰值1843.574219MiB、GPU内存采样峰值1067MiB，GPU利用率采样峰值36%、样本均值8.972308%；不声称连续绝对峰值。final-stats.json保留实际统计。unchanged-baselines.json重新验证本地X1/native/受保护源码，并核对取回cloud/provenance.json：云源码76f28f7、X1 SHA和云native SHA均保持原记录；76f28f7完整门禁之后没有运行源码改变。

本工作项机制、失败结果、正常资格与回收核验已完成。**当前配置不扩训、X1不替换；下一项独立研究结束回合学习。高水平Silent A10完整胜率与Linux实机NoSL仍未验收，整个产品目标未完成。**

最终证据于2026-10-06 11:15:22.748829UTC封存：699文件、258211707字节；manifest SHA **7ddf7e46ac38e2f3d98bd4cb828f9a9d69c2d8cb7cd141dde80085a7dc619b08**。生成后逐文件SHA/大小再核验，封存目录不再写入。README/路线图只暂存本项新增段落，其他对话的工作树字节保持不变，foreign-worktree-preservation.json；CLAUDE.md删除及两份性能文档保持原样。
