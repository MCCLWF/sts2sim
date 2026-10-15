# Q4-t：真实教师路线消费与独立执行预算

## 依据与范围

基线main310b026。Q4-s正常规则32学生轨迹一致；13条预测存活Boss路线中8条合格，4条因为原版自动选牌没有调用ICardSelector而留下教师token，1条因为替代路线被原学生script.Count决策预算截断。本项修复采集消费者，不重新搜索、采样或训练，不改Q4-s阴性判据与原始结果。高水平Silent A10整局/Linux NoSL目标仍active。

## 最小实现

1. 在Headless中增加可选的只读FromHand完成观察：原版方法照常执行、返回原Task和原卡牌，绝不覆写参数、选择、RNG、规则分支或异步结果。只在本宿主同步selector下观察已成功完成的Task；记录调用前已消费选择序号，人工回调已经消费的分支不重复消费。
2. 正常规则自动分支返回全部候选后，RoutePlayback逐项核验计划token的CardId/升级/完整StateKey/来源与选项occurrence、数量及顺序，再消费对应token。非空无计划、错误身份或不支持的异步路径明确拒绝；不清空未消费队列。观察器仅在教师路线作用域安装，并在finally解除。
3. OracleRun增加仅可由离线宿主设置的可选ScriptDecisionLimit；默认仍script.Count。教师替代路线预算为原脚本数量+计划动作与选择token数量+1，checked计算；真实Stop仍要求完整计划执行、玩家存活、敌人死亡、准确HP。保留原版动作队列、普通流程、资源/RNG比较，不将max_decisions当作成功。
4. 输出自动消费数和实际替代预算，原学生输入与教师计划都保持冻结，actor/学习代码不变。

首次固定回放后新增两个已证实的修复约束：观察器改装到 FromHand 异步状态机 MoveNext，保留公开入口与原成功八条的调用来源元数据；终局 Proceed 标记仅由 CombatRoom 奖励屏设置。NQ4S000008 在完整65动作后暴露的 act1/act0 差异，首因为第16层 TinyMailbox 自定义休息奖励留下的旧 Proceed 标记跨过 Boss 奖励处的 ScriptEnded，并非 C++ 战斗规则或奖励 RNG 差异。依据原版 RewardsSet.Offer 的 terminal 标记与 NRewardsScreen.OnProceedButtonPressed 的 Boss 下一章投票条件，不放宽终点或改变停止边界。

## 固定回放与验收

- Q4-s全部13条预测完整胜利路线，使用原来的script/teacher.json/root-before，不重复搜索；5条失败与8条已通过控制均在新目录回放并严格核对Env每决策/terminal/RNG。5条须完整执行并命中预测HP，否则保留新首因，不放宽比较。
- 8条原合格控制的oracle trace逐字一致；正常规则32条冻结学生脚本与Q4-s原trace逐字一致，证明默认脚本及游戏逻辑无变化。
- 针对已证实自动分支NQ4S000001复制冻结路线，分别破坏选项occurrence、来源occurrence、StateKey、缺失token以及完整顺序，均须严格拒绝，不修改原始文件。不以负例拒绝替代真实路线成功验证。
- 本地nice19、单CPU、每宿主90秒、一次一个回放任务；不启动云实例。全部原始证据写artifacts/nosl-q4t，保留Q4-s 416文件SHA；记录失败、命令、实际动作数/自动消费、预算和资源。
- 正式数据不作为新强度样本：这些是旧训练路线的修复验证，不能重算Q4-s扩采通过，不更换X1。下一学习/搜索预算必须另立计划。

## 交付

分支fix/nosl-q4t-teacher-playback，主要文件tools/Sts2Headless/Headless.cs、tools/Sts2Oracle/OracleRun.cs、tools/NoSLTeacherHarness/Program.cs与RoutePlayback.cs、独立回放检查工具。真实数据回放/负例后完整scripts/check.sh通过再合入main；报告docs/reports/2026-10-05-nosl-q4t-teacher-playback.md，更新README/roadmap及相关pitfalls。保留其他chat未提交性能文档/段落与用户CLAUDE.md删除。
