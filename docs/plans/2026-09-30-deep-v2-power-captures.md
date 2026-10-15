# Deep-v2 Power 出牌捕获

基底68b92bb，当前fix/new-seed-run-rules工作树；属于E3/M4条件补覆盖，所有项目必读文档已读。另两树分别冻结8000自然原件复验和2000新高血量组合，工作不改其源码/运行时。

首因DD39B3661521B08/DEFECT/A10/p761454121/HP10000在第747点：MadScience skill本体block10后原energy4，旧C++8且多channel Lightning；Storm/Subroutine误把CardState.extra[0/1]当各power Dictionary捕获并对非Power读取/清零，破坏MadScienceType/Rider。额外两条Ironclad在Hemokinesis/Breakthrough自伤时Rupture提前加Strength；原版按CardModel字典缓冲当前卡自伤，AfterCardPlayed才Apply。

任务：使用已存在POD ActiveCardPlayPowerCapture按(power,card)保存原before量，清除一次；Rupture Add卡0并按source card/当前power amount累积，未在字典的自伤立即Apply，finish取出缓冲量。保持duplicate-Add原错误、嵌套不同卡/不同power隔离、cards.extra无改动、GC/快照/前缀后继；只改模拟器，原始Oracle不变。

验收：原失败完整输入+逐点snapshot后继，原版private Dictionary probe与组合源golden，真实有关条目测试；完整scripts/check.sh。深层批仍可能暴露其它差异和容量失败，报告实际未修项，不把通过当前首因当完整内容验收。source/helper/Storm/Subroutine/Rupture与测试/docs是范围；API聊负责公用接口和真实capturing。

实际批次已结束2000条：1947一致/45差异/8执行错误。候选三个power原语和快照测试通过，全部53失败原件复验38一致/12差异/3容量；完整门禁49CTest、L2/L3/L4通过并到达all checks passed终点。详见同名报告。其它首因单独处理，本项不扩改原始Oracle动作或比较规则。
