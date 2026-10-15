# M5 P2：原搜索可打出政策的有限探针

日期：2026-09-27

分支：`feat/combatsolver-env-backend`

前置：P1 独立预测 session 和 P2 游戏 getter 查询已通过。目标是把原 `CombatPredictionSimulator.CanPlay` 与 `SimulatedCombatState.CanPlayCard` 的核心读取语义和游戏 getter 区分，先在现有 live 根核对，不宣称政策投影完成。

## 来源与实现

固定 upstream `CombatPredictedCardExtensions.cs`、`CombatPredictionSimulator.Card.cs`、`SimulatedCombatState.cs` 的实际顺序：先 `IsCardPlayPrevented`，再 Unplayable；遇到这两类早拒绝时两个返回费用均为 0；X 能量费用取 owner 当时能量，X 星费取 owner 星能，普通费用归一到非负；剩余能量不足时查询额外星能换能量；随后是 `HookMirrors.ShouldPlay`、`CardIsPlayableMirrors`、Smog 判定。不能用游戏 `CardModel.CanPlay` 的聚合 reason 或游戏 getter 的 X 费用直接替代。

`ExportCombatPredictionPolicyProbe` 在私有 Env Clone 上计算这一核心判定并输出 `sts2env.combat_policy_probe` v1；C ABI `sts2_combat_prediction_export_policy_probe` 和公开 owned `CapturePolicyProbe()` 携带源哈希与 `policy_projection_complete=false`。根、模型和查询只读门禁与游戏 getter 查询相同。探针不生成候选、不做目标排序、不计算风险/威胁/评分，不开启搜索。

## 验证

运行 `python3 tools/EnvQueryChecks/check.py --out /tmp/mcc-p2-policy-probe-public-final`：TestMode=false 的 baseline、empty_resources、blocked_hook 三个完整 live 根，逐卡比对固定 upstream 原 `CombatRootSnapshot.ForkSimulator` 的 `CanPlayCard`、返回能量和星费，三组均通过；同时四组游戏 getter 逐字段对拍仍通过。探针 C++ 静态入口重复调用/Clone/Unported 拒绝及源快照不变通过；公开托管接口逐卡比对、源哈希/释放后 owned 文档通过。`play_pile` 仅用于游戏 getter 边界，没有将非稳定 live 牌堆当作原搜索政策根。

另执行 `build/test_combat_prediction /tmp/mcc-p2-prediction-fixtures && python3 tools/CombatSolverEnv/check_prediction_abi.py /tmp/mcc-p2-prediction-fixtures`：C ABI owner、协议和源不变通过。执行 `~/.dotnet/dotnet run --project tools/EnvSessionChecks -c Release`：owned 托管接口、并行分支、失败恢复与释放检查通过。`scripts/check.sh --quick`：10/10 CTest，三组整局冒烟分别 `death=94 unported=6`、`death=100`、`boss=3 death=97`，quick passed。完整 L2/L3/L4 `scripts/check.sh` 没有运行。

## 具体差异与限制

baseline 的 Whirlwind 游戏 getter `effective energy=0`，原搜索 X 费用 `2`；Unplayable Defend 的游戏 getter `effective energy=1`，原搜索早拒绝并返回 `0`；无星费游戏 getter 为 `-1`，原搜索返回 `0`。三组测试还覆盖资源归零和 Sloth 阻止出牌。原政策有专门的 Hook 与卡牌方法 mirror、分支 StateStore、动态历史依赖；这个探针现在用已移植游戏 getter/Hook 计算对应值。三组相等不证明所有镜像源相等，后续必须为每个可支持根证明或拒绝，尤其模组监听者、Clash/Grand Finale/High Five、Enthralled、Smog、选择后续和动态生成卡。

P2 事件历史与风险、威胁查询、P3 全评分/状态键投影和 P4 原算法执行仍未完成。正式 Env 执行门禁保持 `live_root_and_same_policy_adapter_incomplete`；没有实机 Env 搜索或最终 mods 包验收。
