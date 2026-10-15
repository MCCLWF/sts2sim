# Q4-y：冻结战斗的整局回报战略学习

基线 main 40edfe0。Q4-x 的离线教师在初中期有局部改善，但无第三幕/胜利；Q4-q/r 的共享 PPO 长 horizon 未形成可靠整局提升。本项检验独立局外 actor 用完整实际后续回报学习，能否在不改战斗策略的条件下改善正常 Silent A10 跑局。不是重试 Q4-m 已失败的同根监督扩采，不用教师未来标签。

## 固定学习方案

- 原版 v0.111.0、Silent A10、全解锁老档案、正常新局；不注入资源、牌组或遭遇。Env/公开 schema/actor filter1不变。
- 战斗及战斗内 Choice 使用冻结 X1 贪心推理，SHA 6fb5f1955a11d55175d348659460ce459017baa018f7cf9242a671ba19266703。局外使用从 X1 复制的独立完整 actor/critic；不同参数存储，优化器不接触战斗模型。路由只读取公开 global[11]，不在线搜索/模拟/试动作。
- 仅局外多合法动作进入 PPO 更新。每个批次从新种子完成32条正常终局后才更新，采样期间策略不变。回报 gamma=1，沿用当前 floor/HP势/Boss 已观察伤害和真实胜利奖励；从所选动作至真实终局求和，不按战斗出牌次数截断或依赖 combat critic bootstrap。整局 Monte Carlo 有较高方差，不称真实反事实动作价值。
- 独立局外 value 估计上述回报，优势 return-value 在完整批次归一化。PPO clip.2、LR.000133、熵.05、value系数.5（value loss另有.5）、grad norm.5、2epochs、minibatch256，目标KL.01沿用既有PPO的有限LR自适应；无教师CE、无回报事后调权。
- 两个学习seed1/3，各128个更新批次×32局=4096次尝试，只保存起点与登记最终模型用于比较。训练前缀NQ4YTRAIN，开发输入NQ4YPILOT独立；失败/步数限制单独记录、整局不进入MC可信训练，不补局不伪造terminal。故障率>1%或deadline终止则记未完成预算，不重跑不利臂。

## 资格、评价与判据

先在本地验证完整奖励求和与独立逐项后续和一致、终局不bootstrap/故障不使用、所有公开表int32无损、初始双模型路由与同批X1一致、真实优化后战斗全部参数/输出不变，记录实质局外梯度和合法mask。开发8局及至少2条最终混合策略正常规则整局oracle/L4一致；完整scripts/check.sh通过后代码才合main。

正式固定X1、S1、S3在同一新的1024个tune种子NQ4YEVAL000000–001023作整局贪心评价，记录所有尝试、真实terminal、故障、幕/层、胜利、公开路由计数与轨迹。继续扩大此课程要求：两个学习seed均完成固定预算、无新增系统故障，均相对X1至少+.5层且配对seed-bootstrap95%下界>0，且至少一个候选第三幕进入数不低于X1。主要整局胜利仍必须另报；上述只是研究继续门槛，不是高水平产品验收。

不用旧dev/test作为调参输入，不按评价挑中途checkpoint、重设seed或补样本。评价冻结后只抽取每个登记最终候选前2种子及每个候选的最深终点各1条，做实际TestMode=false的全字段/L4终点核验；深轨迹可能重叠时不重复抽取。这些有限回放不代表全部1024条均有正版资格。

## 资源与交付

一项计划/分支feat/nosl-q4y-episodic-strategy/报告，源码python/nosl/episodic_strategy.py、tools/nosl/episodic_strategy_experiment.py及定向checker，产物artifacts/nosl-q4y。本地nice19/CPU4，一次一个重活，构建CPU4,5/两job。先测8局smoke估计正式成本；云正式每臂1800秒、评价共1200秒，总任务最多5400秒，脱离会话的power-off保护最多6000秒，期限不延长。超限为truncation/未完成试验。

云实例pro-7912450e5767需先读当前status、保护在启动前确认live，增量同步独立源码/验证依赖与实际模型SHA，不改Q4-x等冻结产物。取回归档后校验全部SHA/大小立即停机，API确认shutdown才解除保护。记录流水线吞吐、RSS/显存、起停时刻；凭据只在本机读取，不上传。保留其他chat未提交修改。
