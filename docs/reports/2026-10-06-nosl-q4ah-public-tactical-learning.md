# Q4-ah：公开战术特征学习报告

状态：已完成，继续门失败。两seed本地CEM学习、独立优化过程核验、3209公开状态合同、固定四策略各1024正常新局评价及独立统计复核、8条pilot与9条最终完整正常原版回放、完整主线门禁均已完成。H1/H3未超过固定规则R，并明显弱于X1；不扩训本配置、不替换X1。全程本地CPU，云未启动；高水平Silent A10/Linux NoSL目标仍未达到。

依据[计划](../plans/2026-10-06-nosl-q4ah-public-tactical-learning.md)，比较固定公开规则R、同一策略族两次独立整局CEM学习和原X1。所有未来轨迹只在离线学习中执行，新actor仅推理公开特征。游戏/奖励/旧actor不改，公开伤害等特征是近似摘要，不能当精确引擎预览。

## 已实现的信息合同与正常资格

原X1完整冻结，每次按原float32批次调用，value与非适用/Choice/局外分数逐值不变；适用战斗决策才使用16项公开特征的正权重加和。EndTurn=0，DiscardPotion固定低分，合法mask保持。特征按公开能力持有者聚合，不读未来种子、隐藏抽牌顺序或预测队列。静态卡牌表与X1内的全部numbers/flags/powers tensor逐值一致，表SHA256 `17a35189779ad6774cfa3289a1ee8adf8bc08640e8821618746220151f471f23`。

公开估计有明确限制：卡牌静态damage/block/repeat不覆盖临时var覆盖、遗物及全部能力/重复次数；CardsVar同时可能表示抽牌/生成；部分非静态效果没有单独特征。DaggerSpray的2击只依据正版源码明确的WithHitCount(2)修正。这个规则策略族不是完整规则预览，不改变环境，亦没有模拟搜索/试动作回滚回退。

```bash
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/verify_tactical.py contract --out artifacts/nosl-q4ah/contract-v1
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/tactical_experiment.py pilot --out artifacts/nosl-q4ah/pilot
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/tactical_experiment.py normal \
  --experiment artifacts/nosl-q4ah/pilot --out artifacts/nosl-q4ah/pilot-normal
```

均实际退出0。初始实现source `2232de1fd97b850275d0fb99061bc57d9dd333e0`；之后只给采集增加proposal前置保存、独立结果统计核验，actor/原版回放行为不改。合同3209公共状态（611不适用/368局外）、2598逐局vs批量特征核对、基础卡面/持有者golden与padding污染均通过。R在1523个适用状态改变贪心动作，不是未实际参与决策的恒等模块；block native.Batch构造后所有actor推理仍通过，无Env调用。X1所有tensor与非适用分数/value/mask保持，错误格式/schema/基线/能力id/非有限/负数/维度及generic-loader明确拒绝。frozen X1 digest `6996897c9904d11036f9c89ec00866b05981753e7a2a05a97cf35772ee4f8080`。

pilot X1/R各4相同新NQ4AHPILOT种子，正常Silent A10，全部8实际死亡、0故障/截断/通关；两均值24.25/15层，仅系统资格，不以4局结果调整预先冻结的学习或评价预算。实际PID1391941，2026-10-06 15:59:34.680893–15:59:37.499553UTC，wall2.818663秒、kernel child maxrss607.585938MiB。原版资格PID1392301，2026-10-06 16:01:18.955520–16:01:40.306192UTC，wall21.350673秒、kernel child maxrss574.878906MiB；**8/8完整正常TestMode=false、2502/2502决策、逐步快照和全部end.run一致**，failures=[]。重复seed跨两策略是8次执行，不是8个独立质量样本。

Q4-ag封存manifest SHA `f42c5f5042d4c479c9863e819e6d785dd7d916b3964a0df963e5c6fc5d9c5597`及全部515文件在新pilot前逐一验证，不写入旧目录。CEM配置固定为两seed、4代/8候选/每候选32实际终局，离线终局分数按整数floor总和/胜利数最后统一归一化，避免数学相等的均值因浮点加和顺序打破elite tie。每代全部proposal及噪声后的RNG状态在采样前保存，后续失败也保留已使用权重；最终使用固定第4代mean，不挑中途权重。

完整门禁source `a4bcb6d3e8a4b332e5778ed92beb3dabb4dba184`，`taskset -c 4,5 nice -n 19 env STS2_BUILD_JOBS=2 scripts/check.sh`，2026-10-06 16:02:34.191514–16:07:28.354400UTC，wall294.162888秒、实际退出0，末行all checks passed。其后仅改报告，不改运行源码。

## 固定本地学习与独立复核

```bash
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/tactical_experiment.py train \
  --contract artifacts/nosl-q4ah/contract-v1/result.json \
  --normal-qualification artifacts/nosl-q4ah/pilot-normal/summary.json \
  --check-result artifacts/nosl-q4ah/check-result.json --out artifacts/nosl-q4ah/learning
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/verify_tactical.py learning \
  --learning artifacts/nosl-q4ah/learning --out artifacts/nosl-q4ah/learning-audit.json
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/verify_tactical.py contract \
  --learning artifacts/nosl-q4ah/learning --out artifacts/nosl-q4ah/contract-final
```

三命令实际退出0，运行source均`a4bcb6d3e8a4b332e5778ed92beb3dabb4dba184`。学习实际PID1395197，2026-10-06 16:08:39.834274–16:14:52.390293UTC，宿主wall372.556021秒、内部371.529275秒；CPU4/nice19、Torch2.14.1+cu130使用CPU、Torch/OMP/MKL/OpenBLAS1线程，kernel child maxrss646.992188MiB，user/system372.214950/.387123秒。

H1/H3各32完整候选×32同代共同种子=1024策略轨迹，全部真实死亡，0故障/截断/通关；各128唯一run seed、总256唯一训练seed，不能把重复同代8候选当1024独立seed。各4代使用32个新银行，前缀NQ4AH1TRAIN/NQ4AH3TRAIN，编号000000–000127；没有开发/最终留出seed训练。两臂内部耗时183.633170/187.879958秒，均未接近每seed1200秒上限。

固定初始log-weight sigma .35、范围初值±1.5，候选当前mean+7高斯proposal；完整终局目标`floor/50+2*victory`。每代前2按分数、tie按candidate index，mean/σ按预设.5混合、σ下限.1更新。最终固定第4代mean，不选最好的候选或中途权重；两最终16正权重均有限且改变。H1/H3各代candidate0均值分别14.71875/16.25/14.25/16.03125和15.8125/15.40625/14.34375/16.34375层，每代银行不同，不能把这些数当同固定seed上的学习收益曲线。

H1 checkpoint SHA `889ef97fb2bba85b35b45afa46528db2b8daad6fc386bd00504c26b39c36c9bd`；H3 `b8a868bceb996e21d283671e04b7cef5388660f158d3227db6bccd202dce20f2`，显式实验JSON格式，尚无C++/Linux加载适配。学习只优化公开规则的16个权重，X1全部神经参数及静态表冻结；不得把规则先验或教师收益计成神经网络训练改进。

独立学习审计wall1.022688秒，由原2048终局以Fraction另算分数，重生全部64组proposal、重算每代elite/mean/σ与完整PCG64 RNG状态，最终JSON全部权重完全相同。最终合同wall14.129530秒，R/诊断/H1/H3全部3209状态通过，2598逐局vs批量特征再次一致；原X1 digest/value/非适用分数/mask不变、错误格式明确拒绝。正式评价前的checkpoints SHA再核对不变；目前仍不以学习成功或pilot合同声称整局强度提高。

## 正式评价、独立复核与最终原版回放

```bash
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B artifacts/nosl-q4ah/run_final.py evaluate          # 实际退出1：继续门失败的登记退出码
taskset -c 4 nice -n 19 env ... .venv/bin/python -B artifacts/nosl-q4ah/run_final.py evaluation-audit   # 退出0
taskset -c 4 nice -n 19 env ... .venv/bin/python -B artifacts/nosl-q4ah/run_final.py final-normal       # 退出0
taskset -c 4 nice -n 19 .venv/bin/python -B artifacts/nosl-q4ah/verify_closure.py                       # 退出0
```

评价source `eaec5b2a587f9df9785e8de12858c0b2397afc23`（相对门禁source只改报告），PID1396840，2026-10-06 16:17:18–16:35:04UTC，wall1065.6秒、单线程CPU、kernel child maxrss659.6MiB。四策略使用相同的`NQ4AHEVAL000000–001023`，4096次尝试全部是原版死亡终局，0故障/截断/通关/第三幕。

| 策略 | 尝试 | 死亡 | 通关 | 进第二幕 | 进第三幕 | 平均层数 | Boss前死 / 第一幕Boss死 / 第二幕途中死 / 第二幕Boss死 | 主动EndTurn / 策略决策 |
|---|---:|---:|---:|---:|---:|---:|---|---:|
| X1 | 1024 | 1024 | 0 | 345 | 0 | 18.7510 | 198 / 481 / 295 / 50 | 3 / 239160 |
| R（固定规则） | 1024 | 1024 | 0 | 149 | 0 | 15.5059 | 376 / 499 / 142 / 7 | 22685 / 205024 |
| H1（CEM seed1） | 1024 | 1024 | 0 | 136 | 0 | 15.4434 | 376 / 512 / 131 / 5 | 21446 / 202932 |
| H3（CEM seed3） | 1024 | 1024 | 0 | 160 | 0 | 15.7246 | 371 / 493 / 151 / 9 | 23978 / 215528 |

| 配对 | 平均层差 | bootstrap 95%区间 | 登记门槛 | 结果 |
|---|---:|---|---:|---|
| R − X1 | −3.2451 | [−3.6387, −2.8652] | 另报 | 规则族弱于X1 |
| H1 − R | −0.0625 | [−0.2295, +0.1025] | ≥1且下界>0 | 失败 |
| H3 − R | +0.2188 | [−0.0371, +0.4766] | ≥1且下界>0 | 失败 |
| H1 − X1 | −3.3076 | [−3.6865, −2.9395] | ≥2且下界>0 | 失败 |
| H3 − X1 | −3.0264 | [−3.4151, −2.6504] | ≥2且下界>0 | 失败 |

**结论：这轮训练没有提高模型水平。** 学习后的16个权重与初值差别不大（单项相对变化最大约±36%），两个候选相对固定规则R的差在±0.25层内且区间跨零；整个规则族比X1少约3层，Boss前死亡从198增加到约375。完整胜率为0/1024（每个策略），Wilson 95%上界约0.37%。继续门失败，按登记不扩训、不挑权重、不替换X1。

独立统计复核（PID1399357，wall1.32秒）从原始逐局记录另算终局、均值、配对bootstrap与原版回放选取规则，与`results.json`全部一致，输出`verified-results.json`。按登记的“每候选前2局+最深合格终局”选出R/H1/H3各3局，共9局（各候选的最深局均为第二幕33层Boss死亡）。最终原版回放PID1419881，2026-10-06 18:53:44–18:54:07UTC，wall23.63秒：**9/9完整正常TestMode=false、2786/2786决策、逐步快照和全部end.run一致**，failures=[]。`verify_closure.py`另核对：全部任务进程已结束、两checkpoint SHA未变、Q4-ac至Q4-ag共1661个封存文件逐一未变、外来未提交文件字节未变、生产actor/game/奖励源码未改、`libsts2nosl_c.so`与评价时一致。云实例本项从未开机；最后一份只读状态记录为2026-10-06 16:37:23UTC的shutdown，收尾时没有再查询云端。

这些证据的范围：有限的工程与执行合同（公开特征合同、所选9+8条轨迹的保真、统计可重算）。它们不证明策略强度合格、全部新轨迹保真或Linux成品完成。

## 证据封存与合入门禁

`artifacts/nosl-q4ah/manifest.json`于2026-10-06 18:55:10UTC封存333个文件、21013039字节，逐文件SHA256复核通过；manifest SHA256 `cf7569eafdf5f42f8b1ca7e38652fb793265ef7edb574d32d5a23145ab45658d`。封存后不再向该目录写入。

收尾提交`3342adde9ee80a0d6543ef9e9b07d4f4e614b9d3`（相对已通过门禁的source `a4bcb6d`只改报告、README与路线图）上再次运行`taskset -c 4,5 nice -n 19 env STS2_BUILD_JOBS=2 scripts/check.sh`：2026-10-06 18:55:16–18:59:22UTC，实际退出0，末行all checks passed。这次门禁的日志在会话临时目录，没有进入封存目录。

## 这轮结果改变了什么判断

1. 手工公开战术规则加16个权重的CEM不是出路：规则族本身比X1弱3层，权重学习在256个训练种子、2048局的预算下没有可测收益。下面的静态盲点说明这个族还有表达缺口，但补特征只能在同一族内改善，未测量其整局收益，不作为已成立的方案。
2. X1在239160个策略决策中只主动结束回合3次，即“还有合法动作就继续出”。此前Q4-ac至Q4-ag围绕结束回合头的五轮实验都在这个习惯附近做局部改动。
3. 四个策略的死亡结构相同：约一半的局死在第一幕Boss，进入第二幕的局全部死在第二幕。平均层数在这个结构下主要反映“有多少局走到第一幕Boss”，对实力变化不敏感。后续工作项应先重新诊断整局失败的来源，再决定学习方法，不再延续本轮与前五轮的局部头/权重微调。

## 事后静态诊断与本机资源（不改变本轮判据）

冻结评价期间仅作静态只读诊断，`static-blind-spots.json`以导出的91张Silent卡池/两level及当前特征定义检查：15张/30个level在未改变印刷Attack/Skill类型、适用战斗决策中没有任何正特征，正的出牌固定成本使其分数严格低于EndTurn=0，不论CEM权重怎么调整。包括CalculatedGamble、PiercingWail、Malaise、Burst、StormOfSteel等。这不是访问频率或已证实的退步主因，只说明此规则族的表达能力缺口；不据它追加本轮特征、训练或改门槛。

正版CalculatedGamble.OnPlay按当前Hand.Count执行DiscardAndDraw，静态CardsVar表没有这项动态效果；PiercingWail的StrengthLoss是通用DynamicVar并由专用Power应用，不是此表的Weak/Poison/Block。这两个源码明确例子支持上述静态遗漏判断。其他复杂效果/类型变化仍超出此诊断，不能说16项已经覆盖全部战斗规则。后续应先改善这些公开语义的表示，再评估投入更多同配置训练是否有意义。

本机只读资源查询显示NVIDIA GeForce RTX 5060 Laptop GPU、driver595.84、8151MiB总显存；PyTorch2.14.1+cu130/CUDA13.0于2026-10-06 16:24:56.051198UTC报告cuda_available=true、编译架构含sm_120。这里只检查设备/软件metadata，没有分配模型tensor、运行GPU模型或计算smoke；不能据此宣称GPU模型计算合同已通过。本轮继续原冻结CPU采样配置，后续可先验证本机CUDA计算以减少云使用。
