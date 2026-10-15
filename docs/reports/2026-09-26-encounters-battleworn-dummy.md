# Battleworn Dummy 事件遭遇移植报告

日期：2026-09-26
分支：`feat/all-encounters-comparison-mod`
游戏基线：v0.111.0，真值为反编译源码和 `Sts2Oracle`。

## 结果

新增 `BATTLEWORN_DUMMY_EVENT_V1_ENCOUNTER`、`V2`、`V3` 三个正式遭遇，分别生成 75、150、300 HP 的 Battle Friend。每只假人都带有 3 层限时能力；每次其参与的敌方侧回合结束时减少 1 层，最后一层到期后逃跑。奖励关闭来自导出的遭遇内容元数据。

三个遭遇在 A0 和 A10 的 Oracle L3 对拍全部通过：18/18 场战斗、207/207 个决策状态一致，无未移植项、无分歧。

## 逐遭遇对拍

| 遭遇 | A0 战斗 / 状态 | A10 战斗 / 状态 |
|---|---:|---:|
| BATTLEWORN_DUMMY_EVENT_V1_ENCOUNTER | 4/4 · 47/47 | 2/2 · 22/22 |
| BATTLEWORN_DUMMY_EVENT_V2_ENCOUNTER | 4/4 · 47/47 | 2/2 · 22/22 |
| BATTLEWORN_DUMMY_EVENT_V3_ENCOUNTER | 4/4 · 47/47 | 2/2 · 22/22 |
| **合计** | **12/12 · 141/141** | **6/6 · 66/66** |

## 验证

模型分发生成：

```text
python3 tools/gen/gen_dispatch.py
ported models: 781; dispatch cases: MonsterModel 120, EncounterModel 69
```

目标构建通过：

```text
cmake --build build -j4 --target test_env sts2env_c sts2sim_env_solver sts2sim_cli
```

整体验证：

```text
scripts/check.sh --quick
100% tests passed, 0 tests failed out of 8
smoke: death=94 unported=6
smoke: death=100
smoke: boss=3 death=97
quick check passed (differential checks skipped)
```

## 范围

本批覆盖三种训练假人的战斗生成、等待招式和限时逃跑。Battleworn Dummy 事件的战后胜负页、奖励抽取和事件恢复逻辑尚未接入；完整 L2/L3/L4 门禁也未运行。当前测试为 Ironclad、所列种子与策略的抽样验证。
