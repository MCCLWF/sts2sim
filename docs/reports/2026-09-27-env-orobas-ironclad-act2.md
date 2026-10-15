# 战士二幕 Orobas 路径移植与差分

日期：2026-09-27。分支 `feat/generic-env-rf-api`。计划：[Orobas](../plans/2026-09-27-env-orobas-ironclad-act2.md)。真值为 v0.111.0 `Orobas.cs`、`AncientEventModel.cs`、`TouchOfOrobas.cs`、`ArchaicTooth.cs` 和正式 Oracle。

## 本批实现

- 原版顺序创建 pool1 的两个候选、PrismaticGem/SeaGlass 第三候选，再抽 pool1；之后创建 pool2 四候选并抽取；最后创建并设置 Touch/Tooth 后抽 pool3。事件局部 RNG 先从全解锁角色列表去除当前角色后抽一项，再执行 0.3333333f 的分支浮点抽取。未选中候选也照原版分配。
- `EventModel::RelicOption` 增加接受已存在可变遗物实例的通用重载，动态保存属性无需复制成第二件遗物。Touch 保存起始/升级遗物身份，Tooth 保存原始/替换卡牌事实；带附魔的 Tooth 设置尚明确 `Unported`，不伪造复制。
- SeaGlass 保存抽到的角色身份；Touch/Tooth/SeaGlass 的快照字段补有限引用/范围校验。SandCastle 的独立获得效果由 GPT-6 Luna xhigh 对照原版移植并集成；额外获赠 SandCastle 的定向原版路径1局/172决策零分歧，见[专项报告](2026-09-27-env-sand-castle.md)。

## 验证

原阻塞轨迹 `/tmp/mcc-env-act2-oracle.jsonl`，Ironclad A0、hp10000、无Neow、SEED0002/policy1，原先在244/564决策以 `Orobas.GenerateInitialOptions` 停止。移植后 `python3 tests/l4/l4_run.py --trace ... --verify-snapshots --require-complete --verbose` 通过Orobas选项及实际选择Driftwood，在 **455/564** 决策停于下一独立未移植事件 `WelcomeToWongos.GenerateInitialOptions`，此前零分歧；原始日志 `/tmp/mcc-env-orobas-act2-l4.log`。这包括每个已到达决策的快照导入/合法动作/实际动作后继验证。

后续事件接通后，同条真实轨迹已到第二幕Boss战死亡并完整比对564/564决策，见[Lost Wisp 报告](2026-09-27-env-lost-wisp.md)。本条路径仅取得 Driftwood。Orobas其他候选被选中后的效果、带附魔的 Tooth 设置、各候选池更多随机分支及非全解锁档案尚未验收；不能称 Orobas 完整覆盖。完整门禁仍待执行。
