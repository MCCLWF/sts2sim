# 严格对拍工具与首批内容修复

日期：2026-09-30。分支：`feat/strict-differential-batches`，基于 `b4143be`。

用户要求：开发高效对拍工具，参考 sts2-ai 的批次/证据/分诊方法，然后补内容和修错误；简单子任务使用 gpt-6-luna / xhigh。审计报告作为问题证据，不把其中的执行建议当作额外授权或已验证结论。

后续用户要求仔细检查对拍的严格性、真实游戏资格和当前结果，无阻塞后 merge。该要求授权本次合入 main，替代开发阶段的“不合并”范围。整合只包含本批工具与规则修复；基底 `b4143be` 的未验收实机网格桥保留在原分支。另抽正常/高 HP 各 10 枚新种子（各 100 条），保留初始失败，修复后重放全部固定输入，并在 main 基底的整合分支执行完整 `scripts/check.sh`。

合并复查还须冻结 Oracle 实际解析的外部 GameData 和英文语言表，不能只记录其 bin 目录；缺依赖、清单增删或字节变化均拒绝采集/续跑资格。旧捕获缺失的外部指纹保留为资格限制，修复后的最终代码重新执行门禁、525 条固定回放与原生攻击组探针。

## 前置与范围

- 已读 AGENTS、design、roadmap、deviations、pitfalls 与 2026-09-29 完成审计。M0 基础和 M1-M4 有限实现已存在，E0 严格验收与 E3 全量内容仍未完成。
- 游戏真值限定 v0.111.0 源码与正版 DLL oracle；对方模拟器只作工具设计和第二读者参考，不搭建其未开放的宿主。
- 不解冻 CombatSolver，不部署 Mod，不改变游戏版本；main 合并按后续授权与完整门禁执行。
- 本批首先实现独立进程并行、原始证据保存、冻结 manifest、断点续跑、首因聚类、明确终点与通过前缀统计。逐抽取双端 RNG、完整原生合法动作集、guided 执行覆盖和 1 万条验收继续独立列待办，不能以本批替代。

## 工作

1. 重构 L3/L4 的结构化结果与子进程错误处理；拒绝空/残缺轨迹，严格模式拒绝截断与 Unported，不能用后续 Unported 遮盖先前分歧。保留探索模式并明确标注。
2. 增加 `tools/differential/` 批次 CLI：采集并回放 / 离线回放、并行隔离、timeout、缓存和恢复时身份/配置/产物校验、结果 JSONL、汇总与首因代表；保留 oracle/stdout/stderr/actions/命令和 hash。提供工具故障与 malformed/native crash 的回归测试。
3. 按原版修审计 F01-F09：PersonalHive/Osty、Untouchable、ScrollBoxes、SculptingStrike、Vigorous、LanternKey 快照、Illusion 复活、药水容量、降级重附魔。简单移植并行委托 6-luna xhigh，主代理负责工具、共享规则审阅与集成。
   采集端 V01/V02 同时修复：FakeMerchant 的原生库存采购、Amalgamator 的纯屏幕震动空节点，以及普通事件原生 pending-task 等待边界。保留旧无效采集，使用新输出目录重采，不能令 C++ 配合旧错误真值。
4. 先重放已保存失败轨迹，保留修复后可能暴露的新首因；再生成新的多角色种子，区分正常 HP 与提高 HP 的测试辅助。
5. 更新 README/roadmap，并写报告，记录实际原始输出、未解决问题与工具性能；不宣称整体完成。
6. 第一轮失败后继续按实际证据收敛：PersonalHive 的 null creator、PenNib 的 Osty 伤害与无头静止点计数、嵌套休息选牌的驱动收尾、入场即结束的战斗奖励采集。固定重放原 100 条新高 HP 轨迹；用新 oracle 精确重采旧 20 个无效病例，不重抽替换失败种子。
7. 第二轮首因：TheInsatiable 状态牌 creator 为 null；BingBong 接收变换后从 Deck 到 Deck 的 pile-change；SetToFreeThisTurn 同时设能量与星数、出牌/回合结束清除；在已完成动作边界联合回收未引用的能力与脱离生物，保留 Lamp/capture/target 等引用。固定语料继续重放并验证快照后继。
8. 修复第三轮出现的 HeirloomHammer 视觉卡池选择与 Repeat；补齐 VisualCardPool 的源码覆写。最终工具审查修正 probe 必须实际出牌、UTF-8 原始证据、进程中断/嵌套监督、冻结副本续跑、`.def`/系统指纹与完整输入身份。性能用同一批预先固定的 16 条 trace，串行/4 并行交替测量，不用占用率冒充加速比。

## 验收

- 对拍工具自身针对空输入、截断、计数错、先分歧后 Unported、native 非零/崩溃/半行 JSON、timeout、恢复配置及文件污染的回归通过。
- C++ Release 构建与 CTest、适用的 oracle 差分和 `scripts/check.sh`。单个修复必须对应原版源码或独立 oracle，不把模拟器输出当期望。
- 新批次保存全部预先固定 seed 与结果，不筛掉失败；统计 identical/diverged/unported/truncated/oracle_error/native_error/timeout、终点幕与实际匹配前缀。
- 对 V01/V02 原生适配、TestMode 资格、附件组合、容量上界及规模验收的剩余风险如实列出。

## 主要文件

`tests/l3/l3_combat.py`、`tests/l4/l4_run.py`、`tools/differential/`、相关卡牌/能力/遗物/附魔模型、`env/snapshot_validation.cpp`、`engine/run_state.h`、`game/Models/CardModel.cpp`、`README.md`、`docs/roadmap.md`、本批报告。
