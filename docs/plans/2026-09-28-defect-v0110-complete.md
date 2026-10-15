# Defect v0.111.0 全量移植

分支：`feat/defect-complete-v0110`。基线：`add6186`。真值：`../Slay the Spire 2/src/Core` v0.111.0 与本仓库 `tools/Sts2Oracle`。

## 目标

为通用 C++ Env 完整移植故障机器人专属内容：原版 `DEFECT_CARD_POOL` 91 张卡及其生成链、`DEFECT_RELIC_POOL` 8 件遗物、`DEFECT_POTION_POOL` 3 瓶药水、这些模型依赖的 Power、5 种充能球，以及球的生成、被动/激发、Hook、槽位增减和队列次序。全解锁档案下的纪元筛选源分支也列入清单。`IGNITION` 与 `IMITATION_LEARNING` 在原版标为 `MultiplayerOnly`，单人 Oracle 无法出牌；仍保留模型/override/分支审计并以直接 C++ 模型用例验证。CombatSolver 不在范围内。

## 先行源码清单

- 逐项清单由 `tools/coverage/defect_source_inventory.py` 从 v0.111.0 C# 源码、`data/content.json` 的原版 override 元数据及 C++ 注册表生成，输出 `docs/audits/defect-v0110-source-inventory.csv`。
- 清单含 pool/生成关系中的模型、每个原版 override、每条 `if/else`/`switch case`/三元条件分支及会改变顺序的循环；保留源码路径与行号、C++ 文件和当前实现状态。
- 源清单展开为 146 个实体模型、10 个角色/池/纪元源模型、715 项 override、274 个条件分支、52 个迭代点、128 条生成/随机候选边及 29 个通用引擎源方法。与 v0.111.0 原版池列表和导出内容逐项核对：91/91 卡、8/8 遗物、3/3 药水；146 个实体模型全部有 C++ 类型、注册和派发。
- 生成入口必须覆盖原版真实候选集：Discovery 的整张 Defect 牌池三选一、White Noise 与 Creative AI 的 Defect 能力牌生成、随机充能球及所有显式生成牌；不能只测某组种子实际抽到的停止点。
- 每批按模型族实现并更新清单；共享 Cmd/Hook/状态布局的必要改动先记录影响面并协调，再落代码。不得触碰 `external/CombatSolver/`。

## 批次

1. 全部 Defect pool 卡、显式/随机生成卡与其完整升级、生成、选择、回合 Hook 分支。
2. 8 件 Defect 遗物、3 瓶 Defect 药水及其触发顺序、生成与消耗分支。
3. 所有 Defect 卡/遗物/药水/球实际使用的 Power；包含叠加、计数、持续时间、回合边界、目标/来源和生成牌选择。
4. Lightning、Frost、Dark、Plasma、Glass 球和完整槽位/队列：加入、满槽自动激发、指定/队首/队尾激发、被动与回合触发、槽数增减、移除、死亡/战斗结束抑制及相关 RNG。
5. 对清单项使用 Sts2Oracle 差分覆盖基础/最高升级配置、可达触发和选择分支；另以状态牌生成、Discovery、五种随机球及快照用例检查 RNG、候选选牌和恢复。

## 验收

- 清单所有模型、override、生成关系、条件分支和循环均有实现或明确的全解锁/单人/纯表现/源码未调用分类；清单不留 `missing`、`unverified`。卡池 Oracle 矩阵将两张 `MultiplayerOnly` 卡单独标记，且 `tests/test_potions.cpp` 直接执行 Ignition 与 Imitation Learning 的模型行为。
- Oracle 原始输出/日志和实际运行命令记入执行报告；范围包括机制、RNG、需选择动作、分支中断后的快照恢复。
- `scripts/check.sh` 通过并保留原始摘要；若失败，报告逐项列出实际失败结果。
- 更新 README 与路线图。仅在此 worktree 提交，不合并。
