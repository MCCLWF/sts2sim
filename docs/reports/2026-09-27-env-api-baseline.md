# 通用 Env API 本机基线报告

## 结果摘要

在固定 Ironclad / Cultists A0 战斗根上，以当前普通 Env/Preview API 测得 `sizeof(GameState) = 145,936` 字节。每项为 9 个样本、每样本 1,000 次操作，另预热 32 次。表中平均值是 9 个样本的平均单次耗时，中位值是 9 个样本平均单次耗时的中位数。

| 操作 | 每样本迭代 | 样本数 | 平均 (ns/次) | 中位 (ns/次) |
|---|---:|---:|---:|---:|
| `Env::Clone`（含销毁） | 1,000 | 9 | 2,908.15 | 2,908.05 |
| `Env::Hash` | 1,000 | 9 | 10,306.78 | 10,316.10 |
| `Env::Predict(EndTurn)`（含销毁） | 1,000 | 9 | 109,698.22 | 108,163.75 |
| `PreviewAction`，trace 开启（含销毁） | 1,000 | 9 | 121,224.64 | 121,549.24 |
| `PreviewAction`，trace 关闭（含销毁） | 1,000 | 9 | 121,920.35 | 122,034.38 |
| `Env::ExportSnapshotJson`（含返回字符串销毁） | 1,000 | 9 | 130,461.65 | 130,432.63 |

固定根 Hash 为 `9426525947303475788`。普通 Predict、trace 开启 Preview 和 trace 关闭 Preview 的后继 Hash 均为 `9342032363122373350`；基准程序报告验证前后根 Hash 不变。trace 开启的这个 EndTurn 示例记录 1 个 frame、10 个 execution events 和 4 个 power events。

计时 lambda 不额外调用 Hash、Observe 或快照 API 来形成摘要：Clone 读取 `copy->state().run.total_floor`；Predict/Preview 读取后继的 `run.phase` 和 `player_combat.energy`，Preview 另混入 trace event 数。`PreviewAction` 自身的普通实现工作（包括其内部源状态检查）仍计入 API 耗时。Hash 稳定性与 Preview/Predict 后继一致性校验在计时区间之外完成。第一轮的 Clone/Predict/Preview lambda 曾额外调用后继 Hash，Hash 成本混入结果；该轮这些耗时已作废，不作为本报告基线。

## 实际命令与日志

从仓库根目录执行：

```bash
g++ -std=c++20 -O2 -ffp-contract=off -fno-fast-math -I. \
  tools/bench/env_api_bench.cpp \
  build/libsts2env.a build/libsts2game.a build/libsts2engine.a \
  build/libsts2content.a build/libsts2core.a \
  -o /tmp/sts2sim-env-api-bench \
  > /tmp/sts2sim-env-api-build.log 2>&1
/usr/bin/time -p /tmp/sts2sim-env-api-bench \
  > /tmp/sts2sim-env-api-run.log 2>&1
```

两条命令均以退出码 0 完成；编译日志为空。校正后的运行总耗时 `real 4.51s`、`user 3.53s`、`sys 0.97s`。`/tmp/sts2sim-env-api-run.log` 保存校正后的程序原始输出和 `/usr/bin/time -p` 结果，`/tmp/sts2sim-env-api-build.log` 为空。

## 环境与版本边界

- 采集时间：2026-09-27 10:52 EDT；x86_64 Linux，内核 `7.0.0-29-generic`。
- CPU：Intel Core i7-14650HX，系统报告 24 个逻辑 CPU、16 核。
- 源码树 HEAD：`84d59b69b675b6ff2e35e56b278b7a7a2c18a120`（分支 `feat/generic-env-rf-api`）；运行时工作树有未提交的 Env 集成修改，因此 HEAD 不是这批对象代码的独立版本标识。
- 编译器：GCC 15.2.0；基准翻译单元使用 `-O2 -ffp-contract=off -fno-fast-math`。共享工作区在本次采集前已完成包含 `EnergyNextTurnPower` 的库构建；本任务只单独编译基准程序并链接 `build/` 中的静态库。CMake 缓存显示 `Release`、`-O3 -DNDEBUG`。
- 本次链接的静态库 SHA-256：`libsts2env.a` `bf29fa57a92f0897f4e390e00e8983fd4ac9b97286a50d839c27c9447da0930f`；`libsts2game.a` `e12ec033b46241a7e9fca20d87ab7c274da0714958e4232ca3ab7010ad3bf0af`；`libsts2engine.a` `ec230e38422be575a8b8caa1549507289a4171fedd65831e42ee68814cc7191d`；`libsts2content.a` `75a1f800993b5e780cc3ff527be2af44dbbb82389c36eb6985a6101dfbe2f521`；`libsts2core.a` `7b71bf5d240c8bf15f40513359ae3b2c75defba393e846a805790affd57acbbf`。
- 固定根通过 `Env::ResetCombat(NewRunOptions{seed="DUAL001", character=IRONCLAD, ascension=0}, CULTISTS_NORMAL)` 建立。每次 Predict/Preview 都从同一个未推进根独立执行 `Action::EndTurn`；计时包括返回 Env/PreviewResult 及其拥有的状态、trace 的销毁。快照项计入返回 JSON 字符串销毁。每项另有 32 次预热。
- 测量只覆盖 [`Env`](../../env/env.h) 中的 Clone、Hash、Predict、ExportSnapshotJson，以及 [`PreviewAction`](../../env/preview.h)。不包含 MCTS/ISMCTS、Beam、搜索评分、客户端策略、完整 RF 功能矩阵、真机或其他机器对比。

## 结论与限制

本报告只代表当前本机、当前静态库产物的一次基线采集，不是性能验收。没有固定 CPU 频率或处理器亲和性，也没有跨进程重复；单次采样不能证明 Clone/Step 微秒目标或其他性能目标已达成。
