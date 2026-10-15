# Env 动态容量与数值极值验收

日期：2026-10-01。范围是基本 RF 收尾后剩余的容量、资源预算和 C# 数值边界；不扩大到
live capture、悬停预测、部分档案或完整 RF 条件矩阵。规则状态仍只支持全解锁档案，状态由调用端输入。

## 结论

动态容量已替换旧的固定游戏池上限。`StaticVec<T,N>` 在小状态使用内联存储，超过提示容量后
拥有型扩容；Card/Power/Creature/Relic/Potion/Orb/Reward/Affliction 对象池使用地址稳定的
分块存储，32位引用/计数在 `INT32_MAX` 处拒绝。clone、GC、snapshot、Hash 和 canonical
scratch wrapper 均保持独立所有权。`DotnetSet` 仍是有界的源 HashSet-like 结构，只用于有限的
注册/字典槽位，不是游戏对象池。

资源预算由库加载前的环境变量控制：`STS2_ENV_MAX_SNAPSHOT_BYTES`、
`STS2_ENV_MAX_JSON_NODES`、`STS2_ENV_MAX_RESTORED_STORAGE_BYTES`。解析结构深度保持64层保护，
超预算返回错误，不截断状态。C ABI、Python 和 .NET SDK 对 action mask/observation 使用长度发现。

数值边界使用 `core/csharp_int.h` 显式表达 C# checked/unchecked 加减乘、LINQ checked 聚合及
列表计数，避免编译器溢出策略改变规则。费用、伤害、Poison、History、Dampen、药水和资源字段
的 1332 个探针逐行匹配正版 DLL。

## 证据

- `ctest --test-dir build --output-on-failure`：**55/55 通过**，原始日志为
  `.local/capacity-numeric-ctest-v3.log`。
- 注入大状态合同：`/home/wst/Desktop/MCC/sts2sim/artifacts/legacy/dynamic-capacity-acceptance-v3/summary.json`，普通快照
  6,058,774 字节；挂起选择快照 12,543,897 字节、6500 个 action、6500 mask 槽、32588
  observation 字节，均 `qualified=true`。
- .NET SDK 大状态合同：`.local/dynamic-capacity-dotnet-v2.log`，长度发现和6500-action
  前缀通过；Python 同一大状态合同包含在 CTest `env_python` 中。
- 真实保存的旧容量失败原件重放：`/home/wst/Desktop/MCC/sts2sim/artifacts/legacy/dynamic-capacity-real-replay-v1/summary.json`。
  使用正版历史 trace `FEFE3B6F36DC37D`（Regent A0、1067 个决策），当前 C++ 在严格
  snapshot-successor 比较下 **1067/1067 identical**，原件结束于 `death@act2`；这只证明
  一个历史容量失败原件已恢复，不代表全部长局矩阵完成。
- 数值逐行比较：`.local/numeric-extremes-comparison-v6.json`，native 1332 行、C++ 1332 行，
  `differences=[]`。
- 基本 RF：`/home/wst/Desktop/MCC/sts2sim/artifacts/legacy/rf-basic-acceptance-v4/summary.json`，7/7 独立进程任务、98 cases、
  188 prefixes qualified；高级 live/hover/完整因果功能按范围不做。
- 本机 C++ 基准：`.local/dynamic-capacity-bench-v1.log`，固定 Cultists 根、预热32、9×1000：
  GameState 12536 字节；Clone 6.66µs、Hash 17.44µs、Predict EndTurn 75.64µs、Preview
  trace-on 97.80µs、快照 JSON 86.70µs（均含结果销毁）。当前没有同环境、同状态、同预算的 C#
  Env 基准，因此不能据此宣称 C++ 相对 C# 的倍率。

## 仍然保留的边界

快照和 portable 仍绑定当前 native build、数学运行时及浮点配置；资源预算、JSON 深度和32位
引用/计数是明确上限。RF 输入要求全解锁档案，客户端负责传入状态；本项没有实现真实游戏悬停
即预测、实机捕获桥、部分档案或完整 RF 来源/条件矩阵。
