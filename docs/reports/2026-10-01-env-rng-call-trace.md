# Env 特权 RNG 调用观察的有限验收

基线 `b3dad8f`，分支 `feat/env-rf-complete`。本批只完成完整因果追踪计划的 RNG 观察层；
完整通用 Env/RF 目标继续 active。没有修改随机算法、抽取次数、选择规则或 Oracle 比较字段。
API 分支尚未集成 main `702d14f` 的 coverage-v3 驱动增量，不能把本报告的回归数字移用于那个版本。

## 行为与合同

显式 preview flags=2 收集 RNG；flags=3 同时查询 future；普通 run-query flags 仍只接受0/1。
Python `include_rng`、.NET `includeRng` 和 typed operation flags 已接入。默认不收集 RNG，
普通 agent observation 不增加隐藏信息。trace_limit=0 加 RNG 请求明确拒绝；失败无后继，源状态不变。

记录每次实际 Rng 方法调用的类型化参数/返回位模式、前后四字状态/counter、父调用、开始/结束顺序，
并归属实际 owned run/player/event/encounter/monster 流。Gaussian 内部重试、重载、随机下标、
权重选择与洗牌子调用均保留。局部/复制 Rng 明确 unclassified，不推断持久身份。
observer 是线程隔离的借用诊断上下文，没有加入 GameState/Rng；状态继续 POD。
容量预检同时计算 frame/call 两个记录，嵌套失败恢复父上下文。Answer 替换完整操作重放前缀。

`trace_complete=false` 与 `rf_full_matrix_verified=false` 均保持。完整 Hook、来源实例、
对象创建/退休和引用重用、全部结果事实仍未完成。

## 冻结与原始证据

原件在仓库外 `artifacts/legacy/env-rf-api-2026-09-30/`；原生 capture、原失败和旧构建证据保留。

- native build：`43c2bf102cfb6cab2dbcee8d69c8df699f5575891f652355eff9bb2f334d609f`。
- state schema：`02661f7e4c9ce6d2b47ed31cbad52390013664611c26cded43b40611c5b40f0a`。
- shared library：`2071bcff045507e0ff1b6caacdaccf36a6eb439ada51b77494b46014769673cf`。
- 原版 DLL：`2b40d2df538db1ceb5fa48d958c80ab730ada1e07db88a870aff01a661768b9f`。
- RNG evidence manifest：`809a85a26be4057d1944dd71a801fefabefe5328c2e2a417ea481ce6db51c34b`。

## 实际检查

所有下列进程完整 EOF、exit0；没有放宽判据。

| 检查 | 结果 | 原件 |
|---|---|---|
| `tools/RngExecutionChecks` 调用正版 DLL，TestMode=false；`build/rng_execution_checks <native.jsonl>` | 8 seeds、2880 顶层操作、11376 调用逐项参数/位模式/前后状态/父子/顺序一致；288洗牌排列一致；其中262个Gaussian调用含重试 | `rng-execution-v1/native.jsonl`、`checker.log`、`manifest.json` |
| scoped engine / C ABI / Python / 独立 .NET 合同 | 流归属、嵌套/线程隔离、容量失败恢复、trace on/off完整快照一致、权限flags、坏输入无后继、Choice前缀通过 | 外部 `.local/env-rng-contract-v2.log` |
| 完整 `DOTNET=~/.dotnet/dotnet scripts/check.sh` | 54/54 CTest；L2 200/0 mismatch；L3 81完整战斗/1625状态及2有界探针/4状态；L4 103/103、18956/18956决策，零Unported/其它失败 | `.local/env-rng-full-gate-v1.log`，SHA256 `95345eda717b7e334139cd276db4567019363ef97cc9af88c3343f0076798a77` |
| TestMode=false 公共 RF，显式 `--rng-trace` | 遗物8/8、36前缀；水晶球6/6、57；商店5/5、2；Driftwood/Orrery重抽2/2、30。trace开关完整后继相同，最终原版事实/RNG对齐；共21case/125前缀 | `rng-relic-oracle-v1`、`rng-crystal-oracle-v1`、`rng-shop-oracle-v1`、`rng-reroll-oracle-v1` |
| 新进程从上述同构建portable包恢复并继续Answer | 正/反预热各125源/后继及全部RNG调用记录逐项一致；最终事实仍对原Oracle原件，冻结输入和共享库未变化 | `rng-fresh-rf-replay-v1`，manifest SHA256 `56726b06d383a242b3baf67a4dc47ccea00f9ce694ffb96c0ad7ba59d949a9cb` |

RF 命令沿用 portable 报告的四组参数，加 `--rng-trace` 并换到新的 `rng-*` 目录；
fresh-process 命令的四个 `--input` 指向这些新目录。独立 RNG 探针构建/执行命令见
`tools/RngExecutionChecks/README.md`。其 Harmony 补丁只观察原方法，不修改返回或跳过随机逻辑。

首轮合同使用尚未耗尽抽牌堆的 Cultists 终回合，没有实际随机抽取，三个“必须有记录”断言失败。
保留 `.local/env-rng-contract-v1.log`；随后用普通 EndTurn 先耗尽初始抽牌堆，真实洗牌动作再验收。
没有伪造 RNG 记录或添加抽取。此失败属于探针前置条件，不能当作规则修复证据。

## 完整验收缺口与合入状态

这21case是有限 RF 子集，不是RF21项功能全条件矩阵；11376调用是RNG组件对拍，
不是11376整局或完整双端游戏调用追踪。真实档案、任意实机capture/连续恢复、完整Hook/寿命/结果，
RF所有来源/条件/停止结果、1万条零缺口整局与公平C#/C++性能仍待验收。没有新增性能认证。

另一个对话的新2000批尚在运行，main与其程序冻结。已保存 RoomFullOfCheese.Gorge+BingBong
的逐张/批量Add顺序分歧，以及NECROBINDER长战斗活卡池1023满的真实容量失败；前者的
原版EventModel helper确实逐张await Add。HP10000是注入深度探针，其资格限制保留。
本批没有修改这些规则、未覆盖其失败，也没有把它们计为通过。
当前增量已满足自己的门禁，但本轮保持在API分支，不在冻结采集中修改或合入main。
