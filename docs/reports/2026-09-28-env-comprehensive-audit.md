# 通用 Env 与 CombatSolver 当前全面勘察

日期：2026-09-28。分支：`feat/generic-env-rf-api`。真值版本：STS2 v0.111.0，游戏 commit `41cef1ea`。

## 结论

继续开发通用 C++ Env 有意义，而且应当先完成它。CombatSolver 的核心移植暂时冻结，不再把搜索评分、Beam 保留策略、专用状态键或 Mod 适配细节倒灌进 Env。

当前 Env 已经是可验证的原版规则执行底座，但还不是可放入 `mods` 后完全替代原生搜索的成品。正确的完成顺序是：先把 Env 的原版单人规则、地图/房间/选择/战斗恢复和 Random Foreseer 需要的通用操作与结果 API 做完整；之后再做一个只调用公共 Env API 的客户端或 Mod 适配层。这样 RF 迁移不需要修改 Env 内核。

本次后续批次已接入 Darv 与 Dusty Tome，并以五条 `SEED0008` 二幕路线核对；Crystal Sphere 的两组原版小游戏黄金样本验证了 15 件落位、点击、Debt/Doubt、三档卡牌、药水、遗物、金币和事件 RNG。它们缩小了已知内容缺口，仍未改变下文的总体结论。见[Darv 报告](2026-09-28-darv.md)与[Crystal Sphere 报告](2026-09-28-crystal-sphere.md)。

## 本次实测门禁

`scripts/check.sh` 已在本分支完整通过：

| 层级 | 实测结果 |
|---|---:|
| CTest | 16/16 |
| L2 新局和地图 | 200/200，0 分歧 |
| L3 首层战斗 | 80/80 战斗，1611/1611 状态 |
| 默认 L4 第一幕 | 80/80 局，4089/4089 决策 |
| 严格二幕续接 | 3246/3246 决策 |
| 已有三幕胜利路线 | 1138/1138 决策 |
| 追加四条三幕路线 | 3520/3520 决策 |
| `SEED0005/0006` | 1660/1660 决策，2/2 到胜利 |
| Pumpkin Candle / Kindle | 273/273 决策 |
| Regal Pillow | 247/247 决策 |
| Pael's Wing 两次献祭 | 219/219 决策 |

这组数据证明的是已选路线的逐决策恢复和 oracle 对拍，不是全内容完成证明。默认 L4 仍允许遇到 `Unported`，严格路线只对指定语料开启完整门禁。

## 通用 Env 已有能力

- C++ `Env` 提供 `Reset/Step/LegalActions/Clone/Hash/Predict/Observe/Determinize/VectorEnv`。
- 快照 v3 已包含游戏状态、隐藏 RNG、对象引用和选择前缀，可验证、导出、导入并逐点恢复；跨幕、事件战斗返回父事件和若干嵌套选择已有实测夹具。
- C ABI 当前有 27 个普通 Env、快照、预览、查询和结果接口；旧 live mapper 的两个专用出口已删除。构建目标只依赖 `sts2env`，没有链接旧 CombatSolver 或旧 prediction ABI。
- 局外/只读查询已有 run/map、选择、奖励、商店、事件、before/after 投影，以及战斗的 calculated vars、powers、orbs 和公式输入。
- 预览失败能区分 `Unported`、trace limit、非法动作和一般错误；源状态保持不变。结果 trace 已能记录一部分生成、抽牌、出牌、伤害、能力和球事件。
- Python ctypes 和参考 ISMCTS 是公共 Env 的消费者示例，不是 CombatSolver 的后端。

## 通用 Env 仍缺的内容

1. 地图内容闭环：源码清单覆盖 283 个 Act/Event/Ancient/Encounter/Monster 模型，但目前仍有事件行为缺口，Hive 的 The Lantern Key、Glory 的 Trial 和 Tanx 还没有具体 C++ 文件。Darv 已补，Crystal Sphere 有定向原版证据；其它路径不能由这两项推定完成。
2. 规则完整性：大量卡牌、能力、药水、遗物、先古、修饰器、事件条件和真实档案/解锁规则还没有逐方法 oracle 证据。怪物文件中的表现层 `SIM_NOT_PORTED` 也需要逐项确认，不能用文件存在替代行为完成。
3. RF 通用操作：变牌、获取遗物、生成/重掷奖励、商店补货、休息效果、献祭等需要统一的类型化操作上下文、选择边界和失败合同。
4. 结构化结果：父子因果、来源实例、完整资源/伤害/生成差分、稳定事件顺序和全部可解释的执行停止原因仍不完整；当前 `trace_complete=false` 的边界必须保留。
5. 真实档案、live 捕获、跨进程协议和独立 C#/Python SDK 尚未完成。
6. 规模和性能：尚未通过 1 万条完整单人轨迹零分歧/零 `Unported`，也尚未完成 Clone、Step、ABI、trace 的可复现成本验收。

## CombatSolver 移植现状

CombatSolver 的仓库子模块仍保留作历史参考，但不在当前 CMake 构建、Env 公共 ABI 或 `scripts/check.sh` 路径中。旧 managed Env、旧 `combat_prediction` ABI 和 Mod 适配代码已移到 `archive/combatsolver/`。

历史工作已经验证过若干局部组件：真实运行上下文、卡牌/能力/遗物/药水/怪物 RNG 与部分状态导入、owned clone、有限预测和原 Beam 历史投影夹具。但这些检查明确写着 `full_root=false`，当前正式门禁仍拒绝：

- `live_root_and_same_policy_adapter_incomplete`
- `OriginalStateKeyReady=false`
- `history_projection_complete=false`
- 原 Beam 的完整政策、评分、威胁投影和全部风险条目未闭合

因此“旧 CombatSolver 已经改成 Env”是不成立的；准确说法是“已有若干可复用的局部捕获/投影实验，尚未形成可替代原搜索的根状态和同策略执行后端”。继续移植这些特例会把搜索政策重新耦合进通用 Env，不能解决当前最大的内容和合同缺口。

## 决策与执行顺序

当前阶段保持以下边界：

1. CombatSolver 核心冻结，旧 Beam、专用 prediction ABI 和历史 state key 不再扩展。
2. 按源码顺序完成地图/房间/事件/先古/遭遇/怪物的行为清单；Symbiote、Corrupted 与 Darv 已有定向验证，Crystal Sphere 有原版小游戏黄金样本。下一首停是 ChoicesParadox，再补地图清单中的缺失文件和条件分支。
3. 铁甲战士完整单人 run 与 RF 通用 API 按垂直功能同步补齐。先验证抽牌/生成、变牌、遗物获取、商店补货四类代表操作；每类都做 oracle、选择恢复、结果和失败合同。API 只表达原版操作与事实，客户策略留在 Env 外。
4. 内容和公共合同接近完整后，才开发真实 live 捕获桥、独立 SDK 和只依赖 Env 的 RF 客户端/Mod。
5. 最后用多角色、多幕、真实档案和 100+ 长局收敛差异，再进行 mods 文件夹成品、UI 双模式和原生搜索替代验收。

这条顺序满足“移植时不改 Env”：Env 只提供原版事实与通用执行能力，RF/Mod 负责搜索、评分、展示和模式切换。
