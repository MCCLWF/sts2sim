# 全分支集成与性能口径复核

日期：2026-10-02（America/New_York）。

## 集成内容

main 基线 5bd4da1 已包含基本 RF 收尾、动态容量与数值边界；
feat/env-rf-complete 没有独有增量。b47cf9c 的独立 CombatSolver 消费者已被
b7e08fa 的性能优化包含。本次在 b7e08fa 上合并 0481be7 的 sts2-ai Env 比较工具与报告，
保留两条历史及全部功能。只有 README/路线图的现状表发生冲突，代码自动合并。
两表保留新的消费者状态和两组性能证据，去掉已过时的“尚无 C# 对照”描述。

原主树 CLAUDE.md 删除保持原样。工作树目录与原始产物保留；分支清理只删除
已被合并提交包含的本地分支，不丢弃功能或档案标签。仓库未配置远端。

## 比较项目与预算

| 测量 | 对方 | 内容与限制 |
|---|---|---|
| 2026-10-01 Env 基线 | sts2-ai 6fe3336 | 180 共同根、基础 API、ctypes/gRPC、独立状态并发；两端 API/克隆范围不同 |
| 2026-10-02 搜索优化 | CombatSolver 0.47.3 / d7b4620 | 五角色、起始牌组的 21 场完整搜索；不是 sts2-ai 战斗搜索器 |

21 场逐个核对上游 harness 的输入：初始 BeamWidth=30、MaxExpandedNodes=10000、
BudgetMilliseconds=10000、MaxDegreeOfParallelism=1、Smart 用药、Coordinator、A0/ActIndex0。
上游 Profile 中默认 600000ms 不代表实际预算；离线参数通过 BudgetOverrideMilliseconds
进入 SearchBudgetLedger，真实请求限制是 10000ms。两端请求共享各自的时间/节点账本，
预算不会在每个搜索 pass 重置。它们都是软时间边界，结束前还需要完成当前操作与收尾。

**配置上限一致，搜索算法与实际工作量不等价。** 原生实现有目标组合、宽度倍增和
稳定胜利退出；上游使用其自己的 Coordinator/剪枝/退出组织。本组原生实际展开
125892，上游 totalExpanded=39354；节点计数也不是逐项等价定义。
原生 15 场 quality_stable、6 场 frontier_exhausted；没有场次以 time_budget/node_budget 结束。
因此 46.576s 对 34.820s 是同参数的结果与延迟对照，不能据此认定 Env 单步慢 1.34 倍。
两边 21/21 获胜，总战损分别 349/350 HP，最差单场原生多 15 HP。
这里不把小样本推广为中后期或全部牌组的胜率证明。

原件：任务目录 main-integration-2026-10-02/budget-audit.json，以及原搜索报告的
performance-blocks-21/report.json 和逐场 harness-result.json。

## CPU 与内存补测

相同机器、Ironclad/Cultists/CSPR0BE1、相同预算、默认缓存，每端 3 次独立进程，
交替先后顺序；测量串行执行，没有和构建/回归并跑。GNU time 包含进程初始化、运行期、
搜索与输出；单独记录的 search_seconds 排除初始化。表中是 3 次中位数。

| 指标 | 原生 Env 搜索 | 上游 CombatSolver |
|---|---:|---:|
| 搜索耗时 | 2.672s | 1.937s |
| 整个进程耗时 | 2.710s | 3.430s |
| 峰值进程 RSS | 350.258MiB | 196.520MiB |
| 进程 CPU 时间 / 墙钟 | 1.00 核 | 1.07 核 |
| 实际展开 | 7192 | 2083 |

两端搜索配置为单线程；上游运行期的后台 GC/初始化可产生额外 CPU 时间。
进程 RSS 包含 C# 运行期/游戏程序集，也包含原生完整状态缓存，不能与 managed heap 混比。
历史 21 场上游采样 working set 的中位数约 196.7MiB、范围 185.0–209.3MiB；
累计分配字节不是峰值占用。

原生后继缓存默认 8192 条、512MiB 估算 payload 上限，另有值缓存、活动搜索节点与分配器开销；
512MiB 不是 RSS 硬上限。可用 --transition-cache-mib/--transition-cache-entries 调低或关闭。
缓存为请求私有，多个并发搜索会各自持有缓存，当前没有跨请求共享。

原件：main-integration-2026-10-02/resources/report.json、每次 time.json 和两端搜索输出；
其中保存完整命令与二进制 SHA256。三个原生输出的完整动作及最终 hash 相同。

## 多线程能力

当前 CombatSolver::Solve/CLI 没有并行度选项，也没有内部工作线程；此次移植未包含
上游 ParallelExpansionExecutor。上游支持 --dop，但历史 21 场明确设为 1，未启用。
当前 C++/Python VectorEnv 只是独立状态批量包装，内部仍是串行循环。
调用端可以调度独立 Env；同一个可变 Env 不能同时被多个线程推进。

用新构建 env_compare_native，固定 Silent/A10/Cultists/DUAL001 第三回合，
每档 8192 次完整 Clone+Step(EndTurn)，预热 512，绑定不同物理 P 核，三次新进程；
第二次反转线程档位顺序。每个线程拥有独立 Env，源 hash 检查通过，sink 全部 24576。

| 工作线程 | 吞吐中位数 ops/s | 相对 1 线程 |
|---:|---:|---:|
| 1 | 9641 | 1.00× |
| 2 | 18810 | 1.95× |
| 4 | 27308 | 2.83× |
| 8 | 66634 | 6.91× |

8 线程三次吞吐约 38202–67880 ops/s，有明显桌面/调度波动；这不是单棵搜索树的加速测量。
结论是 Env 可以承载调用端的多线程工作，当前搜索消费者还没有利用它来加速单场战斗。
原件：main-integration-2026-10-02/parallel/report.json 及 12 份逐次输出/命令。

## 合并版本验收

```bash
STS2_SRC=/path/to/v0.111.0/src/Core scripts/check.sh
dotnet build tools/bench/compare/managed/Bench.csproj -c Release -p:Sts2AiRoot=/path/to/pinned/sts2-ai
python3 tools/bench/compare/run.py --mode verify --native build/env_compare_native \
  --managed tools/bench/compare/managed/bin/Release/net9.0/Bench.dll --output /path/to/evidence
```

完整门禁 exit0：57/57 CTest、L2 200/200；L3 81 完整战斗+2 有界探针，
1629/1629 状态；L4 111 局、25143/25143 决策一致。
新增 C++/C# Env 比较夹具均构建成功；新构建 180/180 根、540/540 边界对齐，0 failure。
集成没有进一步修改 game/engine/env/search/SDK/Python 的实现。

原件位于工作区 artifacts/legacy/main-integration-2026-10-02/：check-full.log、managed-build.log、
env-verify.log 和 env-verify/qualification.json。本次不将基本 RF、有限搜索样本或批处理
吞吐升级为完整 RF、全部实战搜索覆盖或 M5 万局规模资格。

## main 落地与分支清理

合并提交 eab2a1c 以 b7e08fa 和 0481be7 为父提交，main 从 5bd4da1 fast-forward 到该提交。
主树 external/CombatSolver 已同步到 d7b4620；没有修改上游源码，所需提交从现有本地参照取得。
四个原分支提交均属于 main 祖先；其他三处工作树转为该版本的 detached HEAD 后，
使用 git branch -d 删除 feat/combatsolver-env-2026-10-01、feat/env-rf-complete、
perf/combatsolver-env-2026-10-02 和 perf/env-compare-2026-10-01。本地分支只剩 main，
档案标签和工作树目录保留；主树唯一未提交变化仍为原有 CLAUDE.md 删除。

main 工作区执行 cmake -S . -B build -DCMAKE_BUILD_TYPE=Release 和 cmake --build build -j 16，
全部产物重建成功。其 native build identity 与完整门禁工作树完全相同；
新主树 CombatSolver/动态存储/快照/portable 五项合同 5/5 通过。
主树 CSPR0BE1 实际搜索的动作、质量、根 hash、最终 hash 与已验收构建全部相同，展开 7192。
原件 main-build.log、main-contracts.log、main-search.json/main-search.log。

新增 CMake 目标改变了按全部构建输入计算的 native fingerprint，旧 portable 文件仍须遵守
同构建合同；不是跨构建迁移。本轮文档落地提交不再改变该 fingerprint。
分支/工作树最终状态和原提交的祖先检查保存在本轮目录的 branch-audit.json。
