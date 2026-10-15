# Q4-w：完整轨迹中的重复休息选项

基线main aeb22f4。Q4-v P3-full/NQ4VEVAL000330在第二幕27层第二休息选项被转换器拒绝，只有301原版决策一致前缀；Env轨迹后续进入第三幕并在44层死亡，未经完整规则资格。本项完成该实际路径的normal oracle验证，使后续训练/评估能检查完整跑局，不能当作新策略收益。

## 真值与实现边界

原版v0.111.0 MiniatureTent.ShouldDisableRemainingRestSiteOptions=false；RestSiteSynchronizer.ChooseOption成功后删除已用选项、保留其余并重排索引，原版NRestSiteRoom重新展示剩余项与Proceed。RestSiteRoom.Exit负责原生skip/completion。这些规则在Env已经存在，本项只修离线脚本/回放适配，不改Env/规则/RNG、公开actor/model/NoSL默认，也不运行新搜索/学习。

- 转换器允许第二/后续RestOption，保持原生当前索引和嵌套Choice顺序。
- OracleRun只在脚本下一动作仍为rest（含当前休息屏药水）时再发休息决策；每次重新读原版Options和IsEnabled，严格拒绝空列表/失效索引。非脚本和原来接map的脚本仍一次后离开，不替换同步器结果。
- L4从原版实际后续decision=rest记录显式离线rest_hold标记，公共CLI oracle adapter只据此省略旧自动Proceed；其他标记/动作严格校验，仍比对所有状态/合法动作/终点，不改比较器或参照值，不绕过原版玩法。card_select和自定义奖励返回休息点时同样保留，单次旧输入默认不变。
- 若休息中药水会被旧转换器错误归到map，仅在明确剩余选项路径补准确屏幕绑定；保留旧普通行为并记录新证据。

## 冻结验证

不重跑Q4-v 6656评价/选checkpoint。冻结其全部18条normal回放输入：首2种子×8hybrid及两条深路径。目标18/18完整真实死亡、每个原版begin TestMode=false、全部合法动作/公开字段/HP牌组遗物药水/RNG和终点按原L4严格一致；必须单独验证330的全部后续和真实终点，不把前缀当成功。其余17条旧成功script/oracle必须逐字一致，原330前缀301条也保留/核验。无法达成就报告实际界限，不换种子或扩大样本。

至少三类损坏计划：无MiniatureTent的第二选择、动态剩余索引越界、错误嵌套card_select均须normal oracle拒绝，不能静默默认。另原版snapshot restore对修复后的深路径作完整回放验证。保留全部旧Q4-v文件/清单不变。

分支fix/nosl-q4w-rest-continuation；产物artifacts/nosl-q4w；报告docs/reports/2026-10-05-nosl-q4w-rest-continuation.md。先冻结旧输出与源码依据，定向修复/对拍后完整scripts/check.sh，再更新README/roadmap合入main。其他chat脏文件保留；本地nice19/CPU4、构建CPU4,5/两job，一次一个重活。云实例保持shutdown；不需要GPU，不改变高水平整局NoSL目标或旧学习失败门槛。
