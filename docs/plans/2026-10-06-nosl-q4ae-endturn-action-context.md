# Q4-ae：结束回合与具体继续动作的公开上下文

起点main 836586d6549981a0cda66d98bc086cb206d36d41。Q4-ad的真实终局标签合格，但两个固定头训练balanced accuracy .802665/.782188，联合fit gate失败，留出.565341/.613636；不重开或扩大旧实验。本项检验**将X1当前最佳非EndTurn动作的公开编码给结束回合头，是否在相同容量/成本对照下改善实际整局收益**。冻结原X1，部署仅四公开表推理；高水平Silent A10完整胜率及Linux NoSL目标不变。

## 固定输入与对照

- 正常v0.111.0，全解锁Silent A10，schema1/features3/filter1，原生30000步上限。X1 artifacts/nosl-q4r/start.pt SHA 6fb5f1955a11d55175d348659460ce459017baa018f7cf9242a671ba19266703，全部原参数/缓冲冻结，不改Env/model.py/filter。
- 只读复用封存Q4-ad450root public-roots.npz SHA 0cc7ca9267b631dac95997ca1b85a1064e7ced893f0232ca8d6d926986a3c557、manifest SHA 724d51d67ed89761939fd2d90477e20fc1cb431bafb7393361b6fb3ef2878878。原352训练root/177方向(62正115负)、98留出/49方向(16正33负)及全部reward/delta/近似tie/margin权重保持，不采新标签或换拆分。该留出指标已在Q4-ad看过，只作开发诊断，不能冒充新测试或选择权重依据。
- B1/C1从旧Q4-ac固定E1头初始化(SHA 563fa7fc93d029e26fe395e4f136f13bf176c62987ed8b1705cbb9d70fd209ad)，B3/C3从E3(SHA 51be7593886af4e6cc05285270a8112cdfa130d84ad369bd1f06681251c65239)。不从Q4-ad训练后权重开始。
- 每模型保留原146→64→1结束头，再加274→64→1残差：146为公开run128及原9数的numeric18，另128为冻结X1 action_features中其最佳合法非EndTurn动作的action半部(类型/source/target/number，全部源自公开表)。B残差该128部分恒为零；C使用实际公开编码，其余网络、参数量、初始化、原结束头及训练预算相同。残差最后层零初始化，初始四模型逐位还原各自E公开logit/value。
- 原结束头和新增残差都可学习，整个X1冻结。贪心解码保持：仅公开Combat且唯一合法EndTurn和其他合法动作时按stop>0选EndTurn，否则保留X1非EndTurn argmax；局外/不适用logit、价值和合法mask不变。不能把softmax解释为头的Bernoulli概率。结束头没有动作就绪时残差action部分置零，避免parked非法gather；不向actor给seed/source/outcome/held/reward/状态hash或私有RNG。

## 本地合同与固定学习

按用户“尽量少使用云 GPU”，本项全部本地CPU4/nice19、float32、Torch/OMP/MKL/OpenBLAS各1线程，一次一个重活；不启用云。先只读SHA核验旧manifest及raw标签/公开cache，缓存四头总学习预算180秒；数据缺失或不符则保存失败，不重新采集旧seed。

学习seed1/3各对应B/C，同seed残差初始tensor逐位相同，fresh Adam/lr .0003/minibatch128/gradnorm1/固定128epochs。目标保持方向训练root的margin权重BCE均值，加.1×全部完整训练root的Bernoulli KL(initial||new)均值（包括tie）；同seed B/C采用完全相同方向root随机顺序，无类重权/过采样。保存最后模型/完整Adam/全部129epoch；不按中途/旧留出挑epoch、seed或阈值。

合同必须覆盖450真实root公开缓存vs普通forward，B动作置零/同seed初始权重和E输出严格相同；旧2759只读教师/锚点包含611不适用/368局外，原params/value/非EndTurn选择/mask/仅EndTurn/重复EndTurn/parked不变，padding无影响，格式错误/非finite/错原模型及generic loader拒绝。残差格式显式加载，不静默丢头。独立从raw奖励核对目标/split/weight、最终指标及完整Adam/预算。全部四模型完成且两个C训练balanced acc>=.8、正/负召回各>=.7才进行正常新局及正式评价；B无需fit通过但必须同预算真实完成且合同通过。

## 正常路径与正式实力

- 四最终模型各4个新NQ4AEPILOT000000–000003完整greedy正常新局，采样各120秒；全部16执行必须真实终局且完整TestMode=false Oracle、每步快照/全部end.run字段一致，无HP/deck/relics注入。任何故障停止扩大。
- 原样最终权重的X1/B1/C1/B3/C3各1024同一新开发评价seed NQ4AEEVAL000000–001023，greedy CPU float32/64槽，总评价上限1500秒；不补种子或延长。每次尝试真实终点/故障/截止/未开始均保留，0胜不叫高水平。
- 两个C各对X1和对应B平均层数提升>=.5，10000配对bootstrap95%下界>0（seed为单位、rng20261006），两个C均0故障/截断，且至少一个C第三幕数>=X1，才继续此动作上下文方向。四候选都无故障/截断才给完整配对门槛；失败不扩训、不替换X1。记录真实完整通关与Wilson95%区间，指标不替代正常A10双最终Boss完整成功。
- 正式所有B/C首2局+最深真实终局去重+全部真实胜场均完整正常Oracle/快照/end.run核验，原始结果独立复算。有限验证范围如实报告，不能将少量回放扩大为全部5120条已经Oracle认证。

无新搜索/教师或云开机，本项新增证据预计<100MiB。完整scripts/check.sh CPU4,5/STS2_BUILD_JOBS=2实际通过才合入main；若学习门槛失败，停止正式评价但照实保存审计/主线门禁/报告。旧Q4-z/aa/ab/ac/ad封存只读。保留CLAUDE.md删除、别的chat README/roadmap及两份性能文档；仅暂存本项进度前缀。

分支feat/nosl-q4ae-endturn-action-context；新增python/nosl/endturn_action.py、tools/nosl/check_endturn_action.py、必要时tools/nosl/endturn_action_experiment.py，以及独立审计/报告。源提交、格式/schema/features/X1/head/data/native SHA及实际资源/秒数记录。全部NoSL公开记忆、Linux实机及高水平胜率仍须产品级验收，不将本项小头通过叫完整目标完成。
