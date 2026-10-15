# Q4-ab：有界战斗策略更新的匹配整局实验

基线main 7bfc45f1bac137760e1b1159dc2acff6540fe299。Q4-aa的四组冻结首批中，P单独已有.111–.146精确KL与约11%贪心动作变化，V/H均无登记共同放大信号；仅是诊断，未证明整局退步首因。历史Q4-f的sampled approx-KL后续调LR不等于本项批次完整分布的接受约束。本项检验较小初始学习率、逐批更新接受约束这个组合是否保住X1并改善正常A10整局；高水平NoSL/Linux目标不变。

## 固定实验与信息边界

- 原版v0.111.0、全解锁Silent A10、features3/filter1、X1 SHA 6fb5f1955a11d55175d348659460ce459017baa018f7cf9242a671ba19266703不变。只训练战斗公开actor/critic，局外全部参数与输出独立冻结X1；不改变模型、游戏规则、奖励或观察。C实际整局MC回报、gamma1、HP .2/70、boss .3保持Q4-z；不重复加入未复现的R回报。
- 学习seed1/3各两臂U（原更新）/B（有界更新），均从独立X1+fresh Adam出发。各64次更新尝试×32新正常整局，固定NQ4ABTRAIN{seed}{iteration:04d}{slot:02d}；同seed两臂共享跑局种子，首次动作/标签必须相同，此后按各自真实on-policy轨迹。故障/非terminal整局全部排除，不伪造死亡/补局。
- U调用未修改episodic_strategy.update；初始lr .000133、2epochs、256 minibatch、PPO clip .2、value系数.5（value loss=.5 MSE）、entropy .05、grad norm .5及既有后续LR自适应不变。所有臂记录全批更新前后精确KL(old||new)、p95/max、熵与贪心动作变化；不把逐minibatch sampled approx-KL当同一指标。
- B初始与最高lr .000133/4，最低.000133/128；同样两epochs与全部损失。每批保存actor和Adam更新前状态，在**所有可信战斗多合法行**的规范256批次上缓存旧公开动作分布，完成一次候选更新后精确测量同批分布。均值KL≤.01且有限才接受；否则完整恢复actor/Adam（包括moment与step），丢弃此候选，下一批lr减半。不重跑本批、不搜索环境未来；这是离线优化器参数恢复，部署只公开推理。
- 接受后KL>.008则下批lr/1.5，KL<.0025则×1.5，其余不变，均夹在登记范围。旧更新函数的sampled-KL调LR对B覆盖为本项规则；U保持原行为。每次尝试都保存候选/接受后KL、前后参数/Adam摘要、实际优化步数、接受或拒绝、LR及耗时。B是一个稳定性组合，不从结果分离宣称初始LR或约束的独立贡献。约束只针对本批平均KL，不保证每一状态或所有未来状态变化小。

## 资格与预算

先在Q4-aa冻结真实首批检查：关闭接受约束的诊断包装与原学习函数的权重/Adam严格相同；强制拒绝后参数与Adam状态（除登记下批LR）严格恢复；正式cap下的实际更新决策、合法mask/有限值/局外冻结与权重元数据正确。另8条新正常A10资格整局和更新后U/B各2条贪心整局，TestMode=false/完整L4/全终点/快照Oracle回放；非terminal排除沿用已验证采集器，直接再核对C原始标签。实现完成必须scripts/check.sh全通过，再启动云正式实验。

资格补充（不改正式配置或强度判据）：首轮B均被正常拒绝，增加一次冻结下一批、按规则已减半LR的正式cap检查，以及固定宽cap1的正向接受分支覆盖；若下一批接受，保存另2条新正常贪心路径并完整Oracle回放。额外注入第二个优化step后的优化器异常，验证已经改变的中途参数/Adam完整恢复；这是优化器故障资格，不是修改游戏规则或奖励。补充上限120秒，与原冻结/事务检查累计仍不得超过480秒；保存全部结果，不改正式初始LR、cap或训练预算。

正常资格实际12/14通过后定位两项原版宿主适配缺口，保留原失败：Pandora已执行全部变牌后仍创建检查屏幕；带WingedBoots的最后普通地图行被MapTravel网格枚举漏掉Boss。仅补Headless的纯查看屏幕工厂和OracleRun按原NMapScreen.RecalculateTravelability优先级给出Boss/第二Boss，不改DLL规则、原生Env、选项或L4比较。加入两条原始正常动作的tests/l4/check_nosl_oracle_adapters.py回归及scripts/check.sh门禁、pitfalls来源说明；所有14条冻结资格另目录重放，全部通过才启动云实验，不更换失败种子。

本地CPU4/nice19、Torch/OMP/MKL/OpenBLAS各1线程、float32，一次一重活。冻结资格总上限480秒、新正常资格各采样120秒、Oracle/全门禁独立等待，观察超时不重启活跃任务。云CUDA float32，每臂900秒、固定评价1200秒、driver5100秒/launcher5400秒；开机前启动独立6000秒保护，不延长。取回全部文件逐项SHA/大小验证后立即关机，确认API shutdown才解除保护；失败/超时仍保存证据且关机。旧Q4-z/aa冻结目录只读，新增产物artifacts/nosl-q4ab，预计新增<300MiB，不清理用户数据。

## 固定评价与判据

五策略X1/U1/B1/U3/B3各1024个相同新tune NQ4ABEVAL000000–001023，贪心、64槽、全部正常新局真实终点；不补局、不按中途checkpoint挑模型。以跑局seed为配对单位，10000次bootstrap、rng20261006。每个B必须同时比X1和同seed U平均高至少.5层且95%区间下界>0；两seed都通过、所有训练/评价0故障/截断且预算完整、B每臂接受≥48/64次、接受后全批平均KL均≤.01，至少一个B第三幕数量不低于X1，才继续扩大这个配置。全部胜场/第二幕/第三幕/故障数原样报告；这仍是探索门槛，不是高水平完成或正式模型晋升。

评价后每候选前2条与最深终点（重合去重）正常Oracle回放，真实胜利须完整最终Boss路径，不能弱化判据。全记录独立复核标签/预算/接受规则/摘要/统计及资格选择；未通过不扩大、不替换X1。分支feat/nosl-q4ab-bounded-combat-update；新增python/nosl/bounded_combat.py、tools/nosl/check_bounded_combat.py、tools/nosl/bounded_combat_experiment.py、tools/nosl/verify_bounded_combat.py、tools/nosl/verify_bounded_followups.py及报告，更新README/roadmap仅本项前缀，保留其他chat脏文件。

故障后诊断补充（2026-10-06 10:38UTC，非事前强度判据）：B3第18批已有一条真实step_limit，按原规则停止、保留全部失败和前17批权重。本项继续原固定评价，不补局、不增加训练、不重启。另只消费该关闭的原始失败历史，检查最多512个原始策略决策的公开状态，并选择这个范围内最后一个稳定Combat边界做TestMode=false的原版前缀/快照/全end.run核对；前缀继续保留truncated，不计完整整局资格，也不覆盖全部30000步。原资格、最终候选选择、预算和扩训判据均不变。
