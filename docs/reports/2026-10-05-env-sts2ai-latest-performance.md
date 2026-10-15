# 最新公开 sts2-ai Core 与我们的 Env 性能比较

日期：2026-10-05（America/New_York）。计划：[任务计划](../plans/2026-10-05-env-sts2ai-latest-performance.md)。

**我们仍有采样性能优势，但不是所有操作都更快。** 配对战斗根的克隆约快 5.81 倍，克隆加结束回合约快 12.2%；C# 克隆加出牌约快 7.9%。更贴近 rollout 的受控连续推进，带当前公开观察编码时，单线程为 9,753 对 5,493 决策/秒，约 1.78 倍。这个比值不是完整 PPO 训练速度或整局胜率。

## 版本与公开范围

- 我们：`6567b19431691ab86d41b4672e842a17c5b4009e`，独立 Release 构建，GCC 15.2.0。
- 对方：[iRyougi/sts2-sim](https://github.com/iRyougi/sts2-sim)，已 clone 最新 `2ac1c87f10b0a403b5bf71da8a2b714c1c9bc220`，2026-10-05 07:35:12 UTC 发布，同步私有上游 `bf20e47f`。完成测量后再次 `ls-remote`，HEAD 仍相同。
- .NET SDK 9.0.316、runtime 9.0.18，Release，基准消费者使用 Workstation GC（Server GC=false）。不把这个配置当作他们私有训练服务的实际配置。
- 双方游戏基线均为 v0.111.0，`41cef1ea`。公开仓库只有 Core 与测试，没有最新训练脚本、搜索器、训练模型或 Python/gRPC 服务；没有继续使用旧私有 checkout 冒充最新版。
- 同机 i7-14650HX，24 逻辑 CPU，Linux。标量绑 CPU2；并行使用 2/4/6/8/10/12/14/0。powersave governor，有桌面负载；本结果是工程测量，跨机排名不适用。
- 主工作树同时存在另一个 CombatSolver 更新任务；本轮构建与源码拉取均隔离在忽略的产物目录，保留共享 checkout 的分支和其他改动。后续共享 HEAD 的 CombatSolver 提交不进入这次冻结的 Env 二进制。

## 同状态资格

180 根：五角色 × A0/A10 × Cultists/Exoskeletons/MechaKnight × 三 seed × 第一/第三回合。

- 180/180 个共同根合格，540/540 个边界（root、EndTurn、基本牌）投影相同。
- 六个标量进程均 180/180 成功，0 failure，原根 hash/digest 未改变。
- 投影包括 HP、资源、有序牌堆、能力、宠物、敌方状态及战斗 RNG counter；不是每个内部字段或 RNG s0–s3 的全状态证明，不升级自然整局保真资格。

## 标量操作

每根 128 次预热，7 组 × 64 次；极短操作增加循环；三个独立进程交替顺序，并轮换根顺序。先取每根跨进程中位数，再取 180 根的中位数。单位 µs/次。

| 操作 | 我们 C++ Env | 最新 C# Core | C# 分配 KB/次 |
|---|---:|---:|---:|
| Clone | 2.01 | 11.59 | 30.8 |
| Clone + EndTurn | 121.66 | 135.75 | 201.1 |
| Clone + PlayCard | 36.10 | 33.52 | 64.0 |
| 克隆 + 直接底层 EndTurn | 104.49 | — | — |
| Predict EndTurn | 137.70 | — | — |
| LegalActions / 等价枚举夹具 | 13.20 | 13.96 | 17.2 |
| 基础 Observe / Core ObservationEncoder | 13.78 | 35.26 | 63.0 |
| Hash / ExactCombatDigest | 11.60 | 18.74 | 17.7 |

配对几何均值 C++/C#：Clone 0.172（180/180 根较快），EndTurn 0.891（173/180 较快），PlayCard 1.079（19/180 较快）。所以不能由总体中位数把我们写成“所有场景更快”。

克隆范围不同：我们复制完整 Env，C# 是 CombatState 的战斗投影，包含的整局上下文并不等价；这不是纯语言比较。Observe 和 Hash 的字段/编码/合同也不同，不能把表中倍率当同一输出的计算效率。C++ 分配量没有采集，表中的空白不是零分配。

标量进程峰值 RSS 的三轮中位数：C++ 11.68 MiB，C# 97.91 MiB。C# 每轮约 6,300 次 Gen0 收集；这不是完整训练进程的内存。原生 Env 顶层 EndTurn 比直接底层路径多约 17 µs，仍有包装开销，但不是旧 10 月 1 日报告中的数百 µs。

## 连续推进：更接近 rollout

18 个 Silent 第一回合根：三 seed × A0/A10 × 三遭遇；基础牌优先，否则 EndTurn，每次连续执行 8 个动作。每根每进程重建 8 次，三轮交替进程。没有逐动作显式 Clone、搜索、神经网络、IPC 或 Python 解码。

无编码/有编码两档都先取 LegalActions，然后按相同动作推进；有编码档另外调用 C++ `nosl::Observe` 或 C# `ObservationEncoder.EncodeCombatDecision`。C++ 这里用的是当前公开 token 观察，不是上表较简化的基础 `Env::Observe`。双方字段仍不同；C# 公共 EnemySnapshot 不含敌人模型 ID、能力列表等字段，不能据此认定整个私有训练 actor 的信息合同。

C# 此诊断使用 `DOTNET_TieredCompilation=0`，在首次 JIT 时完成优化；动态 PGO 同时关闭。下面不是默认 PGO 热态的严格上界/下界。

| 排除建根的单线程流水线 | C++ 决策/秒 | C# 决策/秒 | C++ / C# |
|---|---:|---:|---:|
| 合法动作 + 选择 + 执行 | 14,670 | 9,156 | 1.60× |
| 再加当前公开观察编码 | 9,753 | 5,493 | 1.78× |

带编码时的分项 µs/动作：合法动作 12.2/26.4，观察 34.1/71.0，选择和执行 56.2/84.5（C++/C#）。

此强制战斗建根夹具约 6.0/156.3 ms（C++/C#），包括各自的新局、地图与指定幕/房间初始化，未做地图/整个 RunState 等价资格，不能宣称“正式 Reset 快 26 倍”。每 8 个动作都重建一次的吞吐为约 1,182/51 决策/秒，主要反映夹具重置比例；不将它冒充正常整局采样速度。正常长局会摊薄重置，但还包含局外步骤。

18/18 初始诊断路径相同；两种编码档 × 三轮 × 两种语言共 216/216 最终边界及动作串一致，每档每语言总计 3,456 个执行动作。范围是第一回合起的短战斗窗口，不包含完整 Choice、局外、后期复杂构筑或长局。

## 独立分支任务并行

固定 Silent A10 Cultists 第三回合，8,192 次独立 Clone + EndTurn，每档三轮。这是独立任务吞吐，不是单场搜索延迟，也不是多进程整局 PPO 采样。

| 线程 | C++ 默认测量 ops/s | C# 默认 JIT/PGO ops/s | C++ 补测 ops/s | C# 关闭分层编译 ops/s |
|---:|---:|---:|---:|---:|
| 1 | 9,421 | 1,620 | 9,523 | 4,541 |
| 2 | 18,603 | 6,167 | 17,172 | 8,501 |
| 4 | 34,150 | 8,484 | 29,916 | 12,207 |
| 8 | 61,011 | 11,441 | 43,952 | 7,372 |

默认短测量只有 512 次预热；C# 单线程读数明显受启动分层编译影响。补测移除该影响，但也关闭 PGO，因此两个配置不能混成一个“最佳”排名。8 线程补测 C++ 三轮为 44.0k/46.3k/30.4k，C# 为 9.47k/7.37k/7.23k，存在明显波动。两配置均显示我们在该独立分支负载的吞吐领先，8 线程中位倍率约 5.3–6.0；不能推广成整局训练快 5–6 倍。根 digest/hash 均未被并发任务改变。

## 最新代码带来的实际变化

相对上次公开 `c6282abc`，这次有 19 个同步提交，113 个文件变化，约 +6,436/-773 行，包含玩法修复和以下新增接口：

1. Hook 监听来源快照缓存，成本修改与牌堆变化 Hook 的覆写类型索引；这不是缓存玩法返回值。每个克隆从冷缓存开始。稳定状态查询的收益不能直接移用到频繁换牌堆的 rollout。
2. `RunState.CloneExact()` 与 `CloneReseeded()` 已在代码中实现，局外稳定边界可深克隆/重采样隐藏未来。README 仍写“整局不支持深克隆”，描述已落后。活跃战斗、未成功完成的回调/生命周期、未结束的 semantic scope 等仍会拒绝；活跃战斗继续用 CombatState.Clone，不能视为任意决策点的通用整局快照。
3. 隐藏状态与克隆字段登记、特殊地图、当前房间/奖励图复制以及多处怪物和药水修复。公开说明仍保留 TheArchitect 未建模的限制。

参考最新代码：[RunState.Clone](https://github.com/iRyougi/sts2-sim/blob/2ac1c87f10b0a403b5bf71da8a2b714c1c9bc220/src/Sts2Sim.Core/Runs/RunState.Clone.cs)、[HookListeners](https://github.com/iRyougi/sts2-sim/blob/2ac1c87f10b0a403b5bf71da8a2b714c1c9bc220/src/Sts2Sim.Core/Combat/CombatState.HookListeners.cs)、[CombatRoom 边界](https://github.com/iRyougi/sts2-sim/blob/2ac1c87f10b0a403b5bf71da8a2b714c1c9bc220/src/Sts2Sim.Core/Rooms/CombatRoom.cs)。

**对我们的意义**：当前 Env 速度不是已经被对方反超的短板。值得保留的优势是便宜的分支复制、独立任务并行，以及有限连续采样的约 1.8 倍优势。下一步性能决策应使用实际整局 worker + 观察/特征 + 推理 + IPC 的 profile；无法从公开 Core 推断对方训练时长、学习效率或模型强度。对方新增局外世界克隆已经缩小旧功能差距，不宜继续沿用“只能克隆战斗”的旧结论。

## 复现与验收

全部原始产物与辅助消费者在 `artifacts/env-sts2ai-20261005/`，忽略且不提交：

- `upstream/`：原样 clone 的公开仓库，git status 干净。
- `metadata.json`：提交、CPU/runtime/compiler 配置、初始工作树状态、二进制 SHA256。
- `results/`：180 根清单、qualification、六个标量进程、24 个默认并行进程及汇总。
- `rollout/`：新增消费工具、18 根清单、资格、12 个连续推进进程、216 边界复检和汇总。
- `steady-parallel/`：关闭分层编译的 24 个补测进程及汇总。
- `interrupted/`：桌面/App 重启中断的第一次标量进程，126 根已完成；不进入结果。
- `rollout/pilot-128/`：128 次重建的试测，暴露重置成本后主动缩减诊断；不进入最终汇总。
- `rollout-managed-build-failed.log`：辅助工具第一次构建的作用域/元组名称错误；修正后最终构建成功。核心源没有改动。

```sh
cmake -S . -B artifacts/env-sts2ai-20261005/native-build -DCMAKE_BUILD_TYPE=Release
cmake --build artifacts/env-sts2ai-20261005/native-build --target env_compare_native -j 4
# dotnet Bench 使用 -p:Sts2AiRoot=<公开 checkout> 和独立 --artifacts-path；详情见构建日志。
python3 tools/bench/compare/run.py --output <results> --native <native> --managed <Bench.dll> --mode verify
# 相同参数分别 --mode measure、--mode parallel；之后 summarize.py <results>。
# pipeline.py 构建/验证/测量连续推进消费者；parallel-steady.py 跑补充 JIT 配置。
```

两边 Release 构建成功；公开 Core 首次构建有 3 个既有 CS0114 warning、0 error；辅助消费者最终 0 error。没有修改游戏规则或 Env 实现，没有以这次性能实验重跑/替代整局门禁。文档 `git diff --check` 通过。
