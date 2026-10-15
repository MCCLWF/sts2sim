# 通用 Env：战士二幕 Orobas 阻塞

日期：2026-09-27。分支 `feat/generic-env-rf-api`。属于通用 Env 的战士整局闭环；CombatSolver 移植冻结，其他角色新内容暂缓。

## 原版真值与当前失败

固定 v0.111.0：`Models/Events/Orobas.cs`、`Models/Relics/{TouchOfOrobas,ArchaicTooth,SeaGlass}.cs` 及 `AncientEventModel.RelicOption`。实际 Ironclad A0、hp10000、无Neow二幕轨迹 `/tmp/mcc-env-act2-oracle.jsonl` 在第244/564决策停止于 `Events.Orobas.GenerateInitialOptions is not ported`。原版选项包含 ElectricShrymp、Driftwood、ArchaicTooth，实际选择 Driftwood。

## 实施顺序

1. 逐行移植正常 `GenerateInitialOptions`：完全解锁档案的其他角色列表按原版次序取一项；事件局部 RNG 依次抽 `NextItem`、`NextFloat`、pool1、pool2、pool3。构造各候选时保留原版 `ToMutable` 分配顺序，不能只生成选中遗物或过滤未实现候选。
2. `RelicOption` 接受已有可变遗物实例，使 Touch/Tooth/SeaGlass 的保存属性在生成选项前设置且不重复分配。Touch 查第一个起始遗物；Tooth 查牌组首张可转换起始牌并按原版生成对应替换牌及保存数据。超出已实现语义的分支明确 `Unported`，不写近似效果。
3. 先让实际轨迹拿到 Driftwood 并延续到下一真实阻塞；验证选项、局部 RNG、后继牌组/遗物/RNG、逐决策恢复与原版一致。然后逐项补其他可能选中的 Orobas 遗物效果，不能把单条 Driftwood 路径宣称为 Orobas 完整覆盖。
4. 将真实二幕路径纳入明确的严格完整性轨迹门禁；报告新的终点与剩余阻塞。完整 `scripts/check.sh` 仍须通过。

## 判据

原版同种子同策略无差分、无额外 RNG 抽取、合法选项及恢复后继一致。取得未实现遗物时显式失败，根状态不受纯预测影响。战士整局未完成前不启动其他角色内容批量扩展。
