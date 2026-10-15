# TinkerTime / Mad Science 依赖批次

日期：2026-09-27。固定原版源码：`Models/Events/TinkerTime.cs`、`Models/Cards/MadScience.cs`。来源轨迹：`SEED0004` policy 1 在接入 Fur Coat 后的第三幕轨迹；TinkerTime 接入前 Env 在 806/1123 停于 `Events.TinkerTime.GenerateInitialOptions`，接入本批后推进到 814/1123，下一阻塞为 `Events.BattlewornDummy.GenerateInitialOptions`。

## 审计结论

这不是只增加一个事件选项列表的小任务。原版流程是：

1. 初始页面提供 `CHOOSE_CARD_TYPE`。
2. 事件专用 RNG 从 Attack/Skill/Power 三类中抽两类，并把 `ChosenCardType` 持久到事件实例。
3. 选择卡类后，从对应三种 Rider 中抽两种；选定 Rider 后创建带 `TinkerTimeType` 和 `TinkerTimeRider` 保存字段的 `MAD_SCIENCE`，加入牌组并结束事件。
4. `MAD_SCIENCE` 的三种基础效果和九种 Rider 会触及伤害、格挡、能力、能量、抽牌、随机生成战斗卡、临时免费费用和升级 Hook。

当前 Env 缺少：

- `EventState` 对 `ChosenCardType` 的明确保存/恢复字段及对应事件回调；
- `MadScience` 的两个保存属性、动态卡类型/目标/格挡查询和 `OnPlay`；
- `Strangle`、`Curious`、`Improvement` 能力及其原版 Hook（卡牌后伤害、费用修正、战后随机升级）；
- 原版事件专用 hover card 描述与随机生成战斗卡的完整候选/选择合同。

## 本批结果

- `TinkerTime` 已按原版顺序实现初始页面、卡类两选一和 Rider 两选一；每次选择均使用事件专用 RNG 的原版不稳定洗牌顺序。
- `ChosenCardType` 保存在 `EventState::extra(0)`；`MadScience` 的 `TinkerTimeType`/`TinkerTimeRider` 保存在 `CardState::extra(0/1)`。这些是模型实例字段，随通用 POD 快照和克隆遍历，不占用 RunData 的非卡牌状态。
- `MadScience` 已实现动态 `Type`、`TargetType`、`GainsBlock`、Innate 升级和 Attack + Sapping（12 伤害、Weak 2、Vulnerable 2）。Skill、Power 及其他 Rider 在 `OnPlay` 明确抛出 `Unported`，没有空实现。
- SEED0004 policy 1 逐点快照路线通过 814/1123，事件选项和牌组新增状态一致；由于路线在该处还未实际打出 Mad Science，出牌效果仍需独立夹具验收。

新采集的原版 `SEED0004` policy 1 轨迹确认了真实后继：初始类型抽到 `[SKILL, ATTACK]`，策略选择 `ATTACK`；随后 Rider 抽到 `[VIOLENCE, SAPPING]`，策略选择 `SAPPING`；牌组加入一张 `MAD_SCIENCE`，之后轨迹实际打出 7 次（目标 combat id 依次为 `3,1,1,2,1,1,1`）。因此下一批至少必须完整支持 Attack/Sapping 才能继续这条路线；不能只生成事件选项而把卡牌执行或能力效果留成空实现。其余 Rider 仍需要独立分支验收。

## 实施顺序

1. 先为动态卡保存字段建立通用卡实例字段和快照/查询合同，不把字段塞入不相关的 `extra` 槽；
2. 补齐三个能力的通用 Hook 和最小原版差分夹具；
3. 实现 `MadScience` 基础效果与 Rider，并用原版事件选择路径验证候选、RNG、卡牌字段和执行结果；
4. 实现 `TinkerTime` 事件及前缀选择恢复，加入第三幕严格路线；
5. 只有完整路径通过后，才把它从明确 Unported 阻塞移出报告。

在这些依赖完成前，不添加只让 `GenerateInitialOptions` 返回假选项的临时实现；那会掩盖后续状态和 RNG 缺口。CombatSolver 仍冻结，本批只属于通用 Env/RF 主线。
