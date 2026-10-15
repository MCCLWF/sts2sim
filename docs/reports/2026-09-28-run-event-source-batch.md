# Run 级事件源码批次

日期：2026-09-28。分支：`feat/generic-env-rf-api`。真值基线：游戏 v0.111.0，release commit `41cef1ea`。

## 前置状态

通用 Env 的事件房间、选择重放、奖励和事件战斗续接已有实现。`scripts/check.sh` 在本批修改前通过 16/16 CTest、L2 200/200、L3 80/80、默认 L4 80/80 以及现有定向严格路线。地图清单显示 The Lantern Key、Trial、Tanx 尚无具体头文件，若干已有事件留有行为缺口。CombatSolver 仍冻结。

## 源码移植

逐一核对 `src/Core/Models/Events` 的完整事件源码后：

- 完成 Amalgamator、Field of Man-Sized Holes、Relic Trader、Stone of All Time、Zen Weaver 原有占位实现的选项、门槛、选择和状态效果。
- 接入 Doll Room 的随机娃娃、付血后 2/3 选项、遗物获取；后续选项的 `TextKey` 使用 v0.111.0 英文原始遗物标题，与该语言的原版构造函数一致。
- 新增 Tanx 的九件基础先古遗物与 Tri-Boomerang 门槛，再按原版不稳定洗牌取三件。
- 新增 The Lantern Key 的返还金币、保留钥匙和战斗分支；特殊 Lantern Key 卡牌奖励随 Mysterious Knight 战斗传递。
- `EventCombatSynchronizer` 的布局预建从两个事件特判改为按 `CanonicalEncounter` 通用处理。现有 Punch Off/The Architect 继续走相同的预建顺序。
- 重新生成模型分发与地图源码清单。事件状态只使用现有 POD 字段。
- 地图源码清单现在能识别共享 C++ 头文件里的具体模型定义，例如 `MysteriousKnight` 位于 `FlailKnight.h`，并避免把单独的前置声明算作实现。

`FieldOfManSizedHoles` 的 `CardsVar(2)` 与 Amalgamator 等事件的若干展示变量未由当前内容导出器列入 `EventData.vars`；运行效果中的选择数量直接采用原版源码常量 2。文本展示变量不参与行动状态。

## 实测

```text
python3 tools/gen/gen_dispatch.py                         PASS
cmake --build build -j4                                   PASS
ctest --test-dir build -R run_event_batch --output-on-failure
  1/1 passed
scripts/check.sh
  17/17 CTest passed
  L2 newrun: 200 runs, 0 mismatched
  L3 combat: 40/40 + 40/40 combats identical, 0 diverged
  L4 default: 40/40 + 40/40 identical, 0 diverged
  strict Act 2: 1311/1311 + 737/737 + 1198/1198 decisions matched
  strict final victory: 1138/1138 + 3520/3520 + 1660/1660 + 1371/1371 decisions matched
  Pael's Wing: 219/219 decisions matched
  all checks passed
git diff --check                                        PASS
python3 tests/test_map_source_inventory.py               2/2 passed
python3 tools/coverage/map_source_inventory.py --check docs/audits/map-source-inventory.json
                                                        PASS
```

新增 `run_event_batch` 用源码明确的选项、金币/HP、牌堆、遗物、RNG 抽取次数和 Lantern Key 特殊奖励检查本批逻辑。这些是定向 C++ 测试；既有整局 L4 语料没有逐项经过全部新增分支，因此本批尚无各事件的直接整局 oracle 对拍结论。

## 剩余边界

- Trial 仍缺具体事件模型；Fake Merchant、War Historian、Orobas 等已有模型仍有行为标记或特殊 UI/原版规则待核。
- Doll Room 的 raw `TextKey` 目前按英文原版定位；跨语言标题与完整标题/描述呈现尚无公共 API 合同。
- 事件给出的 Perfect Fit、Vigorous 及 Ultimate Strike/Defend 等内容的战斗效果须按各自卡牌/附魔验收；事件选项测试不证明这些效果已全量正确。
- The Lantern Key 的通用事件战斗入口经过定向 C++ 测试，但 Mysterious Knight 完整战斗与返还分支仍需原版事件路径对拍。
- 全量内容清单尚未完成，故尚未开始每轮新抽取并保存的 100+ seed 多角色整局差分；固定种子仅保留为现有回归语料。
