# 严格批次对拍工具与首批内容修复

2026-09-30，分支 `feat/strict-differential-batches`，基于 `b4143be678c0f53b0ef9417e90e5ba9ddb4ac39e`。本批实现可复现的并行采集/离线回放工具，修复审计 F01–F09 及新批次暴露的后续首因。完整 Env/RF 与全内容验收仍未完成。

本报告记录合并复查前的冻结开发结果。后续新增的严格性修复、525 条联合回放和 main 整合资格见[合并复核报告](2026-09-30-strict-differential-merge-review.md)；下列原始批次、指纹与测量不改写。

## 前置状态与真值

- 已读 AGENTS、design、roadmap、deviations、pitfalls，以及用户附带的 2026-09-29 审计；附件是问题证据，不是额外执行指令。
- 对方仓库已通过 MCCLWF 访问并克隆，研读版本 `<私有提交>`。参考其独立采集、append-only 结果、冻结输入、首因分诊与资格说明；没有搭建其未开放的 headless 宿主。
- 规则真值仍是游戏 v0.111.0 / `41cef1ea` 的源码与正版 DLL。`sts2.dll` SHA256：`2b40d2df538db1ceb5fa48d958c80ab730ada1e07db88a870aff01a661768b9f`。
- oracle 继续使用 TestMode 与已登记的 production/presentation 适配。所有 identical 结论限定于已导出的投影和静止决策边界，不是完整实机时序认证。
- 简单移植、定向用例和只读分诊交给 gpt-6-luna / xhigh；主代理负责批次工具、共享规则、GC、源码复核和集成。CombatSolver 保持冻结，没有合并 main、推送或部署。

## 工具交付

入口为 `tools/differential/batch.py`，用法见 [README](../../tools/differential/README.md)。`run` 独立进程采集并回放；`replay` 重用已保存真值；默认 4 个进程槽和每决策快照恢复/后继校验。

批次在执行前固定所有 job、种子、配置、源码/CLI/oracle 依赖与 .NET 指纹，保存 stdout/stderr 原始字节、stdin、真实命令、退出码/信号/超时、逐项结果和 SHA256。包含 `.def` 编译输入，续跑核对系统及相关环境。输出目录加锁；结果追加并 fsync。失败不重抽，未追加的中断产物归档后才重试。已复制的输入可在外部源文件删除后重跑；若仍存在的源文件或冻结产物被改动则拒绝恢复。

SIGINT/SIGTERM 停止排队任务并清理进程组；L4 matrix 的嵌套 supervisor 能清理自己的隔离子进程。异常 UTF-8 明确失败，保存的原始字节不替换。结构化分类包括 identical、probe_identical、diverged、unported、truncated、oracle_error、native_error、invalid_trace、timeout、tool_error；总局数、实际匹配前缀、完整终局、幕边界、显式短探针分别统计。triage 桶是首因线索，不宣称同字段已证明同根因。

批次默认严格；`scripts/check.sh` 的所有 L3/L4 命令显式严格。独立 L3/L4 脚本保留历史探索选项。空/残缺轨迹、到达上限的匹配前缀都不能通过；后续 Unported 不会覆盖先前分歧。L3 还检查显式要求的 probe 卡确实出牌，并按前一状态区分升级形态。行级 identity 保存原生 begin 的全部输入，保留 Neow、MadScience 类型/rider、药水和后续回合等区别。

## 已修复的规则与驱动

| 项目 | 原版依据与最终行为 |
|---|---|
| F01 PersonalHive | Osty 伤害归属其玩家；Dazed 的 owner 与 creator 分开，creator 为 null，避免 RocketPunch/Supermassive 额外触发 |
| F02 Untouchable | 按源 GainBlock，移除错误的 Intangible 效果，验证基础/升级探针 |
| F03 ScrollBoxes | 恢复卡包 reward-options hooks、Defect 每包的 1% Claw 分支与原生 RNG 次序 |
| F04 SculptingStrike | 选牌前过滤本地 Ethereal，恢复单候选自动选择 |
| F05 Vigorous | 新增模型/生成 dispatch，只加第一次 powered attack 的伤害，AfterCardPlayed 后禁用 |
| F06 LanternKey | 完成事件留有预建战斗布局的合法 Map 快照能恢复；未完成布局仍拒绝 |
| F07 Illusion | 复活招式必须先执行，每次死亡重置 performed；EyeWithTeeth/Parafright 两轮复活直接回归 |
| F08 药水槽 | 容纳已知三个额外遗物的非叠加组合，共 10 槽；重新生成快照 schema |
| F09 降级/附魔 | 公共 ModifyCard/OnEnchant 路径重建源效果，覆盖 Adroit、Goopy、Royal、Souls、Steady、Ember；禁止源版不支持的重复附魔 |
| PenNib | Owner.Osty 的 powered attack 也翻倍；静止点视觉 counter 与待翻倍卡引用分开 |
| TheInsatiable | FranticEscape 仍归玩家，但怪物生成的 creator 为 null |
| BingBong | 不排除 Deck→Deck 的变换；HATCH 得到两张 Swoop，LuckyFysh 对两张共给 30 金 |
| 本回合免费 | 同时临时设能量与星数为 0，出牌或回合结束清除；保留源 X-star getter 的语义 |
| HeirloomHammer | 按 VisualCardPool.IsColorless 筛选，包含 Token 池 SovereignBlade；补十个视觉池覆写与 Repeat 克隆循环 |
| 长战斗 GC | 只在已完成动作边界联合回收 retired power/脱离生物，追踪 Lamp、capture、owner/applier/target、卡牌 target、Queen amalgam 和 Dampen casters；NeedChoice 不回收 |

GC 没有通过扩大 PowerPool 隐藏泄漏：原失败在 Decimillipede 的第 274 回合达到 191 个分配槽。新用例执行 1000 次真实 Weak 应用/移除，验证活动能力、RNG 和快照往返；另验证引用保活、无根循环回收与效果执行期间禁止回收。旧 `test_env` 的返回计数更新为 14，含 13 个生物槽和 1 个能力槽，原保活断言仍在。

CLI 在验证可能失败的后继前输出当前决策，保留真正的第一处分歧；嵌套休息选牌完成后，回放驱动按既有原生策略离开休息点。

采集驱动的修复与模型修复分开：FakeMerchant 使用事件原生库存及采购方法；Amalgamator 只跳过精确匹配的屏幕震动，保留原生异步玩法及等待；普通事件等待 AwaitPendingOptionTasks，按单调时钟有界 pump 并传播异步异常；按 CombatRoom 身份重置奖励标记，处理入场即胜利的战斗。旧错误真值保留，用同一身份重采，不要求 C++ 配合错误采集。资格登记见 [deviations](../audits/deviations.md)。

## 固定证据与验收结果

本地产物根为工作区的 `../artifacts/legacy/strict-diff-2026-09-30/`，原始大文件不提交 Git。三个最终回放批次使用相同模拟器源码及 CLI：

- source SHA256：`81af2297b07f1768132b946060f5ec832dcb4573a62577443ab31af7f7efe7ff`
- CLI SHA256：`b1fff12433b1a1700ee404bb68aabd4819332c084da480f411044ca49ac51047`
- 当前 oracle DLL SHA256：`6e5ca736a8826654cd23a996119ca71c372a0e718459b50fca9338c9e616b4eb`；采集 parent manifest 另保存全部依赖/runtime 身份。

| 最终固定批次 | 结果 | 完整比较决策 | 原生终点与范围 |
|---|---|---|---|
| historical-125-final | 125/125 identical | 96,440/96,440 | 100 条高 HP + 25 条正常 HP；31 胜利、94 死亡 |
| fresh-natural-final | 100/100 identical | 3,824/3,824 | 新 10 种子×五角色×A0/A10，默认 Neow，全部第一幕死亡 |
| fresh-long-final | 100/100 identical | 85,888/85,888 | 另一组新 10 种子，同角色/升阶，HP=10000；20 胜利、80 死亡 |

以上均 policy_seed=1、stop=victory、每决策恢复/后继校验。幕号从 0 开始。高 HP 是测试辅助，其胜利数不表示正常通关率。此次新语料只有 20 个不同种子，不能写成 200 个不同种子。离线回放虽完整到死亡/胜利，仍只是该份 oracle 的已输出规则投影。

历史 125 身份逐项核验：104 条使用旧有效 trace，21 条用修复后的 oracle 重采；旧的 9 条异常 trace 原本不存在，没有伪造输入。原历史计划集合、互斥替换与所有原始 SHA 链见 `historical-125-qualified-round2.json`，最终连接见 `historical-125-qualified-final.json`。104 条旧输入的原 oracle build 未提供，明确标为 legacy；21 条新输入保留捕获 parent manifest。最终 replay manifest 不冒充其捕获版本。

新高 HP 同一批 100 条收敛过程全部保留：首次 87 identical / 11 diverged / 2 native_error；第二轮 91 / 8 / 1；第三轮 98 / 2 / 0；最终 100 / 0 / 0。修复首因后出现的后续失败继续修，没有替换失败种子。输出目录分别为 `fresh-long-100`、`fresh-long-replay02`、`fresh-long-replay03` 和最终目录；中间 `replay04` 也保留。

验收命令与日志：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 8
scripts/check.sh
python3 tools/differential/batch.py replay --out "$DIFF_OUT/fresh-long-final" \
  --trace-dir "$DIFF_OUT/fresh-long-100" --jobs 4
python3 tools/differential/batch.py replay --out "$DIFF_OUT/fresh-natural-final" \
  --trace-dir "$DIFF_OUT/fresh-natural-100" --jobs 4
# historical-125-replay-inputs.json 固定历史125的原始输入路径；实际argv在 manifest/jobs中。
```

`check-final.log` 保存最终完整门禁；`ctest-final-roots.log` 保存末次 GC/Env/工具定向验证。工具自身覆盖空输入、缺 end、上限截断、计数错、先分歧后 Unported、崩溃/半行 JSON、UTF-8 原字节、超时、矩阵隔离、中断清理、冻结输入重跑与恢复污染。构建过程遇到的测试 API 编译错误及 GC 返回计数旧断言均保留日志，修正后再验收，没有覆盖失败记录。

最终 `scripts/check.sh` 全部通过：44/44 CTest；L2 200/200；L3 81 场完整战斗、1625/1625 状态，另 2 个显式 Untouchable 探针、4/4 状态；L4 99 条固定路线、16,379/16,379 决策，包括幕边界、三幕胜利和献祭路径。L4 幕边界不计入“完整整局”，这里统计的是 gate 路线数。工具 CTest 内部另执行 14 个 protocol、14 个 batch、2 个 matrix 方法，含真实隔离子进程故障夹具。

## 工具吞吐测量

输入在 `benchmark-16-inputs.json` 固定为 fresh-long-100 原 manifest 的前 16 个 job，没有按结果挑选。每次都是 16/16 identical、12,007/12,007 决策，开启相同严格比较、快照恢复/后继与证据保存。Intel i7-14650HX/Linux，同机默认 CPU 亲和性；其它本批测试结束后执行，顺序为串行/四并行、四并行/串行、串行/四并行。

| 重复 | 1 worker 墙钟秒 | 4 workers 墙钟秒 |
|---|---:|---:|
| 1 | 23.407 | 6.793 |
| 2 | 23.065 | 7.101 |
| 3 | 22.293 | 6.635 |
| 中位数 | **23.065** | **6.793** |

中位数加速 **3.40 倍**；吞吐约 521→1768 个比较决策/秒。六批源码/CLI SHA 和逐项输入/结果签名完全相同，原始命令、逐批 manifest 和 wall/job time 保存在 `benchmark-j1-01` 等六个目录及 `benchmark-summary.json`。这是整套离线工具吞吐，包含子进程、JSON、快照验证及原始证据 I/O；不是单步引擎或 C++/C# 语言上限，也不包含 oracle 采集时间。

## 剩余范围

双端逐抽取 RNG（操作/参数/候选池）、完整原生合法动作集合、实际 hook/条件执行覆盖、guided 探索、战斗入口移植和自动最小化仍未接入；1 万条及全条件规模验收未完成。对方 demo 和历史报告不能替代这些缺口。

oracle TestMode、非交互等待和视觉适配仍限制实机资格；PenNib 的短暂视觉显示 10 等真实时序未认证。药水 10 槽仅验证已知非叠加遗物组合，其他定长容器的合法上界仍需审计，例如 temporary_star_costs 当前为 3，不能据 325 条通过宣称任意组合均无容量问题。

本批没有重新测量两语言引擎的配对 Clone/Step；上一批同机初测属于其冻结版本，不能沿用为当前状态性能或宣称 C++ 上限必然更高。本批完成的是严格批次基础与首批源码修复，E0/E3 和完整 Env/RF 仍保留未完成状态。
