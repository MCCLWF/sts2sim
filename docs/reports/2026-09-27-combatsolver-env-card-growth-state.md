# CombatSolver Env：精确卡牌增长与已审计风险历史

日期：2026-09-27。分支：`feat/combatsolver-env-backend`。计划：[M5 卡牌增长状态](../plans/2026-09-27-m5-card-growth-state.md)。前置提交 `654208a`；固定 upstream 为 `d231e9e51a0e58d6bfa1373c6265cce13ffd9a45`，子模块无修改。游戏行为真值为正版 v0.111.0 源码/oracle，搜索政策真值为固定 upstream。

## 结果与交付状态

Rampage/Thrash 内部 decimal 增长、小数出牌与降级恢复已验证；Rampage 的原特殊卡牌指纹已对齐。原 Beam 新场景发现的 Risk 缺失已补有限映射；11 场检查不再排除风险条目。三组 TestMode=false live 整根的导入、实际出牌和连续两次 EndTurn 全字段一致，包含 Weak 实际到期移除后的已观察对象。

这仍不是最终可替代原搜索的 Mod：完整 state key、全部事件/风险、只读政策与评分、原算法 Env 执行和游戏内同根等预算性能验收尚未完成。`history_projection_complete=false`、`OriginalStateKeyReady=false` 和 `live_root_and_same_policy_adapter_incomplete` 门禁保持关闭。参考 Beam 的耗时不作为原算法性能收益。本次未更新工作区外的旧开发 Mod 包，也未将它作为最终交付。

## 实现与证据

1. 正版 `Models/Cards/Rampage.cs` 的 `_extraDamageFromPlays` 与 `Thrash.cs` 的 `_extraDamage` 都是 decimal。此前 C++ 用整数 `extra(0)` 累计，Thrash 会截断小数；两张牌降级后按内部累计值恢复 Damage。新增 `CardState.growth_damage: Dec`，将其纳入命名字段、hash/schema，增加 CardState POD 断言。普通 run snapshot 保持协议 v2，旧 schema 明确拒绝。
2. Live mapper 仅接受这两个模型已审计的准确子类字段集合，捕获正版对象的当前值及全部 decimal 位；其余内部字段继续拒绝。查询升级为 v3，提供 Rampage 的 growth operand；原 `StateFingerprintBuilder` 在 unordered DynamicVars 后追加 Rampage 的 decimal 四字。Thrash 原指纹没有追加内部 counter，保持原编码。
3. 三组查询根各有 9 张可核对卡牌，包含 upgraded Rampage `-1.2500m` 和 Thrash `0.1250m` 增长。原卡牌指纹逐牌一致。直接执行正版 `DowngradeInternal` 与 C++ downgrade 后，两个槽的全部卡牌字段一致，覆盖符号/scale 与升级后变量。
4. 全解锁 Ironclad/Cultists A0 的三组整根使用生产命令。Rampage 出牌增长为 `2.500m`；Thrash 的手牌普通攻击 Damage 为 `1.2500m` 且玩家 Weak=1，使伤害 Hook 产生非整数增长。连续两次真实 EndTurn 包含能力增长、伤害、Weak 移除与重洗，递归完整 state 文档比较，无忽略字段。
5. Weak 移除暴露夹具原引用跟踪只覆盖应用。观察器现在订阅原 `PowerApplied/PowerRemoved`，对已移除且仍持有的正版对象重新读取当前字段，不复用旧 capture、不读取 Env 后继。只支持固定 roster 和已观察应用/移除事件；未应用分配、未观察移除或重新应用同一实例仍拒绝。这不是生产 mapper 的任意历史闭包证明。
6. Thrash 选择 `CalculatedDamage` 时，游戏源码调用 `Calculate(null)`；原 C++ 错用 BaseValue。本次明确返回 Unported，并验证失败无后继、源/兄弟不变。普通 Damage/OstyDamage 路径保留；计算型公式尚待补齐，未宣称 Thrash 任意目标支持。
7. 原 `MethodMirrorRegistry.Invoke` 在 inferred/unsupported 分派**之前**记录风险。Rampage 的模型写使 strict inference 拒绝，普通攻击使用 inferred，风险为 `MethodMirrorIncomplete`；VoidForm 无可推断攻击/格挡/抽牌模板，风险为 `MethodNotMirrored`，剩余 card spec 随后执行。新增独立预测元数据映射，只在 scope 内于 OnPlay 前记录，不执行原预测器、不改变普通 Env/gameplay。仅覆盖固定默认 inference 下这两个 OnPlay 分派；其他分派/Hook/限制/非默认设置均未完成。
8. 外层预测 envelope 升为 v5，事件由 20 项变 21 项，新增 optional 风险 reason ordinal。严格验证事件 kind、原因范围、纯风险载荷和非风险条目不得携带原因；hash、Clone、恢复均包含它。v4 和旧事件形状拒绝。原 Beam 比较器逐项核对所有条目顺序、已支持 payload、六计数和风险原因，没有 Risk 排除规则。

## 验收命令与原始结果

日志/JSON 保存在临时目录，不提交大体积原始产物。

| 命令 | 原始结果与范围 |
|---|---|
| `scripts/check.sh --quick` | `/tmp/mcc-card-growth-risk-quick.log`：CTest `100% tests passed, 0 tests failed out of 10`；整局 smoke 成功 |
| `scripts/check.sh` | `/tmp/mcc-card-growth-risk-full.log`：`all checks passed`；L2 `200 runs, 0 mismatched`；L3 两组各 `40/40 combats identical`，`784/784` 和 `827/827` states；L4 两组各 40 条完整路线，`1958/1958` 和 `2131/2131` decisions，无 Unported/分歧 |
| `~/.dotnet/dotnet build tools/CombatSolverEnv/CombatSolverEnv.csproj -c Release` | `/tmp/mcc-card-growth-risk-build.log`：`Build succeeded`，0 Warning/0 Error；后续查询工具也重新构建派生程序集 |
| `~/.dotnet/dotnet run --project tools/EnvLiveCardChecks -c Release` | `/tmp/mcc-card-growth-risk-live.log`：strike/rampage/thrash 均 `seed-free assemble/restore, production play and two full next-turn successors including shuffle matched; TestMode=false; installed_game_acceptance=false` |
| `python3 tools/EnvQueryChecks/check.py --out /tmp/mcc-card-growth-risk-queries` | `/tmp/mcc-card-growth-risk-queries.log`：三组原指纹各 `cards=9; exact_encoder=true; complete_state_key=false`；四组 getter、三组原 CanPlayCard core 一致；`passed Rampage/Thrash downgrade: all native card fields and exact fractional growth matched` |
| `~/.dotnet/dotnet run --project tools/EnvCombatPredictionChecks -c Release -- --out /tmp/mcc-card-growth-risk-beam` | `/tmp/mcc-card-growth-risk-beam.log`：11 场全部通过。Rampage `projection_history=5; original_card_fingerprints=11; original_key_ready=False; reason=none`；VoidForm `projection_history=15`；比较包含 `history_entry_kinds_including_risk,history_risk_reasons`。仍是语义投影，非完整原搜索状态/全部 payload 验收 |
| `~/.dotnet/dotnet run --project tools/EnvSessionChecks -c Release` | `/tmp/mcc-card-growth-risk-api-final.log`：64 并行预测、16 个专用 prediction 分支、完整恢复、失败恢复、释放和严格协议通过 |
| `build/test_combat_prediction /tmp/mcc-card-growth-risk-abi-fixtures` 和 `python3 tools/CombatSolverEnv/check_prediction_abi.py /tmp/mcc-card-growth-risk-abi-fixtures` | `/tmp/mcc-card-growth-risk-abi.log`：借用输入缓冲、失败输出 null、owned clone、terminal/choice 恢复、风险分支恢复通过；C++ 合同另覆盖无效原因、改变原因的 hash 拒绝、额外载荷、旧事件形状和旧 envelope 拒绝 |
| `git diff --check`、`python3 tools/gen/gen_state_fields.py --check` | 通过；固定子模块无修改 |
| `python3 tools/CombatSolverEnv/run_smoke.py --mode both --expect-profile-root-rejection --memory-reservation-mib 2048 --out /tmp/mcc-card-growth-risk-game` | `/tmp/mcc-card-growth-risk-game.log`：`derived smoke passed`；实际 game.log 记录 companion `root_source=live_capture env_imported=false env_used=false native_search=continues`，`ENV_UNAVAILABLE_WARNING shown=true` 的 Label 赋值；原 native 搜索 `native_expanded=100 native_transitions=335`。隔离实例已清理 |

游戏冒烟只验证派生程序集加载、现有部分档案的明确拒绝、UI Label 赋值和 Native 继续，不是自然根 Env 接受/可见游戏操作/完整替换验收。headless 输出仍含已知缺视觉节点的 presentation 日志；它不代替玩家实际打开游戏的完整流程检查。旧核对 context 的 `seed_snapshot_import` 是未成功导入完整根的状态标记，此拒绝中未创建 Env 后继；companion 明确记录实际 live 捕获失败。

## 实施中发现的失败

- 第一次 Thrash full-live 下一回合拒绝 Weak 被移除。保留 Weak=1，补已观察移除对象捕获后，三组完整后继通过；没有延长 Weak 持续时间绕过路径。
- 新增 Rampage 原 Beam 场景首先报 `unaccounted_native_policy_risk:rampage:1`。审计原风险入口、增加 owned transport 和真实顺序映射后通过；比较器现在还核对既有 VoidForm 风险，未扩大排除规则。
- 首次计算型拒绝 C++ 夹具误将 `AddInternal(card, true)` 的 bool 传为整数 index=1。按真实 C++ 签名修为 `AddInternal(card, -1, true)`，保留拒绝/源隔离断言；这不是引擎行为修复。
- v5 扩展后首次 managed 合同检查以旧 20 项事件测试攻击 hit count，先被事件形状检查拒绝。补齐新增 null 风险槽后，原 `attack hit count mismatch` 判据仍保留并通过，没有放宽错误匹配。

## 下一步与未验收项

后续继续补原政策读取、其他特殊卡牌/orb/策略账本、完整历史和威胁/评分投影，再接原 concrete simulator 的正式 Env Fork/Replay/round/choice 路由。任何未来预测调用仍须由 C++ 完成；纯政策编码可复用固定原实现。最后才开启执行门禁，验收游戏内切换/取消/部署和同根等预算性能，并生成最终 `mods/CombatSolverEnv` 文件夹。
