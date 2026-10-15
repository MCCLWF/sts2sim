# CombatSolver Env：安全终局预测 scope

日期：2026-09-27；分支：`feat/combatsolver-env-backend`；固定子模块 `d231e9e51a0e58d6bfa1373c6265cce13ffd9a45`。前置：`331f0d3` 的 TestMode 关闭整根、Strike 与两次 EndTurn 有限夹具通过；安装游戏部分档案拒绝，正式同算法 Env 后端仍未完成。

## 已实现的执行与输运合同

普通 Env 用于实际跑局，胜利后执行战后回复、清理和奖励；原搜索在安全胜负点只记录 terminal，保留评分所需状态。新增 `CombatPredictionSession` 在每次独立 transition/choice replay 期间用 RAII 安装临时 boundary，作用结束即清除。指针不进入 POD GameState、不复制到新 Sim。终局保留实际玩家回合与 Victory/Defeat，清 PendingLoss 和 IsInProgress，不伪造 RunPhase::Ended。

专用 C ABI `sts2_combat_prediction_*` 和托管 `EnvCombatPredictionSession` 使用独立 owned 句柄。Clone/Predict/Restore 返回新 owner，失败的后继为 null；`TryPredict` 区分 Ready/Unported/Error。终局无合法动作，继续 Predict 拒绝。raw handle 由调用者串行访问，托管 wrapper 用同一把锁保护调用、读缓冲和 Dispose。初始化 native 库按程序集加锁且幂等。

`sts2env.combat_prediction` v4 外层包含 format/version/inner_snapshot/terminal/history/history_projection_complete/replay_terminal_base/prediction_hash；内层仍是完整 protocol-v2 状态。hash 使用独立域，含 terminal、部分政策历史与选择重放基线。v4 历史条目包含卡牌/出牌快照、伤害来源和完整攻击命中结果。Answer 同时回滚状态与整个 boundary，再重放答案前缀。专用恢复允许 terminal 的非 Play/敌方侧；普通恢复严格要求 active 决策。缺字段、新字段、错 runtime/schema/hash、无效动作或不一致选择基线拒绝。普通游戏快照的待选择状态缺少历史动作起点，专用会话拒绝直接导入。当前 `history_projection_complete=false`，不能由这部分历史授权正式搜索。快照是受信特权数据；hash 检查输运完整性，不证明已经清除的历史 PendingLoss 来源。

## 实际发现与修复

- 药水原路径缺 ActionExecutor 动作尾部胜负安全点；专用 scope 的 UsePotion 完成后运行 AfterPlayerAction。
- EnemyEnd 的胜负截停后继续 SwitchSides 会增加玩家回合；专用 scope 的已锁 terminal 直接返回。
- 死亡或逃跑的怪物 pool 对象保留注册状态机布局与 NextMove，原 decoder 的 absent-machine 要求错误拒绝合法终局。现在只允许 detached 且不在 active allies/enemies 的对象保留布局，仍逐项检查模型归属、state indices、callback；active roster 的 absent machine、未知 definition/index/callback 仍拒绝。
- VoidForm 卡牌与能力此前未移植；现按正版源码实现强制结束回合、手牌/Play 费用减免、手动末次计数、参与者回合重置。`HideTemporaryZeroCostVisual` 虽名称为表现 helper，却写入费用 Hook 使用的 `999999999`，完整保留。能力 counter 映射现有 extra[0]，无新增 POD 字段，live mapper 只忽略确切 `_vfx` 引用。

## 验证记录

```text
cmake --build build -j 2
ctest --test-dir build --output-on-failure
10/10 passed

build/test_combat_prediction /tmp/mcc-prediction-fixtures
Combat prediction checks passed: terminal scope, ordinary isolation, turn stamps,
  potion safe point, owned branches and choice replay.

python3 tools/CombatSolverEnv/check_prediction_abi.py /tmp/mcc-prediction-fixtures
Prediction C ABI checks passed: borrowed buffers, strict input, null-on-failure,
  independent owners, terminal restore and choice replay.

dotnet run --project tools/EnvSessionChecks -c Release
Combat prediction API checks passed: independent hash domain, complete restore,
  16 parallel branches, failure recovery, strict protocol and owned disposal.

python3 tools/VoidFormChecks/check.py --out /tmp/mcc-voidform-checks-root
VoidForm hooks: 16 native rows matched (decimal bits, energy/stars, owner/piles,
  manual last-play counter, amount/reset); live counter freeze passed;
  TestMode=false; full_root=false

dotnet run --project tools/EnvLiveCardChecks -c Release
whole live root fixture: seed-free assemble/restore, production Strike and two full
  next-turn successors including shuffle matched; TestMode=false;
  installed_game_acceptance=false

scripts/check.sh --quick
10/10 tests passed; smoke death=94 unported=6; death=100; boss=3 death=97
quick check passed (differential checks skipped)
```

上述 C++ invariants 检查 ordinary BurningBlood 仍从 HP40 回复到46，而预测停留40；包含 locked PendingLoss 的 recovered-HP Defeat、敌方致死与 enemy-end Demise、药水、Discovery、BurningPact/CharonsAshes 两层 Answer 致胜。Clone/Restore/source purity 与状态机严格拒绝均通过。C ABI fixture 来自同构建的实际 C++ 分支；测试进程先注册相同 Cultists layout，仅重绑 runtime_id，玩法 state/hash 原样保留，并先证明 foreign runtime 导入拒绝。这是输运测试，不是跨进程存档支持。

## 原搜索终局差分与新首差

`tools/EnvCombatPredictionChecks` 从 TestMode=false 正版对象组装同一 live 根，调用派生程序集中的固定 upstream Replay，比较 Env 独立动作后的语义投影。原六类已通过：Strike 击杀且 BurningBlood 未回复、敌方致死、手中 Burn phase-one 致死、敌方 start 毒伤致胜、VoidForm/MercuryHourglass 强制推进到 T+1 致胜、FirePotion 击杀。

投影包括 HP/maxHP/block/HpDisplay/attached、有效 powers 顺序和金额、五个牌堆中的模型/升级/附魔/污染、资源/阶段/round/side/敌人列表/药水槽、九条战斗 RNG 的 counter 与四状态字，以及 terminal/flags。**不是完整 shadow GameState 比较**：pool allocator、完整历史、私有模型 sidecar、非战斗流和完整原 state key 尚未对拍。后继来自原 shadow，未用再次捕获 live state 冒充。

扩展的 `enemy_end_poison` 查出固定原生预测的时序偏离：`Expansion.Replay.cs` 在敌方 end 提前 TriggerPoison(player)，死亡 terminal=turn1；正版 `PoisonPower.AfterSideTurnStart` 在玩家 T+1 setup/draw 后触发，Env terminal=turn2。原始失败日志 `/tmp/mcc-combat-terminal-checks-root.log` 保留；比较没有跳过或重写字段。

`VanillaPoisonTrace` 只读观测正版 production StartTurn/Poison listener：独立非致死 HP3→1 fixture 在 T+1、player side、Start phase、五张牌已抽入 Hand 时触毒。它不冒充原 HP1 shadow 终局；死亡后的 OnEnded/游戏结束逻辑不是纯表现，未通过删掉这些逻辑来取得证据。低 HP 边框 factory 的 scoped null 补丁在 Headless 和 O-014 中说明，其余 Hook/伤害/回合方法原样执行。

派生生成源码纠正 Native 预测的生命周期：唯一锚点删除 enemy-end 的玩家触毒，给 ContinuePlayerStart 增加 setup/Side 完成后的独立 Poison stage，保留 suspended continuation 的下一阶段；补真实 None/Start phase，dead player-start 跳过 AutoPrePlay，复用原 PhaseOne 安全点锁 loss。原子模块未改，score/key/候选/剪枝/预算公式未改。毒伤所属回合与 EmotionChip 的统计窗口随正确时序改变，不能宣称修复前后预测结果完全不变。

修复后 **10 个场景严格投影通过**：原六终局、enemy-end Demise 致胜、普通与 Ambergris 额外回合的非致死玩家中毒、T+1 玩家中毒致死。每场 source purity 和完整专用快照 roundtrip 通过。原始结果 `/tmp/mcc-combat-terminal-fixed.log` 与 `/tmp/mcc-combat-terminal-fixed/`；工具 README 提供 stock predictor 首差与派生修复的不同参照说明。

```text
dotnet run --project tools/EnvCombatPredictionChecks -c Release -- --out /tmp/mcc-combat-terminal-fixed
passed strike/enemy/phase_one/poison/enemy_end/forced_end/fire_potion
passed player_poison/extra_turn_poison/enemy_end_poison
vanilla player poison timing only: HP3 to HP1; real AfterSideTurnStart entered
  at T+1, player side, after drawing 5 cards; TestMode=false; shadow_successor=false
combat prediction checks: ... full_state_comparison=false; TestMode=false;
  native_only=False; installed_game_acceptance=false
```

## 仍未完成

P1 专用接口和上述有限边界已验证；多监听器的完整执行顺序、早期 Side callback 与暂停选择组合、Poison 引出的选择 continuation、Accelerant 多次触发尚未在这批夹具证明。完整同算法后端尚未接管。原预测事件流、全部 pure queries、原 Snapshot/BuildStateKey 投影、所有 Replay/choice/round/子 solver 执行入口，以及游戏内安全切换、连续部署与等根等预算性能仍须实现。当前 Env 执行门禁保持 `live_root_and_same_policy_adapter_incomplete`。

此报告未包含新的安装游戏接受或完整 `scripts/check.sh` L2 差分合入 gate，也未宣称最终 mods 成品完成。
