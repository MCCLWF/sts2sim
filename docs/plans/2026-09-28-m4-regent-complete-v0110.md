# 储君 v0.111.0 全量内容移植

## 目标

依据反编译版 v0.111.0 与 `Sts2Oracle`，把 `RegentCardPool` 的 91 张卡及生成/转换牌、`RegentRelicPool` 的 8 件遗物、`RegentPotionPool` 的全部药水、相关专属 Power、角色开局与星星及跨回合机制移植到通用 C++ Env。覆盖范围由原版模型和控制流清单决定，不以任何固定路线、首停点或已打出的内容缩小范围。

## 范围边界

- 只在独立 worktree `feat/regent-complete-v0110` 实现、验证并提交；不自行 merge。
- 不修改 `external/CombatSolver/` 中任何文件，也不扩展 CombatSolver。
- 触及共享引擎前先核对通用 Env 现有合同、相关工作状态和既有改动；先在本计划/清单定位具体缺口，再实施并以 Oracle 验证。
- 原版 v0.111.0 源码与 `Sts2Oracle` 是真值；第二移植仅能辅助阅读。

## 批次

1. **源清单**：逐项记录 Regent 角色、三个池、七个解锁纪元、91 张池内卡、所有显式生成/转换卡、8 件遗物、3 种药水和其传递依赖 Power；逐 override、逐显式分支记录实现与验证状态。
2. **角色与资源**：开局和池解锁条件、星星获取/支付/费用修饰/X 费用、生成资源、跨回合排队与快照恢复。
3. **卡牌批次**：按相互依赖和公共机制分组实现所有 91 张池内卡及生成/转换牌；每个模型覆盖所有 override、升级行为、选择、生成顺序及分支。
4. **Power 批次**：实现并核对所有 Regent 专属 Power 和被触发的通用 Power 行为；核对实例内部数据、监听者顺序、触发条件与跨回合状态。
5. **遗物与药水批次**：完整实现职业遗物与解锁药水，包含获得/使用/战斗结束等 Hook、目标选择和 RNG。
6. **差分与收尾**：针对清单中的模型/分支用 Oracle 核对初始值、动作、数值、RNG、选择和前缀快照恢复；清单不留未处理项；运行 `scripts/check.sh`，更新路线图、报告并提交。

## 验收标准

- `docs/audits/regent-v0110-port-checklist.md` 与原版源树逐项一致，91/91 池内卡、所有生成/转换依赖、8/8 遗物、3/3 药水、角色、Power 和 epoch 条件的每个 override/条件分支均标记为已实现且有验证依据；没有以路线首停替代内容覆盖。
- Oracle 对拍覆盖所有模型效果，包含数值、RNG 次数与顺序、选择合法项/执行顺序、升级和条件路径；对跨回合行为与选择重放进行快照前后恢复对拍。
- 运行 `scripts/check.sh` 并保存原始 stdout/stderr 与退出码；如有失败，明确列为剩余问题，不把工作标成完成。
- 提交计划、源清单、原始验证结果和剩余问题报告；更新路线图/README 当前状态；改动不触及 CombatSolver；仅创建本分支提交。

## 文件范围

- `game/Models/{Cards,Powers,Relics,Potions,Characters,CardPools,RelicPools,PotionPools}` 与所需通用 Env 共享实现。
- `game/generated/` 仅由 `tools/gen/gen_dispatch.py` 生成。
- `tools/Sts2Oracle/`、Oracle 差分/清单工具及现有测试语料中与 Regent 直接相关的接口。
- `docs/audits/regent-v0110-port-checklist.md`、本计划、执行报告、路线图与 README。

## 前置状态（开始工作时）

- 以 `feat/generic-env-rf-api` 的提交 `add61860` 为基线建立独立 worktree。
- 原始 checkout 有未提交的事件文件改动；这些改动没有复制进本 worktree，也不会纳入本批。
- 角色资源基础设施已存在部分实现（星星计数、费用支付、若干星星 hook）；必须以源清单与 Oracle 复核，不能据此视为完整。

## 执行结果

- 全部源清单条目实现并对拍；卡池与派生卡 3 档升级矩阵、遗物/药水模型矩阵、星星历史、Monologue 跨回合及胜利整局快照恢复均通过。原始结果和 SHA-256 索引见 `docs/reports/regent-v0110/regent-v0110-closeout.md`。
- `scripts/check.sh` 退出码 0；CTest 18/18，L2/L3/L4 全通过。
- 仅留出通用 Env 中与 Regent 模型无关的几个遗物缺口及 headless Oracle CrystalSphere 异常，已在 closeout 报告列明。
- 实现及文档已提交到 `feat/regent-complete-v0110` 独立 worktree；未 merge，`external/CombatSolver/` 无改动。
