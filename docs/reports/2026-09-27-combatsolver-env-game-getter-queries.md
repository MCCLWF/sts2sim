# M5 P2：战斗预测的游戏 getter 查询

日期：2026-09-27

分支：`feat/combatsolver-env-backend`

前置：P0 有限完整 live 根通过；P1 独立 combat prediction scope、终局与恢复已通过。这里仍是 P2 的一部分，原搜索后端未接入。

## 实现

- 新增 `env/combat_prediction_queries.*`，在预测会话的私有 Clone 上执行已移植的游戏 getter，输出版本 1 JSON。返回源 `prediction_hash`、玩家资源和回合、五牌堆中每个卡实例的身份/关键词/费用/可打出理由/阻止者/所有 roster 目标，以及怪物下一动作和原始意图伤害。
- 查询前检查当前 encounter、所有监听者、牌组和牌堆模型的移植范围。查询后除 `BaseStarCost` getter 自身的 lazy 初始化外，按完整 `GameState` 哈希检查无 gameplay 写入；源快照和哈希不变。未知模型按 `Unported` 返回。
- 经 `sts2_combat_prediction_export_queries` C ABI 和公开 `EnvCombatPredictionSession.CaptureGameQueries()` 暴露 owned JSON。协议显式标明 `query_semantics=game`、`policy_projection_complete=false`、`threat_projection_complete=false`。这些游戏 getter 值不能冒充原 CombatSolver 的 `CanPlay`、评分特征、威胁或状态键。
- 修正 pinned catalog 的 cards/monsters 元数据读取：两类共 596/120 条定义没有 `mock` 字段，其他类别仍检查显式 `mock`。类型和游戏程序集约束继续执行。

## 验证

| 命令 | 原始结果 |
|---|---|
| `cmake --build build -j2` | 成功，查询源加入静态库及 C ABI 共享库 |
| `ctest --test-dir build --output-on-failure -j2` | 10/10 passed |
| `scripts/check.sh --quick` | 10/10 CTest；三组整局冒烟分别 `death=94 unported=6`、`death=100`、`boss=3 death=97`；quick passed（L2/L3/L4 未跑） |
| `build/test_combat_prediction /tmp/mcc-p2-prediction-fixtures && python3 tools/CombatSolverEnv/check_prediction_abi.py /tmp/mcc-p2-prediction-fixtures` | 通过：owner、终局、choice 重放、终局与待选择分支的查询协议/哈希、source purity、诊断/null 错误 |
| `~/.dotnet/dotnet run --project tools/EnvSessionChecks -c Release` | 通过：16 并行预测分支、owned 查询释放后可读、重复查询与源快照不变 |
| `python3 tools/EnvQueryChecks/check.py --out /tmp/mcc-p2-env-queries-linked` | 通过：TestMode=false 的 baseline、empty_resources、blocked_hook 三个完整 live 根；另一个显式 native 静默 Play 牌堆移动夹具；五牌堆逐卡、费用、目标、资源、原始意图逐 JSON 字段与正版 getter 相同；Clone/重复查询/Unported 拒绝和源不变 |

后两类测试均调用生产 C ABI/托管接口。`play_pile` 并非生产 live mapper 接受的完整根；仅用于只读查询该牌堆的边界。fixture 跨进程时只在校验同 schema/model/registry 后替换运行时 ID；生产导入仍严格拒绝跨进程 ID。

同一夹具另从固定 upstream 原 `CombatRootSnapshot.ForkSimulator` 读取 `.native-policy.json`，不把它当作 Env 查询通过项。已观察到原政策与游戏 getter 的具体差异：baseline 中 Whirlwind 的游戏 effective energy 为 0，而原政策 X 费用按当前能量返回 2；带 Unplayable 的 Defend 的游戏 getter 仍给 effective energy 1，原政策提前拒绝并返回 0；无星费游戏 getter 使用 -1，原政策查询归一成 0。后续需独立实现并逐字段对拍原政策查询，不能直接转发这份游戏 getter JSON。

## 尚未完成

P2 的原搜索事件 trace、六项 history counter、威胁及政策 query 尚未实现。P3 评分/状态键投影、P4 原算法执行 seam、P5 实机 UI 切换、P6 同预算性能和可安装成品均未验收。正式 Env 执行门禁维持 `live_root_and_same_policy_adapter_incomplete`。本次没有运行合入 main 所需的完整 `scripts/check.sh`，也没有声称实机安装验收。
