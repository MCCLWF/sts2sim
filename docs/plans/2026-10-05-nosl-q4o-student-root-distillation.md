# NoSL Q4-o：真实学生战斗教师蒸馏

基线 `0d5a20f`。Q4-n 的原始 0.50 教师在 15/16 真实学生根完成正常规则搜索、原版执行和
Env 差分，9 根至少多保留 3 HP；此前人工/起始牌组教师 BC 没有净收益。本轮检验教师的
真实学生分布标签能否以有界更新改善冻结 X1 的整局水平，不改公开输入或训练奖励。

## 实现与合同

1. 提取 CLI 原来的动作输入适配为共享 helper，行为不变。新增独立离线导出器：回放完整
   原版 trace 的动作，重建公开 Observe/ActorFilter 历史，从教师 root 起每次真实 Env 步
   输出公开三表/动作 row；包括 subset 选牌的数量与有序选择。只输出实际可选动作，过滤
   拒绝明确失败，单动作 forced 不训练。原始规则/随机性仍由 Env 执行。
2. 先导出 Q4-n 的 15 个合同合格根；保持 int32，不沿用旧 BC loader 的 int16 clipping。
   对每个原始动作验证标签 row 映射；原始 trace 的每决策和 terminal 已与正版核对。
   抽样重新用导出的动作 row 回放，公开表/hash/状态一致；隐藏扰动观察不变。
3. 冻结最多 128 个新训练种子 `NQ4O000000`–`NQ4O000127`，同样选第 8 层起首个战斗。
   原始 C# beam30/nodes10000/软10秒/DOP1/进程90秒，全部尝试、失败和无根保留；
   不换失败种子。至少 96 合同合格根且 >=25% 根改善 >=3 HP 才正式更新学生。
4. 只将合格且 >=3 HP 收益的教师路线用于 CE。按 seed SHA 的 10% 根留出，根内行等权
   归一，避免长战斗支配。其余冻结学生轨迹按固定规则取公开 combat/strategic anchors。
   actor 只读公开表；gain/seed/hidden state 属于离线采样/诊断元数据，不作输入。

## 小规模学习与预先判据

- 从同一个 X1、features3/filter1 开始两个随机种子（1、2）的固定 8 epoch 更新。
  Adam lr=1e-5、batch128、CE + 0.5 KL（教师状态）+ 0.5 KL（固定学生 anchors），梯度裁剪1。
  冻结 value head；共享 encoder 的变化可能改变 value 输出，不能宣称整个 critic 冻结。
- 记录教师 train/held CE/一致率、公开 anchors KL、参数/有限值和每类决策漂移。
  不通过 held 选 epoch、不在更新中读取评价种子。
- 贪心整局配对评价 512 个新种子 `NQ4OEVAL000000`–`NQ4OEVAL000511`，保持原始权重
  X1 对照。种子不进入标签。每个候选须平均层数提高 >=0.5、seed bootstrap 95% 区间
  下界 >0，且无新增 error/unported/hang，才继续 PPO/扩采；两个种子都通过才称稳定信号。
  这不是最终留出集验收；不替代当前 0/200 dev 胜的产品现状。若通过，再登记后续评价。
- 未过门槛不继续无界 BC/PPO，不用教师一致率代替整局收益；X1 保留。

## 资源与交付

本地先完成导出/回放合同，nice19、轻 CPU、一次一个重任务。若启动已授权
`pro-7912450e5767`，先独立关机保护（最多60分钟），远端冻结源码、X1、teacher/game/
本地化依赖 SHA；凭据只留本地。取回产物 SHA 校验并 API 确认 shutdown 后解除保护。
正常规则尚未支持的 Rolling Boulder 轨迹明确拒绝，不切 TestMode 或丢掉伤害。

代码改动完整 `scripts/check.sh` 后合入 main。分支/计划/报告一一对应；记录全部原始命令、
失败、吞吐与模型结果，更新 README/roadmap 并保留其他会话未提交改动与 CLAUDE.md 删除。
报告 `docs/reports/2026-10-05-nosl-q4o-student-root-distillation.md`；产物 `artifacts/nosl-q4o/`。
主要文件：`tools/cli/oracle_actions.h`、replay_cmd.cpp、独立导出器、Python 教师采集/训练工具。
高水平 Silent A10 完整 NoSL/Linux 目标仍 active，只有实际产品要求全部满足才完成。

## 开始正式采集前的上游刷新

2026-10-05 17:03 UTC 对上游 HEAD 的读取发现 0.50.0 发布提交之后已有新主线
`9a4489d840a3772aef451d5c6dc42dd79719f380`（PR #213 和 forced potion continuation 修复）。
按用户“拉取最新版本并使用它”要求，在正式 128 根采集前改锁该提交，保留旧工作树与 Q4-n
历史证据。重新构建原始 DLL/正常规则宿主，先用 NQ4N000000 验证远端依赖和教师合同，
再按原来固定种子、预算、门槛和评价采集；不把旧教师标签混入正式训练。上游辅助宿主代码
未变，NoSL 的正常规则 bootstrap、公开输入和动作回放保持原样。
