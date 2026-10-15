# Deep-v2 剩余历史与克隆差异

基底 `8b8c820`，分支 `fix/new-seed-run-rules`。属于 E3/M4 条件补覆盖；项目必读文档已读。API 聊负责通用接口，本聊负责原版差分和首因修复。已冻结的深层2000原件、自然8000原件与原始参考运行时不变。

目标：关闭深层失败复验中的8条规则差异，不把3条容量错误计入通过。

范围：EmotionChip 的 DamageReceivedEntry 非完全格挡谓词；Stomp/Midnight 入战时的 IsClone 判定；CardModel.AfterCloned 的 DeckVersion/CurrentTarget/CurrentPlayIndex/HasBeenRemovedFromState 清理；PoisonPower.Trigger 的 Accelerant 次数（包括 Outbreak 主动触发）；Iteration 的当前 Side CardDrawnEntry 计数；POD 字段遍历及快照。只改模拟器，不改原版、RNG或比较器。

验收：每类原始失败轨迹严格完整复验、逐点快照后继；全部53条旧失败重新执行到EOF并保存首因；`taskset -c 0-2 scripts/check.sh` 到 `all checks passed`；记录源版本、CLI SHA、manifest SHA、未通过项。通过失败集不能代替其余1947身份的全量重放，后续需单独冻结执行；完整API、所有合法集合、双端逐抽RNG尚未认证。

实际结果：53原失败严格完整复验50一致/0差异/3容量，64034/64722决策；最终完整门禁49CTest/L2/L3/L4通过。剩余容量和全2000重放另行执行；本项8条规则差异已关闭。见同名报告。
