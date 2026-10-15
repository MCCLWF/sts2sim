# 跨 native runtime 的完整 Env 快照

基线：`73d053e`，分支 `feat/env-rf-complete`。这是 E1 的必要工作，不缩减既有 E0–E5 验收。

当前 v4 的定义/眩晕回调是 runtime registry 下标，LocKey 字符串也在全局表；
删掉 runtime_id 检查会让合法导入后的效果和查询错误。保留 v4 合同，增加显式 portable 协议。

1. 为所有 production FSM move/condition/weight/intent 和 stun 回调建立构建内稳定符号，
   不序列化指针，不执行效果去猜回调，不改变游戏 RNG/调用顺序。
2. 以相对源码内容、生成内容/schema、编译器/平台/浮点配置绑定规则构建身份。
3. 携带实际 immutable FSM 定义（包括创建时数值）、LocKey 字典；快照内 registry 下标
   按符号/定义排序规范化。恢复映射到本地 registry，重绑定 pending operation 的 base hash。
4. 延用完整 POD 解码、稳定根校验、Choice/Boundary 前缀证明；未知符号/构建/定义/字典拒绝，
   C ABI 失败无 successor 且不改变 diagnostic Env。
5. C ABI discovery、Python/.NET SDK 暴露明确 portable 导出/导入。原版 live capture 另行验收。

验证：新进程、不同 registry 预热顺序、五角色/多个遭遇/全部 callback 类型、stun、事件文本、
typed Choice/Boundary 的后续执行与 RNG/规范化快照对齐；畸形定义、旧 runtime v4 和版本错误拒绝。
实际共享库消费者及完整 scripts/check.sh。跨构建/跨平台迁移不在此兼容承诺中，明确拒绝。
报告区分实现、有限证据和未验收条件；完整目标继续需要 RF 全矩阵、profile/live capture/trace/规模。
