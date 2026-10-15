# M5：卡牌增长字段与原状态指纹

分支：`feat/combatsolver-env-backend`。前置：真实组件组装与普通原卡牌指纹已提交 `654208a`，完整原算法 Env 执行仍未开启。

## 目标

使已移植 Rampage/Thrash 的 decimal 增长状态在 C++、live 捕获、克隆、导出/恢复和降级中保留全部位；让 Rampage 消费固定 upstream 的相同特殊指纹编码。继续保留完整原状态键与执行适配未完成的门禁。

## 工作

1. 对照正版 v0.111.0 源码，将整数增长槽改成 POD `Dec`，纳入生成字段/hash/schema。
2. 严格映射这两个正版模型的已审计实例字段，其余子类仍拒绝；没有原生字段类型/身份依据时不设默认值。
3. 查询提供 Rampage 的增长数值，按原 `StateFingerprintBuilder.Add(decimal)` 顺序编码。Thrash 依照原指纹仅编码既有动态变量，不额外改变政策键。
4. TestMode=false 原对象分别执行 Rampage 的小数增长、Thrash 对普通攻击的增长，并比较完整组装后继及源隔离；用原游戏降级结果检查增长恢复。
5. 对尚缺结构化计算语义的 Thrash 计算型目标明确拒绝，避免继续用 CalculatedDamage 的 BaseValue 冒充 Calculate 结果；补齐该路径后才能扩大支持。
6. 增长夹具中 Weak 移除后，按实际 PowerRemoved 事件和仍持有的正版对象捕获当前字段，保持已观察能力的引用/池状态；不使用旧 capture 或 Env 后继伪造移除对象。
7. 原 Beam 新增 Rampage 后继中暴露风险历史缺失；审计固定原镜像分派并保留 Rampage/VoidForm 的原因与顺序，扩展 owned 外层协议和 hash，测试不再排除任何 Risk 条目。完整历史仍明确未完成。

## 验收

- Live root/原指纹差分使用生产源码和正版对象；小数与 decimal scale/符号必须参与核对。
- Snapshot/Clone/恢复纳入新字段，旧 schema 拒绝。
- 原 Beam 的候选/评价/预算不变；不声称同政策完整状态键或独立 Env 搜索完成。
- `scripts/check.sh` 完整门禁、派生 Release 构建、managed/C ABI 合同检查通过；报告记录范围与失败。

涉及：`engine/sim/state.h` 与生成字段、`CardModel.h`、`Rampage.h`/`Thrash.h`、Env 查询与 live 卡牌映射、原指纹投影、差分工具和状态文档。
