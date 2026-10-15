# Q4-u：冻结编码器的动作交互学习

基线main abd948f。Q4-o 57个真实收益根的公开标签中，X1训练一致率40.46%，旧小步全模型BC8轮后仅43.59%；固定独立整局提升未过门槛。Q4-r重复PPO仍0胜；Q4-t已修复教师回放，但不重算旧实验。本项独立检验动作评分与干扰，而不扩采旧BC、改变规则或宣布教师拟合即高水平。

## 实现与信息合同

- 新增独立action_interaction默认关闭：将source*target逐元素交互通过零初始化无bias投影加到原action_kind+source+projected_target+number。两者均为现有公开表编码，不运行未来规则、伤害模拟、查询或隐藏读取。新投影初始权重全零，原有参数逐字保留，启用初始输出与X1一致。
- 仅训练动作policy MLP。编码器、各种embedding、目标/数值投影、value等所有其他参数冻结。离线预先缓存教师/anchor公开编码；缓存加速只省重复前向，不作actor新信息。
- Python/原生格式显式记录开关，旧格式1/2/3保持原行为；新格式4明确features、power_context与action_interaction，未知标志/shape拒绝。非零交互与原生前向、变序/padding、无目标为零、非法动作屏蔽均核验。不得因只做Python训练而遗漏部署格式。

## 固定实验

只使用冻结Q4-o合格public-tables.npz（SHA 8d30a2c7e3ce51c7c0bb73551d062671a4892b5f5d3f241959864d0d74599f29）及X1（SHA 6fb5f1955a11d55175d348659460ce459017baa018f7cf9242a671ba19266703）。原始train/held划分与等根权重不变（55训练根、2留出根），不读取评价集训练、不重抽held。只观察旧元数据，不重复搜索或重新标签。

登记C1/P1/C3/P3四臂，C现有动作头，P新增交互；学习seed1/3，每臂fresh Adam lr3e-4、64epoch、batch128、梯度裁剪1、根CE+.5(KL教师+KL anchors)。所有编码器缓存相同，样本/anchor抽样同seed一致；固定最终第64轮，不按拟合/held挑epoch。记录逐轮CE/一致率、root/anchor KL、每类漂移与全部冻结参数SHA。两个held根只能作诊断，不能声称泛化。

五个最终策略含X1在512个新tune种子NQ4UEVAL000000..000511、A10/filter1、greedy完整评价，保留所有胜/死及error/unported/hang。C用于分离“更多动作头更新”的效果，P-C用于交互效果。每P须比同seed C和X1均至少+.5层，全部对应逐seed配对95%区间下界>0、无新增故障，两个seed均通过才继续该交互学习路线。C两seed均比X1满足同样判据才称动作头单独学习正信号；否则不扩大该方案、不替换X1。记录胜率/过幕，层数门槛仅用于下一学习计划，不是高水平验收。

## 执行与交付

先本地nice19单CPU完成公开缓存/冻结参数和非零原生合同，再运行完整scripts/check.sh。若使用已授权pro-7912450e5767，源码/权重/数据冻结后再启动；独立关机保护最多2700秒（准备/4短学习/5评价/回收），一次一个学习/评价任务，取回所有证据SHA核验并API确认shutdown后解除保护。凭据只留本地用户文件。

分支feat/nosl-q4u-action-interaction；产物artifacts/nosl-q4u；报告docs/reports/2026-10-05-nosl-q4u-action-interaction.md。更改model/export/native policy、独立诊断/实验driver；不修改PPO默认行为、公开schema或任何游戏规则。更新README/roadmap，保留其他chat性能段落/文档及CLAUDE.md删除。真实高水平Silent A10完整NoSL/Linux目标保持active。
