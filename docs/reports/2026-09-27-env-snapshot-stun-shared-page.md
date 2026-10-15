# 通用 Env 快照：眩晕、共享事件进度与事件战斗布局

日期：2026-09-27。分支 `feat/generic-env-rf-api`；未提交集成批次。计划：[恢复计划](../plans/2026-09-27-env-snapshot-stun-shared-page.md)。游戏真值 v0.111.0；CombatSolver 保持冻结。

## 问题与原版依据

旧版第一幕原版轨迹普通重放8/8条、2903/2903决策对齐，逐决策导出/导入却只有2/8通过。失败发生在快照校验，并非当时的普通游戏执行差分：4条在怪物被眩晕后报 `monster state index`，2条在共享事件选项后报 `live event synchronizer state`。

- 原版 `Creature.StunInternal` 构造独立的 `STUNNED` MoveState，再交给 `Monster.SetMoveImmediate`。C++将其 `machine.current` 编为 `-2`，`NextMove.stunned` 单独表示合成状态；普通 registry 的 log/follow-up 仍使用有效索引。
- 原版 `EventSynchronizer.ChooseOptionForSharedEvent` 在回调前增加 `_pageIndex`；`BeforeExitingRoom` 只重置战斗同步器，并不清空事件页。C++ 已增加页数，校验却只接受0。
- Punch Off 的原版 `EventCombatSynchronizer.InitializeForEvent` 会在事件选项前建立战斗展示布局并加入怪物；正式战斗进入时才执行怪物状态机建立。新增针对性夹具因此又发现 `inactive monster machine attached to combat` 的合法预备状态被拒绝。

## 修改

1. 用 `MonsterModel::stunnedStateIndex` 标识唯一合成索引；仅在当前状态为该值、NextMove确为stunned且无普通next.state时接受。log、registry follow-up与回调仍校验。
2. 共享事件允许非零pageIndex；空事件和非共享事件保留原校验。不改退出房间时的事件状态。
3. 仅在事件战斗布局已预建、仍处于事件决策、事件模型布局为Combat且怪物的next/machine仍是未初始化形态时，接受 roster中尚无状态机的怪物。正常战斗中的无状态机怪物仍被拒绝。
4. `test_snapshot` 通过普通Stun命令和事件MapMove/EventOption建立状态，验合法状态恢复、合法动作及后继Hash；人为破坏合成索引/日志/事件页/预建标志的状态仍拒绝。

## 验收

| 命令/语料 | 结果 |
|---|---|
| `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`、`cmake --build build -j 4` | 退出0；`/tmp/mcc-env-snapshot-gca-configure.log`、`/tmp/mcc-env-snapshot-gca-build.log` |
| `ctest --test-dir build --output-on-failure` | 13/13通过，含新增生成牌空结果合同；`/tmp/mcc-env-snapshot-gca-ctest.log` |
| `python3 tests/l4/l4_run.py --trace /tmp/mcc-env-act1-endpoint-oracle.jsonl --verify-snapshots --require-complete --verbose` | 8/8一致、2903/2903决策；`/tmp/mcc-env-snapshot-stun-l4.log` |
| 同上，trace换为 `/tmp/mcc-env-act1-neow-oracle.jsonl` | 4/4一致、1003/1003决策；`/tmp/mcc-env-snapshot-shared-neow-l4.log` |

逐决策验证在每个点比较导入Hash和合法动作，并让恢复根与普通根执行同一实际动作比较后继Hash；L4随后把普通根决策/终点与正版轨迹比较。合计12条、3906决策，有限第一幕路径的原失败已解决。正版轨迹为Ironclad A0、hp10000的定向记录，其中11条进入第二幕地图、1条死亡。

完整 `scripts/check.sh` 退出0：13/13 CTest；L2 200局零分歧；L3 80战、1611状态零分歧；L4 80局、4089决策零分歧；Oracle构建0 warning/0 error。原始日志：`/tmp/mcc-env-snapshot-full-check.log`。默认 L4 仍在第一幕 Boss 前停止，允许 Unported；不能据此声称全部房间/角色快照覆盖或真实游戏捕获已完成。

## 仍需处理

第二幕先古Orobas、最终TheArchitect/WinRun、父事件恢复、真实档案及live捕获、跨进程稳定身份尚缺。默认gate允许Unported，完整Env/RF目标继续进行。
