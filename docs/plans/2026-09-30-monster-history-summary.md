# 怪物 StateLog 的等价 POD 表示

初始基底 f63ae51；分支收拢后对齐 main 73d053e，在现有 wip/monster-history-summary 工作区继续；属于 E3/M4 长局可达状态补覆盖。必读文档已读，真值仅为 v0.111.0 反编译源码与正版DLL。API聊确认目前没有全StateLog列表查询或live-root映射；不改另一聊天的树。

原始首因：deep-v1 MA73D889EDE60DF/REGENT/A10/p535937513 和 deep-v2 DC209E17DB81C54/IRONCLAD/A0/p45722707 均在第1025回合追加怪物历史时超过1024条。原版无该规则上限。另两条活卡池容量不在本项范围。

先验证所有原版消费者：RandomBranchState.Contains(UseOnlyOnce)、连续重复尾部、过滤IsMove的末尾cooldown、Creature.StunInternal/Illusion/Flutter.Last。拟用曾见状态mask、最后条目与连续重复长度、逐状态最后Move距离和总条目数保存这些谓词，不保存无限列表。全部数值保留原版int32/float比较语义；不删除历史对后继规则的影响、不增加RNG调用、不缩比较字段。登记内部表示偏离和公开可观察限制；旧schema必须明确拒绝。

验收：正版DLL私有GetStateWeight参数化输出，与C++摘要在短/长历史、non-Move条目、once/repeat/cooldown组合逐点比较；两条原始>1024回合完整轨迹与逐点快照后继一致；快照验证包含mask/last/count/age约束、格式拒绝；完整taskset -c0-2 scripts/check.sh。源/CLI/manifest和命令保留，未通过项如实报告；完整原版StateLog列表不是本项公共API承诺。

2026-09-30 跟进：对齐 API 的 ChoiceOption.enabled 后，冻结 0c6c596 完整复验旧深层 2000 身份。保留其原始 TestMode/headless 资格，所有源/CLI/manifest 固定到 EOF；5 条原采集连续第二次 REROLL 不改动作、不计通过。复验后加强 seen_moves 必须精确等于 seen 中的 Move 状态，新增拒绝遗漏 Move 的快照用例，再执行最终完整门禁和两条长局。旧 CLI/中断批及压缩输出按 SHA 归档；不创建额外工作区。
