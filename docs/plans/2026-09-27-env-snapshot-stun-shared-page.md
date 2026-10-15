# 通用 Env：恢复眩晕与共享事件进度

日期：2026-09-27。分支 feat/generic-env-rf-api；基线8a93c1f及已有未提交跨幕批次。E1实际恢复修复，CombatSolver保持冻结。

## 原因与目标

第一幕实际正版语料普通重放8/8、2903决策通过，但逐决策快照恢复2/8通过、6/8失败。合法合成STUNNED状态(-2)和共享事件非零pageIndex被当前校验拒绝。必须按已实现编码与原版生命周期修复，不能清空状态或放宽全部索引。

真值：v0.111.0 Creature.StunInternal、MonsterModel.SetMoveImmediate、MonsterMoveStateMachine、EventSynchronizer.BeginEvent/ChooseOptionForSharedEvent/BeforeExitingRoom。

## 实施

1. 明确共享的眩晕当前状态编码，只在独立NextMove确为stunned且无普通registry state时接受；普通current、log、follow-up继续按registry校验。
2. 共享事件允许其pageIndex进度；非共享和空同步状态保留零页/投票结构要求。退出房间不额外重置canonical event/page。
3. 通过普通命令建立眩晕、通过真实事件选项建立共享进度；验证恢复后后继相同，并拒绝伪造编码、log或非共享page。
4. 对有/无Neow实际语料逐决策恢复、合法动作、原动作后继Hash与L4正版状态差分，确认所有原失败消失；遇新失败继续定位，不放宽对拍。
5. 完整scripts/check.sh回归，更新勘察/路线的恢复状态并写报告。现有跨幕、三卡和生成牌非法wrapper批次一起通过构建；最终胜利及完整Env/RF仍继续。

## 文件与验收

主线程编辑 env/snapshot_validation.cpp、game/Models/MonsterModel.h/.cpp、tests/test_snapshot.cpp与文档；统一构建。

验收：2类合法状态可恢复且执行后继相同，针对性非法编码继续拒绝；已有12条第一幕语料3906决策逐点恢复与普通正版对拍零分歧；新旧测试全通过。仅有限路径证明，不宣称全部原版字段/内容/真实捕获完成。
