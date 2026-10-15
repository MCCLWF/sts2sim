# Q4-v：动作头战斗/局外影响归因

基线main 73b1bab。Q4-u四臂均未通过整局提升门槛；冻结编码器仍有商店等局外动作漂移。本项用已冻结候选做独立诊断，回答教师拟合的退步是否主要来自局外干扰。不得继续训练Q4-u、重选checkpoint或重算其门槛。

## 固定输入与公开信息

X1仍为正式基线（SHA 6fb5f1955a11d55175d348659460ce459017baa018f7cf9242a671ba19266703）；候选为Q4-u最终C1/P1/C3/P3，第64轮固定文件。所有非评分参数SHA必须仍相同。路由只读取现有公开global列11（kGInCombat），战斗中的嵌套Choice也归战斗，不能只按DecisionKind划分；不读取未来、privileged状态或终点。

每候选比较full（所有决策用候选）、combat（战斗用候选，局外用X1）、outside（局外用候选，战斗用X1），加纯X1，共13策略。路由是本实验的诊断，不自动成为部署策略。共用冻结编码器只节省重复前向，必须与两份完整原模型的合法logits逐项一致，value始终为X1；标准greedy/CUDA bfloat16设置与Q4-u一致。记录每类决策、候选生效/实际改变次数及全部终点。

## 登记评价与判据

所有13策略各512个独立新tune种子NQ4VEVAL000000..000511，正常Silent A10/filter1完整跑局；无资源注入，不在此集合学习。全部尝试/死亡/胜利/error/unported/hang保留；固定顺序X1、各C1/P1/C3/P3的full/combat/outside，不按结果停止或加量。保存每局action_rows和首2种子轨迹；四候选SHA校验，拒绝重复正式run。

每臂报告full-X1、combat-X1、outside-X1、combat-full、outside-full及同seed P-C在三模式下的逐种子配对层数均值/SE/95%区间；同时报告胜率、过幕、故障。若C或P的combat或outside在两个学习seed都比X1至少+.5层、配对区间下界>0、无故障，才打开对应独立后续学习路线。此探索性归因只作下一步选择，不是高水平验收或多重比较后的显著性承诺。否则不扩大该头学习路线、不替换X1。不要将combat-full改善直接解释为同一战斗的因果损失：整局状态分布会随行为变化。

## 合同与执行

先本地CPU冻结参数、公有teacher/anchor表逐项logit路由一致、嵌套Choice/两类覆盖、padding/合法mask核验；CUDA正式前也复核。新工具不改model/schema/Env/部署默认，公开动作推理不触发搜索。正式结果的首2固定种子、8种hybrid共16条完整轨迹用TestMode=false正版oracle按现有L4合同回放；真实终点/全部决策一致才具备这批有限规则资格。

需要GPU时使用已授权pro-7912450e5767；启动前建立并验证本地独立detached保护，绝对最长2400秒，远端正式进程限时1500秒，一次一个任务。原始结果取回SHA验证后立即power_off并API确认shutdown；保护只有确认shutdown后解除，凭据仅留本地。不触碰已有artifacts；新证据artifacts/nosl-q4v。

分支feat/nosl-q4v-context-attribution；新增tools/nosl/context_attribution.py及定向合同工具；完整scripts/check.sh后合入main，报告docs/reports/2026-10-05-nosl-q4v-context-attribution.md，更新README/roadmap并保留其他chat未提交工作。高水平整局NoSL目标保持active。

## 结果读取后的规则资格补充（不改评价）

正式13×512已结束且全部继续门槛失败。P1-combat/NQ4VEVAL000473及P3-full/NQ4VEVAL000330两条轨迹进入第三幕并死亡（39/44层）；另增这两条完整正版回放，检验实际访问的新深度。不替代原先16条固定样本，不重跑/挑选评价、不更改成功判据，不将这些tune种子转为训练数据。若有回放拒绝/分歧，保存故障并报告，不能将深度当作既有规则资格。

首次调用旧oracle_replay默认实际TestMode=true，不符合本计划；全部原日志保留为legacy诊断，不作正常资格。为工具增加显式--oracle-test-mode（默认off，on仅旧诊断）并核验真实begin头，全部18条在新目录显式off重放，不替代/修改旧结果或正式整局评价。新工具改动后重跑完整门禁。
