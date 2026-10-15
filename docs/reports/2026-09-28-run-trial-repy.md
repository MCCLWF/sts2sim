# Trial、War Historian 与 History Course run 批次

日期：2026-09-28。分支：`feat/generic-env-rf-api`。基线：游戏 v0.111.0、commit `41cef1ea`。依赖的事件前一批 `3f4b65b` 已提交；其它角色和附魔任务按用户要求暂停，本批只修改通用 run 级实现。

## 实现

- 按 `Trial.cs` 接入 Accept 的三种 RNG 判决、各两条结果路径、两份卡牌奖励和拒绝后的页面；事件状态保存外层 `trialFormat` / `trialResult` 本地化键。`EntrantNumber` 使用原版 Chaotic 随机源，只影响展示，不消耗 Env 的 gameplay RNG。
- 按 `WarHistorianRepy.cs` 接入单人一把/多把 Lantern Key 消耗、先开笼/箱和第二次开锁；`FreedRepy` 纳入 POD run 状态及快照。箱子提供两瓶药水和两件遗物；开笼取得 History Course。
- 按 `HistoryCourse.cs` 记录上一回合最后一张由玩家打出的非复制攻击牌，下一回合自动复制并打出；该历史事实纳入 POD 战斗状态、快照字段遍历及引用校验。
- 更新旧 Lantern Key 测试：进入 War Historian 后现在应看到两项原版选项，而不是旧的 `Unported` 停止。新增三个定向测试，并重新生成分发及地图源码清单。

## 验收记录

| 命令 | 原始结果 |
|---|---|
| `./build/test_trial_event` | 通过：拒绝页、弃局确认拒绝执行、三种判决各两条结果 |
| `./build/test_war_historian` | 通过：单钥匙、两种双钥匙顺序 |
| `./build/test_history_course` | 通过：前回合攻击回放与快照往返 |
| `DOTNET=/home/wst/.dotnet/dotnet python3 tests/l3/l3_combat.py --seeds HISTORY01,HISTORY02 --characters IRONCLAD --policy-seeds 1,2 --encounter CULTISTS_NORMAL --relics HISTORY_COURSE --max-actions 80 --verbose` | 4/4 战斗一致，76/76 状态一致，0 Unported，0 分歧 |
| `python3 tools/coverage/map_source_inventory.py --check docs/audits/map-source-inventory.json` | 通过，无差异；普通事件 58/59 有具体头文件，仅 DeprecatedEvent 缺失 |
| `scripts/check.sh` | 通过：20/20 CTest；L2 200 局；L3 80/80 战、1611/1611 状态；默认 L4 80/80 局、4089/4089 决策；各定向完整路线均零分歧、零 Unported |
| `git diff --check` | 通过，无输出 |

## 限制和后续

Trial 的 DoubleDown 打开原版弃局确认弹窗。Env 尚无弹窗的确认/取消决策，所以该动作明确抛出 `Unported`；玩家可用 Reject 页的 Accept 继续。内层 `TrialStory` / `TrialResult` 原文需要动态本地化插值 API，当前只保留外层描述键和内层键哈希，尚不能逐字展示该段文字。War Historian 的 `CompleteQuest` 只记录原版 run history，当前 Env 未导出该展示历史。Trial/War Historian 新路径尚未被真实整局 oracle 路线逐项覆盖；定向 C++ 测试不能代替这个证据。默认 L4 仍允许 `Unported`，本批不宣称 run 级内容完整。
