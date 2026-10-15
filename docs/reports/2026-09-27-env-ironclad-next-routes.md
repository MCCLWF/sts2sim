# 通用 Env：下一批铁甲战士路线执行报告

日期：2026-09-27。分支：`feat/generic-env-rf-api`。计划：[下一批铁甲战士三幕路线](../plans/2026-09-27-env-ironclad-next-routes.md)。真值：游戏 v0.111.0 源码与 `Sts2Oracle run`；CombatSolver 保持冻结。

## 首次巡检

`DOTNET=/home/wst/.dotnet/dotnet python3 tests/l4/l4_run.py --seeds SEED0005,SEED0006,SEED0007,SEED0008 --policy-seeds 1 --no-neow --hp 10000 --stop victory --verify-snapshots --verbose`：4 路线仅 1 路线完整，1864/3620 决策对齐；`SEED0005` 在决策 957 遇 `StaticVec<CardRef, 512>` 容量错误，`SEED0007` 在决策 290 停 Pumpkin Candle `AfterObtained`，`SEED0008` 在决策 30 因 Regal Pillow 保守可用性门禁停住。`SEED0006` 587/587 完整到胜利。

## 修复与原版依据

- `SEED0005` 的正版轨迹在决策 957 有 511 张弃牌，结束回合继续增长；这场战斗最多同时有 605 张牌，多数是 Anger 的复制。原版单堆没有 512 张限制。C++ 牌堆容量从 512 调到卡牌池上限 1024，重新生成 POD 字段遍历；没有改牌序或 RNG。使用保存的正版轨迹重放并逐点恢复：1073/1073 决策对齐。
- Pumpkin Candle 本批新增头文件，移植 +5 点火、战后 -1 与有点火时 +1 最大能量，并生成实际 hook 分发；按 `KindleRestSiteOption.cs` 在休息点追加 KINDLE、选择后给当前玩家的遗物 +5。固定 `SEED0002` 注入此遗物，原版确实在第一幕第 16 层选择 KINDLE；严格 L4 273/273 决策对齐并逐点恢复。
- Regal Pillow 的原版回复加成作用于主人或其宠物，回复后 Flash 再重置状态；头文件已对齐。`ModifyExtraRestSiteHealText` 唯一调用在 `HealRestSiteOption.Description`，只添加本地展示文字，不影响 Env 的决策或状态；生成器据此将其排除出 `IsUsable` 的逻辑门禁，其余模拟 hook 仍照常分发。`SEED0008` 严格第一幕 247/247 决策对齐并逐点恢复。

## 当前结果与限制

四路线重新巡检：`SEED0005` 1073/1073 与 `SEED0006` 587/587 完整一致；`SEED0007` 进入第二幕后在 `Events.Symbiote.GenerateInitialOptions` 明确停止，332/1371 已对齐；`SEED0008` 进入第二幕后在 `Events.Darv.GenerateInitialOptions` 明确停止，247/589 已对齐。合计 2239/3620 决策，2 完整、2 Unported、0 分歧。Symbiote 涉及 Corrupted 附魔与变牌，Darv 涉及先古遗物候选、洗牌和奖励；两者需要独立源码移植与对拍，不能把已通过的前缀算作完整三幕。

`scripts/check.sh` 完整通过：16/16 CTest、L2 200 局零差异、L3 80 战/1611 状态零差异、默认 L4 80 局/4089 决策零差异；定向二幕 3246、旧胜利 1138、此前四路线 3520、本批两路线 1660、Sand Castle 172、Wongo ticket 256、Pumpkin Candle KINDLE 273、Regal Pillow 247、Pael's Wing 219 决策全部逐点一致。严格路线开启快照逐决策恢复。`python3 tools/gen/gen_state_fields.py --check` 与生成分发一致性由构建门禁通过。

本批仍是高生命值压力路线，不等于正常血量或全部内容覆盖。对象池、全部 Hook 与性能上限仍须按 E0/E5 验收；完整 RF 操作/结果合同、真实捕获与 SDK 仍缺。
