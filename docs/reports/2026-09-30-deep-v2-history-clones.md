# Deep-v2 剩余历史与克隆差异

基底 `8b8c820`，分支 `fix/new-seed-run-rules`，属于 E3/M4 条件补覆盖。原深层批的45差异/8执行错误经过先前 Power 捕获修复及 event/Cascade/Drum 修复，进入本轮时53原失败为42一致/8差异/3容量。本轮未改原版DLL、Oracle、随机数或严格比较器，未合入 main。

## 原版首因

- **EmotionChip，2条**：源码 `LostHpInPreviousTurn` 查 `DamageReceivedEntry` 的 `!Result.WasFullyBlocked` 与上个玩家 TurnNumber，和 AfterDamageReceived 激活状态所查的 `UnblockedDamage > 0` 并不是同一谓词。Buffer 在无 Block 时可令实际掉血为0，前一谓词仍为真。新增独立的当前/上一玩家回合非完全格挡 bool，并保持 Spite 的真实掉血谓词不变。原轨迹2/2、2951/2951完整一致；其中一次增加 Dark6，另一次增加 Frost Block2。
- **Stomp，3条**：`AfterCardEnteredCombat` 的原版条件是 IsClone，不是只看 IsDupe。Juggling 在第三次 Attack Before 阶段复制当前cost=1的Stomp；普通克隆入战必须跳过按已完成攻击次数再次减价。增加 clone_of 检查；同源 Midnight 的同一遗漏一并修正。三条原轨迹3/3、3097/3097完整一致。
- **Outbreak，1条**：原版 PoisonPower.Trigger 本身计算 Accelerant 的 TriggerCount；旧移植仅在 AfterSideTurnStart 外围计算次数，Outbreak 主动调用 Trigger 漏掉额外次数。次数计算移入 Trigger，每次伤害后递减（敌方回合仍只调用一次 Trigger）。原轨迹 poison=9 造成9+8伤害，剩余7，而旧实现只造成9、剩余8。完整复验一致。
- **Goopy，1条**：CardModel.AfterCloned 清空 DeckVersion、CurrentTarget、CurrentPlayIndex，并令 HasBeenRemovedFromState=false。C++ run/combat CloneCard 继承了 DeckVersion，使 BurningSticks 等生成的 Goopy 克隆后续播放也增加牌组原件 Amount。该隐藏多加值在下一场战斗首张Defend体现为20而非19 Block。两个CloneCard入口按源码清理，原轨迹完整一致；克隆后的能力/局部费用/附魔仍保留。
- **Iteration，1条**：CardDrawnEntry.HappenedThisTurn 同时比较 Round、CurrentSide、各玩家TurnNumber。GremlinHorn 在敌方回合抽到 Dazed 时必须重新从首张Status计数；旧计数仅在玩家 StartTurn 清零，沿用了此前玩家回合3张Status数，阻止额外抽牌。SwitchSides 清零该计数，原轨迹1210/1210完整一致。

这些结论来自反编译 v0.111.0 源码与原始正版DLL轨迹。未用仿真输出构造期望，临时诊断日志已经移除。

## 严格失败集复验

全部53个原始失败capture离线重放，保持原始动作，逐点快照恢复后继开启。输入名单为证据根下 `deep-v2-all-failure-inputs.json`；命令为 `python3 tools/differential/batch.py replay --out <新证据目录> --traces <名单展开成53个JSONL路径> --jobs 4 --timeout 180 --cli build/sts2sim_cli`。

结果：**50 identical、0 diverged、3 native_error**，**64034/64722 决策**。50个匹配终点为35 victory@act2、9 death@act2、5 death@act1、1 death@act0。进程已EOF，exit1；exit1来自保留的容量错误，不能改写成全部通过。

证据根为workspace的 `artifacts/legacy/mass-seeds-2026-09-30/`，本批目录 `deep-v2-history-clones-final-failures-replay/`。manifest SHA256 `6bea6251e73184abf0e00c1f37ad8f75022290d9966f3cb396165bbd8d684d69`；源码SHA256 `0842fd77febd5fad15c1fe9923e26e0b3eddd702954c0bfda39980cbc0b95c4d`；CLI SHA256 `ad4f036f0042a3fc6aa35638d2c7e6db9419f5124a854abae6678096bd0167f0`。manifest记录执行时父提交8b8c820与未提交的11个源文件修复；本报告提交只封存相同源码与文档。

## 门禁与限制

初轮完整门禁通过49/49 CTest、L2 200、L3/L4，到达all checks passed。最终源码复核去掉Poison目标死亡后的提前退出，使各次Damage调用与原版循环顺序相同（死目标由CreatureCmd过滤，纯表现等待省略）；最终53条已重新执行到EOF并取得上述相同结果。最终 `taskset -c 0-2 scripts/check.sh` 已EOF，exit0、all checks passed：49/49 CTest；非hero遗物259/259可用；L2 200 runs/0 mismatch；L3 81完整战斗1625/1625状态及2有界probe4/4；L4 103整局18956/18956决策。最终日志 `deep-v2-history-clones-final-check.log`，SHA256 `9684fb04882631739c9625bf631eb63f71c92c7e5bcac4067e710a58ca38cb9b`。构建后的CLI指纹和最终失败复验相同。

3条容量失败：D840A2150523CC0/IRONCLAD/A0/p1676726483 第2023点（round57）和 DE7CEB1029A341C/SILENT/A0/p917732361 第1425点（round51）达到1023活卡；DC209E17DB81C54/IRONCLAD/A0/p45722707 第2343点（round1025）超过StateLog1024。保留原始首因和原件，不删除活卡、不截断历史、不放宽比较。

原深层2000的其余1947身份尚未在本版复验，本报告只证明53失败集的结果；后续全量批必须单独冻结并执行到EOF。自然8000在68b92bb下已有完整复验，其首幕死亡资格不代替本轮后幕资格。原生完整合法集合、双端逐抽RNG、任意原版live执行点恢复和完整Env/API仍未认证。
