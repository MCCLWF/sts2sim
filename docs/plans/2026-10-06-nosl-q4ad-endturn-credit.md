# Q4-ad：实际整局收益的结束回合反事实学习

基线main d7727f2ce1541f8f5b3ef4e7090351d7cd31fc84。Q4-ac全部封存/取回shutdown，E1/E3虽拟合示范却比X1平均低1.1445/1.2070层，继续门槛失败。独立检验在实际访问状态比较EndTurn与X1最佳非EndTurn动作、共用冻结接续的真实终局回报，能否改进两候选并超过X1。不是扩大旧教师BCE配置，也不从小标签成功推断高水平；最终A10整局/Linux NoSL目标不变。

## 固定数据与选择

- 正常v0.111.0、全解锁Silent A10、公开schema1/features3/filter1、原生30000步上限保持。X1 SHA 6fb5f1955a11d55175d348659460ce459017baa018f7cf9242a671ba19266703全部原参数/缓冲冻结。source/continuation为Q4-ac最终E1/E3，SHA 563fa7fc93d029e26fe395e4f136f13bf176c62987ed8b1705cbb9d70fd209ad /51be7593886af4e6cc05285270a8112cdfa130d84ad369bd1f06681251c65239，原样载入，不重训旧候选或改其decode。
- 资格16新NQ4ADPILOT000000–000015；数据固定512新NQ4ADTRAIN000000–000511，每次一个root，不补种子。index偶数用E1/奇数E3；desired_stop=(index//2)%2==0，min_floor=4 if (index//4)%2==0 else10，八种组合均匀，root为该策略实际轨迹首个满足floor>=min、公开可选EndTurn且另有动作、当前greedy是否EndTurn符合desired的状态。没到root的真实死亡/故障/截止全部记录，不能挑替代root或删掉失败。
- 每root独立native.Batch.fork两个深复制分支，其他Env/filter/观察/步数/RNG历史均保留。在同一公开状态执行唯一EndTurn，或原X1全部非EndTurn合法logit中argmax。之后两分支都用root来源E1/E3固定greedy策略继续，直到原版真实胜利/死亡；不以战斗结束、原路径长度或截止当整局terminal。全部完整动作保留，私有fork/源seed/目标只在离线采集；actor只能读四公开表。
- 每一步真实回报沿用RunRewards(gamma1,boss .3,HP .2/70,floor/50)，包含第一分支动作及所有后续实际动作，直到终局。若root已在Boss中，初始化公开maxHP基数并把已损血比例标为已付，不能将root前Boss伤害再付一次。保存逐步公共reward-state摘要、结束类型和奖励，独立按公式复算，不从critic预测/旧缓存补收益。两完整分支之差delta=return_end-return_continue；非terminal/故障整对排除，不赋死亡标签。
- train/held以整个新seed的SHA前8字节模5==0留出；seed/group/held/outcome/回报/分支状态不能进actor。收益差绝对值<=1e-5视作近似tie，不提供方向标签，但完整root可作KL锚点；有方向的label是delta>0，权重min(abs(delta)/.02,5)。这是单实际隐藏世界、固定接续下的离线动作比较，不宣称公开最优Q或公平多世界搜索。

## 资格、学习与门槛

本地CPU4/nice19、Torch/OMP/MKL/OpenBLAS各1线程、float32、一次一重活，资格采样上限180秒、全部512正式采集600秒、cache+两头训练180秒，不因失败扩限。fork合同在真实Combat root检查源不变、同root副本公开表/步数相同、同动作的两个fork及独立新局前缀重放结果一致，且无跨分支变更。资格至少8完整paired roots，全部遇到的分支0故障/截断；所有资格分支完整TestMode=false Oracle、每步快照和全部end.run字段核验，记录全部没root/不完整。含直接低步数非terminal合同，证明不能伪造标签。未通过则停止扩大，保存失败。

512数据至少384完整paired roots、0分支故障/截断，有方向训练root>=128且正/负各>=16；有方向留出>=24且正/负各>=4才训练。已有稀少示范问题由新root质量检查，不事后更换种子/过滤类或扩大预算。先记录全部实际回报/标签/资格，再启动训练。

复用Q4-ac146→64→1公开头和frozen X1；两seed1/3分别从固定E1/E3头+fresh Adam开始，在同一个新数据集训练128epochs，minibatch128、lr .0003、gradnorm1。目标为方向root的margin权重BCE均值，加.1×全部完整训练root的Bernoulli KL(initial||new)均值（包括ties）；不做类别重权/过采样，不从留出/中途选择头、epoch或阈值。全部X1原参数和值/局外输出及旧非EndTurn贪心保持，保存最终头/完整Adam/全部epoch记录。train balanced accuracy>=.8、正/负召回各>=.7才启动固定整局试验；所有留出统计原样报告，不用于挑模型。实际训练不得读私有state、seed、outcome或held以外的特征列，cache独立验证公开forward一致。

两最终候选C1/C3各4新NQ4ADPOST000000–000003正常完整greedy，采样各120秒，全部8条TestMode=false原版整局/快照/end.run一致；新候选保存加载、公开特征/mask/原参数合同与完整scripts/check.sh通过后才开云。main合入仍要求完整门禁，不以样本标签检查代替。

## 固定评价与预算

2026-10-06用户要求“尽量少使用云 GPU”。在候选训练与任何正式评价之前，将本项执行平台改为本地CPU4/nice19、float32/各1线程；不启动云实例。训练预算和固定种子/样本数/学习、收益与正常回放门槛全部保持。若本地预算不足按原截止记录，不自动扩大或改用云端；不声称CPU/CUDA逐位等价。

X1/E1/E3/C1/C3各1024相同新tune NQ4ADEVAL000000–001023，greedy CPU float32/64槽，全部真实终点/故障原样记录，总评价1100秒；不补局、不挑中途头。两个C都须0故障/截断、同时比X1及对应E平均高>=.5层、10000次配对bootstrap95%下界>0（以seed为单位、rng20261006），且至少一个C第三幕数量>=X1，才扩大这一实际收益学习方向。全部通关与置信区间原样报告，0通关不能叫高水平。两seed不通过则不扩大、不替换X1；这是组合实验，不分离宣称margin权重/KL/数据源的独立因果。

每最终C首2局、最深终局去重及所有真实胜场均正常Oracle整局/快照/全部end.run核验；独立复算数据、回报、标签、预算、head/Adam roundtrip和统计。云pro-7912450e5767先确认shutdown，开机前独立1800秒保护确认armed；driver1400/launcher1500秒，不延长；云端只运行合同和固定评价，不重训。结束仅打包一次，全部文件/归档SHA/大小验证后立即关机，API确认shutdown才解除保护。新增证据预计<150MiB；旧Q4-z/aa/ab/ac封存目录只读，不清理用户资料。其他chat的CLAUDE删除/README/roadmap性能改动与两文档保持原样。

上述云预算为最初登记、现不执行；本项仅只读确认shutdown。本地同样保存全部采样/失败/权重和独立核验，不因省去云端而减少正式样本或放宽门槛。

分支feat/nosl-q4ad-endturn-credit；新增python/nosl/endturn_credit.py、tools/nosl/check_endturn_credit.py、tools/nosl/endturn_credit_experiment.py、tools/nosl/verify_endturn_credit.py与报告，复用端点/公开NoSL合同工具，不修改游戏、原filter或放宽比较。工作项完成时README/roadmap只暂存本项前缀。高水平完整胜率、全部公开信息/历史合同和Linux实机部署仍须产品级验收。

## 资格阶段执行记录（正式采集前）

首次16固定pilot在5.2504秒得到13完整paired roots、3条未到root正常死亡；原始轨迹和fork快照完整保存，但末尾打包误用固定长度stack，动态token/action长度不同导致ValueError。仅修复公共表补齐及故障记录边界：从原13个冻结root恢复public表，原目录/失败日志保留，不重新采样、不换种子、不延长预算；packing-recovery.json记录逐文件SHA及唯一追加的public摘要。独立3343步奖励/13公开源动作复算一致，26/26完整TestMode=false回放、6380原版决策及每步快照/全部end.run字段相同。真实低步数fork两个STEP_LIMIT通过实际标签函数均无标签；相同动作副本与新局前缀检查通过。资格达到原门槛，后续512种子/600秒及原数据和学习门槛保持。
