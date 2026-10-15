# 基本 RF 收尾与容量、数值极值

2026-10-01 用户明确缩小交付范围：仅全解锁档案；Env 提供状态输入规范和预测接口，
不开发实机捕获、悬停 UI 或其它实际调用端（测试消费者除外）。先验收基本 RF，再修复容量
和数值极值。此范围替代旧完整 RF 计划中的 live/profile/全因果追踪要求；历史证据不改写。
分支沿用 feat/env-rf-complete，基线 cfe50f4。

## 基本 RF

保留 ABI discovery、六类 typed operation、纯预测 owned successor、Choice/Answer/
Boundary/Resume、事实查询、local/同构建 portable snapshot、Python/.NET 封装。
完成 Affliction 独立可写对象及深克隆/安全回收的正确性；输入明确拒绝不支持的档案。
写清字段、数值编码、引用、RNG、静止根/pending 前缀、版本/所有权/失败规范。
不要求完整 Hook/Task/delegate/对象寿命诊断账本；trace_complete 与 RF 全矩阵资格保持 false。

验收：完整 scripts/check.sh；公开 Python/.NET 合同；关闭 TestMode 的遗物、变牌、
Driftwood 重抽、Courier 购买/补货/删牌、水晶球对拍；两种 registry 预热顺序的新进程
从 portable 初始状态实际执行并续接全部有/无选择操作。冻结源码、构建和 Native 输入。

## 容量与数值

用普通小状态快速路径与可增长存储替代没有原版上界依据的定长容量。扩容后引用稳定，
复制/克隆及选择重放拥有独立状态；快照、Hash、GC、临时容器、动作/查询与 SDK 一起处理。
不能把常量从1024改成8192当作完成。资源或原版类型极限明确失败，不能截断。

审计原版 int 与窄字段；修复 X 费、星数、重放、附件/历史计数及 C++ 有符号溢出，
保留原版 clamp、checked/unchecked、decimal、浮点规则，不统一改 int64 改变原版。

验收：真实旧容量失败原件、Monologue 原生动作边界、大状态 C ABI/Python/.NET 恢复/
预测/选择、跨扩容克隆独立性、原版数值边界探针、完整门禁及常规/大状态性能测量。
容量和数值独立提交/报告；基本 RF 通过不代表整个目标完成。
