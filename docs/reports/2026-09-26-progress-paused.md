# sts2sim 总体进度报告（2026-09-26，暂停开发）

## 当前结论

本阶段已完成 M2、M3 验收，并在 M4 按“铁甲战士 + 通用 run 内容 + 无色牌”的范围持续补全。当前批次完成 Bugslayer 事件及其事件牌后，按用户要求暂停开发。仓库保留现有实现和验证结果，后续可以从 M4 未完成清单继续。

其他角色的卡牌、能力和专属内容没有纳入本阶段；跨职业卡池获取路径仍明确保持未实现，不会静默替换成铁甲战士牌。

## 里程碑

| 里程碑 | 状态 | 说明 |
|---|---|---|
| M0 基础设施 | ✅ | RNG、Decimal、排序、哈希和内容表已通过 golden 验证。 |
| M1 开局、地图、战斗和整局骨架 | ✅ | 地图、战斗、奖励、事件、休息点、商店、宝箱和 replay 骨架已接入。 |
| M2 铁甲战士打通 | ✅ | 90 张铁甲战士卡的矩阵与完整路线已对拍；普通/高生命值路线均通过。 |
| M3 环境层 | ✅ | 结构化动作、固定观测、Clone/Hash、determinize、VectorEnv、C ABI/Python、ISMCTS、药水动作和 mask 已验收。 |
| M4 内容补全 | 🟡 进行中后暂停 | 已覆盖大量铁甲战士、通用遗物/药水、无色牌和部分第三幕事件/遭遇；仍有大量事件、遭遇、能力和共享内容缺口。 |
| M5 性能目标 | ⬜ | 尚未开始。 |

## 内容覆盖矩阵

以下“头文件覆盖”表示已有对应 C++ 模型文件，并不表示所有模型的每个 Hook 都已经完成。

| 内容池 | 总数 | 已有模型头文件 | 备注 |
|---|---:|---:|---|
| 铁甲战士卡池 | 90 | 90 | M2 范围完成。 |
| 无色卡池 | 65 | 52 | 缺 BeaconOfHope、BelieveInYou、Coordinate、GangUp、HuddleUp、Intercept、Knockdown、Lift、Mimic、Rally、Splash、TagTeam、TheBall。单人范围为 52/53；Splash 的跨职业牌池路径仍未实现。 |
| 事件卡池 | 28 | 7 | 本阶段新增 Exterminate、Squash；其余事件牌按实际路径逐步补全。 |
| 铁甲战士遗物池 | 8 | 8 | 已完成头文件与行为路径。 |
| 共享遗物池 | 115 | 115 | 头文件已覆盖；复杂商店、选择、附魔或多 Hook 行为仍可能有 partial。 |
| 铁甲战士药水池 | 3 | 3 | 已完成。 |
| 共享药水池 | 45 | 45 | 已完成共享药水模型头文件；Colorless Potion 可能触及未移植无色牌。 |
| 事件模型 | 60 | 49 | 14 个已有模型仍含 `SIM_NOT_PORTED`，11 个尚无头文件。 |
| 遭遇模型 | 98 | 39 | 已覆盖当前主要可达遭遇；第三幕和 Underdocks 仍有大量缺口。 |
| 能力模型 | 283 | 95 | 仍有大量其他角色和后续内容能力未实现。 |

生成器当前报告 **709 个 C++ 模型接入分发**；这是一项实现覆盖指标，不等同于游戏全部 1400 余个模型已完成。

## 最近完成的 run 级内容

### 批次 20–23：战士与经济路径

- Brimstone、Self-Forming Clay 及其专属能力。
- 铁甲战士遗物池 8/8：Demon Tongue、Paper Phrog、Ruined Helmet 等。
- 铁甲战士药水池 3/3；Ambergris、Glowwater 事件药水。
- Foul Potion 的战斗和普通商店路径。
- Girya/Shovel 休息点、The Courier 补货、Juzu Bracelet 未知地图过滤、Golden Compass 路线图、Maw Bank 进房金币/购买停用、Black Blood、Darkstone Periapt、Sozu、Ectoplasm、Runic Pyramid、Lantern Key 的部分 Repy 路线。

### 批次 24：蘑菇事件与 Construct Menagerie

- Big Mushroom：获得时最大生命 +20；持有者第一回合少抽 2 张牌。
- Fragrant Mushroom：获得时失去 15 生命，并按稳定洗牌顺序升级最多 2 张可升级牌。
- Hungry for Mushrooms：两项遗物选择，Fragrant Mushroom 选项带 15 点伤害预警。
- Construct Menagerie Normal：Punch Construct + 两只 Cubex Construct。

详见：[批次 24 计划](../plans/2026-09-26-run-events-encounter-24.md)、[批次 24 报告](2026-09-26-run-events-encounter-24.md)。

### 批次 25：Bugslayer 与事件奖励牌

- Bugslayer：两项选择分别将 Exterminate 或 Squash 加入牌组并结束事件。
- Exterminate：全体敌人 3 点伤害、重复 4 次，升级伤害 +1。
- Squash：单体 10 点伤害并施加 2 层易伤，升级伤害 +2、易伤 +1。
- Bad Luck：不可生成、不可升级、永恒且不可打出；手中回合结束造成 13 点无视格挡/无力量伤害。

详见：[批次 25 计划](../plans/2026-09-26-run-events-encounter-25.md)、[批次 25 报告](2026-09-26-run-events-encounter-25.md)。

## 验证结果

最后一次完整门禁为 `./scripts/check.sh`，结果如下：

- CTest：8/8 通过。
- Smoke：`death=94 unported=6`、`death=100`、`boss=3 death=97`；未出现崩溃或卡死。
- L2：200/200 new-run 对拍一致。
- L3：两组均 40/40 战斗一致，状态分别 784/784 和 827/827，一致数 0 diverged、0 unported。
- L4：两组均 40/40 整局一致，决策分别 1958/1958 和 2131/2131，一致数 0 diverged、0 stopped at unported。
- Oracle 构建：0 errors；保留既有 `OracleRun.cs(234,84) CS8604` 警告。
- Construct Menagerie 定向 L3：`M4EVENT24001,M4EVENT24002` × policy `1,2`，4/4 战斗、56/56 状态一致，0 unported、0 diverged。

## 明确未完成项

1. **跨职业卡牌路径**：无色 Splash 以及任何从其他角色卡池生成牌的事件、遗物、药水或修饰器路径，继续以 `Unported` 停止。
2. **事件**：Fake Merchant 的 Foul Potion 分支、War Historian Repy 选项，以及其余 11 个无头文件事件仍未完成；已有 14 个事件保留 partial Hook。
3. **遭遇**：当前 39/98 个遭遇有头文件，第三幕、Underdocks、精英和 Boss 仍需继续接入。
4. **能力和其他角色**：能力模型只有 95/283 有头文件；静默猎手、故障机器人、储君、亡灵契约师及其专属内容不在当前范围。
5. **事件卡和共享内容**：事件卡池仍有 21 张无头文件；无色牌还差 13 张；部分共享遗物虽有模型头文件，复杂行为仍可能未完全覆盖。
6. **M5**：性能优化、批量并行和 Clone <1µs 目标尚未开始。

## 继续开发时的建议顺序

1. 先补剩余可由铁甲战士路线稳定触达的事件与遭遇，并为每批增加直接行为测试和定向 L3/L4 记录。
2. 再补事件卡池和剩余无色牌，但继续保留跨职业生成边界。
3. 最后处理 Fake Merchant、War Historian Repy 等依赖复杂选择/商店状态的 partial 事件。
4. 其他角色内容和 M5 另开范围，不与当前暂停批次混合。

## 暂停状态

本报告生成后暂停继续开发。恢复时从本文件的“明确未完成项”和 `docs/roadmap.md` 的 M4 行继续，不需要重做 M2/M3 验收。
