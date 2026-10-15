# 完整 Env 因果执行追踪

基线b3dad8f，分支feat/env-rf-complete。完整E2/E0目标不缩减为当前已有事件列表。

1. 特权RNG调用观察：原版标量调用、Gaussian内部重试、随机下标/权重/shuffle父子链；
   参数/返回按原始类型逐位保存，前后Xoshiro状态与counter，实际run/player/event/monster/encounter流归属。
   observer在owned执行作用域中；GameState/Rng仍POD，trace关闭不收集，容量失败无successor且原根不变。
2. 所有generated model override/Hook实际调用及原版命令的因果帧：来源模型/实例、开始/结束顺序、
   父子关系、Choice/Boundary前缀重放语义；不另写随机算法/Hook执行逻辑。
3. 创建/退休/引用重用、移动牌堆、资源/奖励/商店/事件页/球/伤害等完整事实；退休对象仍有稳定身份与被冻结值。
4. C ABI discovery、显式特权flags、Python/.NET SDK与消费者。与普通Observe的信息边界分开。
5. 正版DLL独立追踪探针、公共API矩阵、trace on/off后继逐位一致、前缀重放/容量失败、完整scripts/check.sh；
   关闭/开启/ABI的性能分别记录。只有全部事实/来源/对象寿命条件验证后才能声明trace_complete。

本轮先接RNG基础与真值探针，之后继续Hook和对象寿命，不把第一步完成写成完整trace验收。
旧portable构建/原始证据保留；native源码变化重新导出快照与验收。另一个对话负责seed规则/容量，不修改其冻结树。
