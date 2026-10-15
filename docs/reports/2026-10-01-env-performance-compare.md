# Env 性能大测试报告：sts2sim 与 sts2-ai

日期：2026-10-01（America/New_York）

结论：在同一台 i7-14650HX、Release、同一固定战斗根和相同动作投影下，180/180 个根的三处状态边界全部一致。`sts2sim` 的完整 Env 克隆约 13.4 µs，`sts2-ai` 的 CombatState 克隆约 11.4 µs；这两个克隆范围不同。包含完整 Env 快照、动作执行和回放校验的 `sts2sim` `Clone+EndTurn` 中位数为 469.0 µs，`sts2-ai` `CombatState.Clone+EndTurn` 为 106.8 µs；`sts2sim` 还提供了完整 Env 级快照和校验，因此这个比值不能解释为语言差异。C++ Env 直接调用底层 EndTurn 的中位数为 145.6 µs，说明 Env 快照/合法性/回放层约占主要额外成本。

## 范围和条件

- `sts2sim`：提交 `b5ad9d5265a1460474335545129dc5bd3dc6b5f5`，CMake Release，GCC `__VERSION__` 记录在原始元数据中。
- `sts2-ai`：提交 `<私有提交>`，.NET 9 Release，Server GC 关闭。
- 游戏基线：v0.111.0，release commit `41cef1ea`。
- 机器：Intel Core i7-14650HX，24 logical CPUs；测试绑到 logical CPU 2（P-core 1），governor 为 `powersave`。测试期间存在桌面负载，结果是本机工程决策数据，不是跨机器排行榜。
- 根矩阵：5 角色（Ironclad/Silent/Regent/Defect/Necrobinder）× A0/A10 × 3 代表遭遇（Cultists、Exoskeletons、Mecha Knight）× 3 seed × 首回合/第三回合 = 180 根。
- 每个有效根：预热 128 次；7 组样本；每组 64 次（极短操作按 4 倍循环）；三次独立进程，语言交替顺序。所有根均通过根 hash 未改变检查。

## 核心稳态耗时

单位是 µs/次；数值为 180 个有效根的中位数，括号为根间 P05–P95。C# 还记录每次操作的线程分配量。

| 操作 | sts2sim C++ | sts2-ai C# | 解释 |
|---|---:|---:|---|
| Clone | **13.39 (12.70–14.08)** | **11.42 (9.01–16.29)**，30.6 KB | C++ 是完整 Env；C# 是 CombatState 图克隆，不能直接排名 |
| Clone + EndTurn | **469.03 (403.35–558.99)** | **106.79 (51.10–188.44)**，169.3 KB | 前者包含 Env 顶层快照、合法性和回放路径 |
| Clone + PlayCard | **360.73 (345.22–374.02)** | **31.95 (20.61–45.82)**，61.3 KB | 同上，工作范围不同 |
| 直接底层 EndTurn | **145.59 (82.33–234.87)** | 未提供同等公共调用 | 用于分解 C++ Env 层成本 |
| Predict EndTurn | **502.09 (429.89–588.98)** | 未提供同等公共调用 | C++ 包含分支合法性和前后验证 |
| Hash / ExactCombatDigest | **13.58** | **68.62**，88.0 KB | 哈希定义不同；不能按数值宣称优劣 |
| LegalActions | **26.29** | **22.51**，33.1 KB | C# 使用等价动作枚举夹具 |
| Observe | **26.95** | **81.88**，129.9 KB | 两边观测编码字段和容器不同 |

配对比值（C++/C#，仅用于发现工程热点）：`Clone` 几何均值 1.15×；`Clone+EndTurn` 4.69×；`Clone+PlayCard` 11.52×。后两项主要反映完整 Env 的快照/回放设计与 C# 战斗对象图 API 的范围差异。

## 并发吞吐

固定 Silent A10 Cultists 第三回合根，执行 8,192 次“克隆 + EndTurn”，每档三次独立进程。单位是 ops/s，speedup 相对该语言单线程档。

| 工作线程 | sts2sim C++ | sts2-ai C# |
|---:|---:|---:|
| 1 | 5,526 (1.00×) | 2,199 (1.00×) |
| 2 | 10,851 (1.96×) | 6,676 (3.04×) |
| 4 | 21,073 (3.81×) | 11,369 (5.17×) |
| 8 | 28,915 (5.23×) | 12,546 (5.70×) |

两边均未出现状态交叉污染。C# 并发测量每批约 1.285 GB 线程分配；C++ 使用定长状态和手工池，未记录 GC。

## C ABI/Python 与 C# gRPC

`sts2sim` ctypes（Silent A10 Cultists，稳定样本中位数）：Clone 13.1–33.9 µs，Clone+EndTurn 401.9–451.5 µs，Predict 430.7–484.1 µs，Observe 58.6–62.6 µs，LegalActions 28.4–35.6 µs，Hash 13.3–14.2 µs；带 Observe+LegalActions 的完整 ctypes 回合为 507–567 µs。Ironclad 根的 `Preview` trace-off 1.70 ms，trace-on 1.77 ms，带 RNG/causal 特权诊断 28.0 ms；snapshot JSON 0.73 ms，portable snapshot JSON 1.41 ms。

`sts2-ai` gRPC 服务（8 局，默认服务 A10；策略取每次返回的首个合法槽位）全部返回第一幕死亡，0 失败：123 次 Step，总墙钟 91.95 s，即 1.34 calls/s。Echo 往返中位数 0.977 ms；Reset 到首个策略决策中位数 44.9 ms。产品 Step 中位数 1.39 ms，但 P95 3.54 s，最大 5.91 s；秒级调用来自 C# 战斗搜索，不能归因于 gRPC 通信。

原始 Python Gym 解码器在其中一次产品回放遇到历史候选槽与 legal mask 不一致（候选 `[590,591,592]`、mask `[590,591]`），因此产品吞吐采用 protobuf 直连，未把这个严格解码失败伪装成成功。该接口缺陷应另开修复任务。

## 验证与产物

- `ctest -R '^(env_m3|env_preview|env_snapshot)$'`：3/3 通过。
- 配对预检：180/180 根，540/540 边界（root、after EndTurn、after basic card）一致。
- 三轮核心计时：C++ 和 C# 每轮 180/180 根成功，0 failure。
- 原始产物：`/home/wst/Desktop/MCC/sts2sim/artifacts/legacy/env-performance-2026-10-01/results/`。
- 工具与清单位于本工作树：[`tools/bench/compare`](../../tools/bench/compare/)，计划见 [`docs/plans/2026-10-01-env-performance-compare.md`](../plans/2026-10-01-env-performance-compare.md)。
- 主要二进制 SHA256：`env_compare_native` `b1a24990…c6b17`；`libsts2env_c.so` `dca3c03b…20d8d`；C# Bench `dbcacf76…3e0ec`；<私有组件> `fc1f6ed6…17487`。

这次没有修改任何模拟器规则；性能报告也不升级完整 Env/RF 或自然整局保真资格。下一步最直接的优化点是减少 `Env::Step/Predict` 的重复静止点快照和 `LegalActions` 纯查询副本，并在 C# 侧复用 CombatState 克隆对象/降低每次分支的 100 KB 级分配。
