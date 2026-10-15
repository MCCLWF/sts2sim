# E0 整局采集策略覆盖

2026-09-30，分支 `fix/seed-policy-coverage`，基于 `5266daf`。本任务只补采集策略与 L4 动作协议；未改通用 Env/ABI/RF，没有合入 main。

## 前置与真值

已读 AGENTS、design、roadmap、deviations、pitfalls。确定性基础与严格批次工具已存在；原生合法动作集合、全内容执行覆盖和万局验收仍未完成。实现位于独立 `sts2sim-seed-policy` 工作区，不改正在大规模采集的冻结工作区。

真值为 v0.111.0 游戏源码和正版 DLL。保留既有 TestMode/headless 资格边界，未新增玩法补丁。策略所有随机决策使用独立 `System.Random`，不读写游戏 RNG，不按模拟器结果修改策略。

## 修改及源码依据

- 原 `OracleRun.PlayCombat` 的 `else` 绑定在 `foreach` 内层目标检查，所有无目标牌从未进入候选；另有 Neutralize/Survivor 显式跳过。`coverage-v2` 用正确的大括号与原生 `CanPlay`/`IsValidTarget` 枚举全部卡牌。策略审计不能只查显式黑名单，必须核验实际动作覆盖。
- 原版 `NPotionPopup._Ready` 与 `PotionModel` 确认：所有权/排队/`CanUseOrRemovePotions` 限制同时约束使用和丢弃；Automatic 禁止手动使用但允许丢弃；使用还检查 `PassesCustomUsabilityCheck`。本驱动只在玩家 Play 阶段静止边界枚举，以 `PotionModel.IsValidTarget` 检查目标。
- 原版 PotionModel 明确 Self 与卡牌不同，需要玩家目标。使用走 `EnqueueManualUse`，保留原生 `UsePotionAction`；丢弃走 `DiscardPotionGameAction`，保留 `PotionCmd.Discard` 的历史和 hooks。
- 多选从原生 min/max 与候选数量限定的全部合法数量均匀抽取，再用策略 RNG 的 Fisher-Yates 生成有序子集。奖励在原生 cards/alternatives 间随机，包含 Skip/Reroll/Sacrifice；`--force-sacrifice` 继续优先。原版 `CardRewardAlternative.Generate` 与 `CardReward.OnSelect` 执行效果。
- L4 JSON combat action 新增 `use_potion`/`discard_potion`，文本为 `potion slot combat_id`/`discard slot`，CLI 映射已有 Env 动作。旧 play/end/select 保持兼容。
- 默认 `coverage-v2`；显式 `legacy-v1` 保留原有跳过、无目标牌遗漏、固定最大多选和只选卡行为及策略 RNG 调用。策略版本进入 begin、采集 manifest config/job 身份；实际捕获版本不符、未知版本或续跑改版本均拒绝。旧无版本 trace 可离线回放，按 legacy 资格理解，不能冒充新策略。
- 门禁的 80 条普通 L4 使用默认新策略；原 SEED000* 定向深幕/遗物路线显式 legacy，保留既有故障回归路径。新增 4 条 v2 Silent 选择/药水探针。

## 原始证据与结果

证据根：`../artifacts/legacy/policy-coverage-2026-09-30/`，大体积原始产物不提交 Git。

| 批次 | 结果 | 实际范围 |
|---|---|---|
| `fresh-natural-v2` | 32 identical / 8 diverged；1209/1578 匹配决策 | 第一轮开发版，尚未修无目标分支；2 个预先随机种子 × 五职业 × A0/A10 × policy 1/2，共 40 条。全部第一幕自然死亡；保留此轮证据，不作为最终覆盖结论 |
| `fresh-natural-v2-braced` | 32 identical / 8 diverged；1971/2564 匹配决策 | 用相同 40 个身份按修正后的策略重采；全部第一幕自然死亡。8 条首因均为 Large Capsule 跨职业起始牌，已交主代理按源码修复，没有换种子或放宽比较 |
| `focused-choices-v2` | 4/4 identical；1677/1677 决策 | 2 个固定 seed × Silent × policy 1/2；no-Neow、HP=10000、Driftwood/GamblingChip。首幕 Boss 前边界，每决策快照恢复及动作后继；是注入探针，不计完整自然对局 |
| `focused-choices-v2-final` | 4/4 identical；1677/1677 决策 | 门禁更新后的最终代码指纹重采同样四个身份；输入、原始动作和状态结果与上一 focused 批次一致 |
| `legacy-equivalence-qualified` | 10/10 原版策略等价；392 决策、804 事件 | 同 seed × 五职业 × policy 1/2，对冻结的 5266daf 原 Oracle 与本分支 legacy-v1 逐 JSON 事件比较，仅移除新增 begin.policy_version；所有其它字段及动作相同 |

正常 40 条的新策略实际动作：Neutralize 37、Survivor 18、DefendSilent 118、药水使用 12、丢弃 8。自用、敌人目标及 null 目标均实际发生。4 条 focused 中 Neutralize 98、Survivor 63、药水使用/丢弃各 5，Reroll 3、Skip 3；GamblingChip 实际覆盖 0、1、2、3、4、5、6、7 张多选，原生 max 为 999999999，实际数量仍由候选限制。

8 条自然样本的首分歧：`DF7AA96A3B6D3`，policy 2，Silent/Defect/Regent/Necrobinder × A0/A10；Neow 选 Large Capsule 后的首个 Map，`$.run.deck[12]` 原版按本职业生成 Strike，C++ 却是 StrikeIronclad。代表 job `d1e9f1df550a4b7aba40`。这证明新策略扩大了探索路径，同时说明正常新样本尚未全部零分歧。

最终 focused manifest：

- source SHA256：`74af8e82190c83f2911038b376660f2567750e9ca2145126409d37fae9a590ed`
- CLI SHA256：`4bb96a6218f0fece798bd61ebf29a75dc97d997ee6aff8120272425bfb9a55dc`
- Oracle SHA256：`d3d107a1df34971b1d9fd73b635f8c26087ee35442049dcd234c3001c085054e`
- manifest SHA256：`dd14c1484385fa41e46fd79bed6383c8b41a4b122d5d1bc10621d44978bcc175`
- 原冻结 Oracle SHA256：`4687fd0091b5be91b92071128e615d692f7b0363edfe54d0ec21c1cbff37b7e3`

批次保留游戏 DLL、外部语言表、.NET、命令、stderr、动作、所有结果与输入 SHA。前两轮与最终轮源码指纹不同，均在各自 manifest 记录；没有修改旧 manifest 冒称最终代码。旧 Oracle 位于冻结工作区 `.local/runtime/oracle/`，未重建或修改它。

## 验收命令与门禁

```bash
~/.dotnet/dotnet build tools/Sts2Oracle -c Release -v quiet -nologo
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
python3 tests/test_differential_tools.py
python3 tests/test_differential_batch.py
python3 tests/test_differential_matrix.py
scripts/check.sh
python3 tools/differential/batch.py run --out "$OUT/fresh-natural-v2-braced" \
  --seeds DF7AA96A3B6D3,DE1209365EF58 --policy-seeds 1,2 --jobs 2
python3 tools/differential/batch.py run --out "$OUT/focused-choices-v2-final" \
  --seeds POLICYV2CHOICE01,POLICYV2CHOICE02 --characters SILENT --ascensions 0 \
  --policy-seeds 1,2 --jobs 2 --no-neow --hp 10000 \
  --relics DRIFTWOOD,GAMBLING_CHIP --stop before-act1-boss
```

最终完整 `scripts/check.sh` 通过：46/46 CTest；L2 200/200；L3 81 完整战斗、1625 状态，另 2 个显式 Untouchable 探针、4 状态；L4 103 条固定路线、19394/19394 决策。L4 的组成是 80 条 v2 普通路线、4 条 v2 选择探针、19 条 legacy 深幕/遗物路径，边界探针不计完整整局。原始门禁日志 `check-final.log`。

工具脚本按 CTest 的隔离方式执行，通过 17 个 tools、20 个 batch、2 个 matrix 方法。初次把所有 unittest 文件在一个进程 discover 时，既有中断测试留下全局取消标记，后续进程夹具失败；按独立脚本运行全过，此测试生命周期问题已报告工具审计代理。初次新参数导致两个旧 Namespace 夹具报缺属性，也已修复。初次 legacy 比较误用不存在的默认 binary 路径失败，原 stderr 留在 `legacy-equivalence/`；确认实际冻结路径后独立目录完整比较通过。

## 剩余限制

仅增加战斗静止边界的药水动作；局外药水、完整原生合法动作集合、条件/hooks 实际覆盖账本仍未接入。新随机策略不保证每个动作或内容必达，有限通过不能视为 E0/E3/M5 完成。

发现共享 Env 的合法动作查询仍把 Automatic 药水连同 DiscardPotion 一起排除，而原版允许丢弃；已交 API/主代理处理。本任务不为迁就该遗漏限制 Oracle。TestMode 和 headless 视觉/等待适配资格仍然存在，结果只证明导出投影和静止决策边界，不是完整实机时序认证。
