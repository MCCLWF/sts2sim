# Q4-ac：冻结X1的公开结束回合决策学习

基线main ae8f0b0c2ca580fbbb6c6535d72be4369ccc8564。Q4-ab已独立核验阴性并取回关机；不扩大它的配置。其正常前缀揭示免费ParticleWall返手循环，同一4个公开状态X1亦偏好出牌。旧Q4-o缓存有89条训练EndTurn标签，但X1/两个旧蒸馏候选均0/89贪心匹配。因此独立检验：保留X1其他动作排序，单独学习结束回合，是否能改善真实完整A10 NoSL。局部拟合不等于高水平，最终产品目标仍按2026-10-03需求。

## 固定模型、数据与边界

- 原版v0.111.0、正常全解锁Silent A10、公开schema1/features3/filter1不变。正式X1 SHA 6fb5f1955a11d55175d348659460ce459017baa018f7cf9242a671ba19266703、全部原参数/缓冲（含编码器、动作评分和价值）冻结；引擎、原动作filter与原model.py不改。
- 只消费已验证旧Q4-o/cloud/labels/public-tables.npz，SHA 8d30a2c7e3ce51c7c0bb73551d062671a4892b5f5d3f241959864d0d74599f29。原1248教师行/1511锚点、55训练根/2留出根保持原划分；不得让group、held、标签、特权根状态进入actor。只选公开Combat标志、有唯一合法EndTurn且另有合法动作的行：1088训练行/89正例，28留出行/2正例。锚点只用于冻结合同诊断，不新增训练损失或教师搜索。
- 输入为X1公开encoder的run向量128维，加9个公开数字的numeric双尺度18维：当前能量、星数、护甲、存活敌人展示意图的伤害×次数之和、max(意图-护甲,0)、max(护甲-意图,0)、公开手牌数、合法PlayCard行数、合法UsePotion行数。展示意图只是已编码公开数值，非执行后预测；数字和token计数严格忽略padding。
- 学一个146→64 ReLU→1二分类头，seed1/3分别独立。hidden默认Torch初始化，最后weight=0、bias=log(89/999)，初始决策为继续。Adam lr .001，128个固定epochs，minibatch128，普通逐行BCEWithLogitsLoss，不做class权重/过采样，梯度norm上限1。只训练小头，不改原参数，不按留出或中途成绩选epoch/threshold/权重。
- 公开Combat存在EndTurn及其他合法动作时，所有其他旧logit减去其最大值，EndTurn logit改为小头输出s；贪心决策s>0时结束，否则保留旧非EndTurn排序（s=0按原torch argmax行顺序）。这是贪心分层解码，完整softmax分布不解释为二分类头的sigmoid概率。无EndTurn、只有EndTurn、局外等其余行输出完整旧logit/价值不变。部署不搜索、不模拟未来、不做试动作回滚，不加规则阻止循环。
- 新候选独立checkpoint格式显式标记公开结束回合头，需要同SHA的外部X1才能加载；现有通用ActorCritic loader/export不接受这一格式，避免静默丢头。此项是Python公开推理实验；晋升/实机使用前须另做C++/Linux同合同支持和验证。

## 本地资格及预算

CPU4/nice19，Torch/OMP/MKL/OpenBLAS各1线程、float32，一次一重活。冻结cache、两seed小头训练合计最多240秒；保存所有128 epochs但只保留最终权重/完整Adam，两个训练都执行完不因首seed成绩更改。检查缓存vs实际forward一致、原参数逐tensor相同、所有局外/无EndTurn行logit与value相同、其他动作排序/合法padding、初始继续、正/负头控制、保存加载以及明确拒绝错误baseline/schema/非finite头。

正常资格：两个最终候选各4条固定新NQ4ACPILOT000000–000003，greedy，64槽内实际4槽、每候选120秒采样，不补局、不扩大上限；8条所有原始轨迹均进行TestMode=false的Oracle完整终点与逐决策快照核对。不完整/分歧就停止扩大，保留全部错误。冻结教师train二分类balanced accuracy≥.8、正例召回≥.7，且所有合同与8条正常整局资格通过，才进入正式云评价；这是筛选是否值得整局实验，不称泛化/强度成功。留出仅2根，完整原样记录，不据此调参。实现后scripts/check.sh全通过再启动云；首次阈值不通过也不在本项内改配置。

## 固定正常整局评价与继续判据

X1/E1/E3各1024个相同新tune NQ4ACEVAL000000–001023，贪心float32/64槽，完整终局（原30000步上限），总评价900秒；不补种子、不挑中途候选。每次尝试记录全部动作、结束类型、各幕/层/HP、耗时、结束回合次数与其他动作类型、故障。两候选都须全部1024局真实胜利或死亡且0故障/截断，平均层数相对X1≥+.5且以独立run seed为单位的10000次配对bootstrap95%下界>0（rng20261006）；至少一个候选第三幕数量≥X1；两seed固定拟合资格通过，才扩大同方向。实际胜场、故障、CI全部报告，0通关仍不能算高水平。无直接RL本项，与X1对照回答这个固定教师小头组合的净效果，不能分离宣称特征/分层/BC各自因果。

正式每候选首2条、最深终局去重，以及所有真实胜场均做完整正常Oracle回放/快照/全部end.run字段；若没有完整终局，首2条仍保留失败诊断，不能替换。独立复核种子/计数/统计、固定最后权重/Adam和原参数冻结、资格选择；原始证据逐文件SHA封存。

云pro-7912450e5767先确认shutdown，开机前独立1800秒关机保护启动且确认armed，不延长；远端driver1200秒/launcher1500秒，内部固定评价900秒。只传本项源码和已有验证数据/两最终小头，X1及云native SHA保持原基线；不上传凭据。CUDA再检查完整公开forward、原参数/输出冻结合同，不重训/选择新权重。任务终止后只打包一次，全部文件/归档SHA和大小核验后立即关机，API确认shutdown才解除保护；任何失败也保持有界关闭及证据回收，不因观察超时重启同一活跃任务。预计新增<120MiB，不清理旧证据或用户资料。

## 文件与完成记录

分支feat/nosl-q4ac-endturn-gate；新增python/nosl/endturn.py、tools/nosl/check_endturn.py、tools/nosl/endturn_experiment.py、tools/nosl/verify_endturn.py和本项报告。复用正常Oracle工具，可抽取共同的严格单轨迹验证函数，但不能弱化已有比较。README/roadmap只写本项段落，其他对话的删除/性能文档不暂存、不覆盖。旧Q4-z/aa/ab封存目录只读。正常资格与固定评价通过只确定后续路线；高水平完整胜率与Linux实机NoSL仍需要独立验收。

活动评价期间的只读诊断补充（不改任何训练/评价/继续门槛）：仅消费Q4-ab已冻结且有正常前缀资格的第190/200/256/511公开状态，对固定E1/E3和X1进行CPU float32推理。只用global/tokens.row/actions重建四张公开表，不读info/actual_encounter等诊断字段；保存头logit、贪心动作及源文件SHA。不搜索、不采样、不修改权重，也不把这4个状态的选择当整局或故障原完整路径已解决。
