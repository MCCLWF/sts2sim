# CombatSolver Env：真实卡牌组件

日期：2026-09-26；分支：`feat/combatsolver-env-backend`；计划：[M5 后端接入](../plans/2026-09-26-m5-combatsolver-env-backend.md)。

## 前置状态与本次范围

前一阶段已经具备完整 Env 文档的 owned 导出/恢复；生产分支核对仍明确使用种子根。本次开始从正版对象冻结卡牌组件，没有完成完整 live 根或同算法 Env 搜索。

`EnvLiveCards` 捕获 RunState/CombatState 注册牌、历史牌及递归可达的克隆/牌组原件，分配局部引用并保存 cards pool、deck、五个战斗牌堆。基础实例字段包含升级、owner/pile/注册/移除状态、费用修改顺序与组合到期标志、临时星数费用、本地关键词、标量动态变量的精确 decimal 位、历史出牌回合及目标/克隆/原件引用。星数费用尚未惰性初始化时，读取 canonical 值并投影为 Env 的 eager 表示；不调用会修改游戏 lazy flag 的 getter。

未映射的子类内部字段、特殊变量/变化的伤害属性、附魔/污染、Dampen 历史、未知模型/引用和容量超限明确拒绝。默认零的扩展槽只用于通过这些门禁的普通卡牌组件，不为未知模型推测状态。

变量和费用预热位于 native 的 before stamp 前；组件冻结位于 before/after 稳定性检查内。after stamp 后的 Attach 仅发布冻结数据，不读取 live。上下文持有独占 `JsonElement`，没有后台可变游戏对象。

## 修复的实际兼容问题

首次游戏检查失败，原因是 `live_cards_game_model_hash_mismatch`：已加载 Mod 的全局网络模型表与纯原版导出不同。原生继续完成 100 节点搜索，Env 明确停用，没有隐瞒失败。

Env 使用自己的固定 ID 表。修正为核对正版程序集 hash `1579942752` 及每张活跃卡牌的原版程序集、完整类型名和 entry，再投影到固定 Env ID。生成器核对 content/生成 CardId 的非 mock 集合和变量升级基线；Env 导入仍核对自己的 model/schema/runtime。不能用此规则放行未知玩法 Hook/补丁；这项审计仍属于完整根合同的后续工作。

初次失败材料：`/tmp/mcc-env-livecards-game-20260926`。

## 验证

### 正版对象与 C++ 卡牌组件

```bash
~/.dotnet/dotnet run --project tools/EnvLiveCardChecks/EnvLiveCardChecks.csproj -c Release
```

工具按原版模型数据库初始化，不启用 TestMode。创建两个不同状态的 Strike、牌组原件和可达的已移除原件；检查费用的 combined expiry、本地费用、升级、动态变量、引用和历史回合。冻结后修改真实变量/费用及牌堆，验证 frozen JSON 不变。将实际卡牌组件放进**其余状态明确来自种子的测试夹具**，经 C++ 完整 import/export 核对，再通过 Predict 检查实际手牌、资源消耗、弃牌及费用到期。检查附件、Rampage 内部状态和变化的 DamageVar 属性明确拒绝。

结果：

```text
live card component: native instances, exact values, identity, immutable ownership, C++ restore/predict and explicit rejection passed; full_root=false
```

日志：`/tmp/mcc-livecards-check.log`。工具中的混合夹具不能作为完整 live 根对拍证据。

### 派生 Mod 构建

```bash
~/.dotnet/dotnet build tools/CombatSolverEnv/CombatSolverEnv.csproj -c Release --no-restore
```

结果：Build succeeded，0 Warning(s)，0 Error(s)。日志：`/tmp/mcc-livecards-mod-build.log`。

### 隔离正版游戏

```bash
python3 tools/CombatSolverEnv/run_smoke.py \
  --out /tmp/mcc-env-livecards-game-fixed-20260926 \
  --mode both --verify-branches --memory-reservation-mib 2048
```

结果：Passed，启动实例已清理；采样峰值 RSS `1601304 KiB`，低于声明的 `2048 MiB`。关键日志：

```text
LIVE_CARD_COMPONENT cards=20 source=live immutable=true full_root=false
BRANCH_ENV_SUMMARY native_expanded=100 native_transitions=335 env_checked_transitions_root_cumulative=343 env_disabled=False reason=none scope=structural_and_combat_rng root_source=seed_snapshot_import native_timing_includes_verification=true
```

原生稳定性检查未报告捕获期间变化；原有进入第二回合的核对继续通过。生产分支仍以 `seed_snapshot_import` 为根，没有使用不完整 live 组件替换根。该结果证明捕获入口可运行，不能声称完整后端、同预算性能或可发布成品。

本次没有修改 C++ 战斗行为，也没有合入 main；未重复前一阶段已经成功的完整 L2/L3/L4 门禁。

## 剩余条件

- 完整人物/怪物/能力/遗物/药水/RNG/FSM/历史/跑局字段映射，卡牌特殊内部状态与附件，以及准备阶段/选择/终局边界。
- 局部 pool ID 的重分配、历史分配与独立注册表顺序仍须完整根审计，不能由卡牌组件断言全部保真。
- 生产根完全从 live 组件组装和逐动作核对；不得保留种子模板中的未知字段后称为完整导入。
- Env 接管原 CombatSolver 所有搜索转移、克隆、跨回合及辅助威胁投影，保留同一算法政策。
- 持久设置 UI、安全后端切换、同根同预算性能视图与可放进 mods 的完整交付验证。

目标保持进行中。
