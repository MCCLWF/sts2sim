# 完整通用 Env / RF API

> 2026-10-01 用户调整范围：以[基本 RF、容量与极值计划](2026-10-01-env-basic-rf-capacity.md)
> 为本轮交付标准。仅全解锁档案；Env 提供输入规范，调用端/live UI/高级因果账本不开发。
> 下文保留历史完整计划，不再作为本轮完成条件。

日期：2026-09-30。基线：5266daf，分支：feat/env-rf-complete。

## 目标与责任

执行用户要求：完整 API 达到可验收级别。范围沿用
`2026-09-27-generic-env-completion.md` 的 E0–E5，不以新接口数量或有限测试代替完整验收。
本分支负责 API 合同、原版入口执行、恢复/预测、查询/追踪、独立 SDK、RF 消费者验证。
另一个对话负责大量新 seed 差分、规则补漏；双方使用独立 worktree 和冻结产物，交换可复现证据。
CombatSolver 继续冻结。

## 实施项

1. ABI discovery：版本、内容/状态 schema/模型指纹、状态码、所有权与能力范围。
2. Typed operation：普通 action、遗物取得、按实际来源的转换、奖励与商店原版入口；
   参数引用绑定 source hash，不允许把另一个状态的实例下标当作当前实例。
3. 完整 pending operation 的 clone/hash/snapshot/choice 前缀重放；新增停止点也采用重放。
4. 明确 operation 完成、选择、敌方阶段、结算边界；未实现的停止条件必须拒绝。
5. 查询与执行事实：实例、资源、生成/转换、顺序、来源及特权 RNG；getter 在 owned clone 上执行。
6. 独立 Python / .NET SDK：生命周期、诊断复制、失败分支不可搜索、typed operation 与恢复。
7. live capture/restore、真实 profile、RF 功能矩阵与正版探针；未经验证的项目保持未完成。
8. 与 seed 对话的规则提交整合后执行完整检查及最终覆盖/性能门槛。

## 验收与证据

- SDK 消费的是实际构建共享库；检查导出 ABI、跨生命周期 successor、Choice 续接、坏输入、
  trace 截断、source 不变、快照事务恢复与拒绝跨 runtime/schema。
- 新 RF 操作逐项记录原版源码入口、实际 RNG 来源、停止状态、正版 Oracle 比较。
  RF 本身仅为功能需求参考，不能作为游戏规则真值。
- Snapshot 必须证明 pending prefix 可从 stationary base 再现，不能导入任意中间状态冒充决策。
- API 覆盖清单区分 implemented、Oracle-verified、unsupported；报告保留未覆盖条目。
- 最终验收要求完整 E0–E5、10k 整局零 Unported/差异、trace/SDK/RF 原版 matrix，
  `scripts/check.sh` 通过；不因本分支局部合同测试通过而标记全目标完成。

## 涉及文件

`env/`、`engine/sim/`、必要的原版命令/边界观测点、`python/`、`sdk/dotnet/`、
API 合同测试/Oracle 探针、CMake、文档与验收工具。原版逻辑改动先与 seed 对话协调。

## 共享规则及 TestMode=false 奖励输入集成

在 Crystal Sphere 公共查询批次后，集成 seed 聊的 `8b8c820 → f63ae51`，
保留 API 的 ChoiceOption.enabled，并重新生成组合后的 schema。完整门禁及公共变牌、
商店、水晶球和遗物原版回归单独冻结；不把另一分支的 53 失败集或全2000进行中当作
当前 API 分支的全量通过。

关闭 TestMode 的 Calling Bell 原版操作回归发现 Oracle 仍创建 Godot 奖励屏幕。
将已验证的奖励 screen/input adapter 统一用于 Env 操作探针，保留原版 RewardsSet.Offer、
同步器、CardReward.OnSelect 和效果；重新执行五角色/A0/A10 四种遗物矩阵。
恢复每个 scoped patch，保留失败原件；完整目标及其余 RF 功能继续待验收。
