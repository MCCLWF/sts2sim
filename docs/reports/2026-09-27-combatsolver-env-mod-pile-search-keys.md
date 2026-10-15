# Mod 牌堆保留键与循环牌堆形状

日期：2026-09-27。分支 `feat/combatsolver-env-backend`；固定原 CombatSolver 为 `d231e9e51a0e58d6bfa1373c6265cce13ffd9a45`，本次未修改子模块。

## 实现边界

`EnvSearchPileFingerprintProjection` 读取 owned Env 通用 getter v4 的前四个战斗牌堆及已在 Mod 复刻的单卡指纹，按原 `StateFingerprintBuilder` 编码：

- `UnorderedPileKey`：逐堆对完整单卡指纹的两个半字分别 mix、求和，再附加 H/D/C/X、张数和两个和；用于原 Beam 的牌堆保留。
- `CyclePileShapeKey`：只用卡牌 ID 与升级级别的结构指纹构成原循环牌堆形状部分，刻意忽略可变增长字段。
- `AppendStateKeyPiles`：保留原状态键四牌堆在政策 `PileOrderInvariantMask` 下的有序/无序编码；本次实际对拍覆盖默认 mask=0 的有序片段，无序分支的计算输入由独立无序键对拍覆盖。

初始实现误把 `BuildCyclePileShapeKey` 的结果同完整 `SimulationSnapshot.CycleShapeKey` 比较。原 `BuildCycleShapeKey` 还附加存活敌人位图、药水库存键和搜索边界；本次修正名称和测试目标，直接反射调用固定原 Beam 的 `BuildCyclePileShapeKey`，不改原源码。完整键所需 `aliveEnemyMask` 按原 `KnownEnemies` 顺序以及 `EffectiveEnemyHp > 0 && ContainsCreature` 计算，当前 Env 活动 roster 的原始生命不能代替；遇到复活等情况尤须保留明确拒绝。药水槽可从 owned snapshot 读取，但还需固定原根槽位数和政策边界。

## 验证

- 派生 Mod Release 构建 0 warning、0 error：`/tmp/mcc-pile-key-mod-build.log`。
- TestMode=false 的 16 个原 Beam 场景拥有 Env 后继；无序保留键、循环牌堆形状部分和默认有序 `AppendPile` 四堆片段均逐位相同：`/tmp/mcc-pile-key-beam.log`。反射只用于测试夹具，运行时 Mod 不通过反射读取原预测状态。
- Getter 回归三组 TestMode=false 完整 live 根与一组 Play 牌堆边界通过：`/tmp/mcc-pile-key-queries.log`；预测 API 的 64 并行与 16 专用分支、协议、源隔离及 Dispose 通过：`/tmp/mcc-pile-key-api.log`。
- 完整 `scripts/check.sh` 通过：`/tmp/mcc-pile-key-full.log`。CTest 10/10；L2 200 局零差异；L3 A0/A10 各 40/40 场、784/784 与 827/827 状态；L4 两组各 40/40 局、1958/1958 与 2131/2131 决策。Oracle 构建保留既有一项 CS8604 warning、零 error。

这仍是部分键投影。`OriginalStateKeyReady=false`、Env 正式执行门禁未开启；游戏内两种模式的同算法等预算性能结论和最终 `mods/` 成品仍待完成。
