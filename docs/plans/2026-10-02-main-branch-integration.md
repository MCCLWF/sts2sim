# 全分支功能合入 main 与性能口径复核

日期：2026-10-02（America/New_York）。用户要求将 sts2sim 所有分支功能合入 main，
清理其余分支，并核对搜索预算、资源占用、多线程和比较项目。

## 前置状态

- main / feat/env-rf-complete：5bd4da1；主树原有 CLAUDE.md 删除须保留。
- feat/combatsolver-env-2026-10-01：b47cf9c，已被性能分支包含。
- perf/combatsolver-env-2026-10-02：b7e08fa，完整门禁与容量专项已通过。
- perf/env-compare-2026-10-01：0481be7，从 b5ad9d5 分叉，包含 sts2-ai Env 比较工具与报告。
- 附属工作树没有未提交内容；仓库未配置远端。

## 实施

1. 在性能工作树合并 Env 比较分支；保留两组报告与工具，解决现状表的文档冲突。
2. 明确预算参数与实际展开计数不同；区分 CombatSolver 搜索和 sts2-ai Env 测量。
3. 同一代表场景、交替次序、串行新进程补测两端 GNU time CPU/峰值 RSS。
4. 构建新增 Env 比较工具，复测当前 Env 的 1/2/4/8 工作线程吞吐及源状态隔离。
5. 完整 scripts/check.sh 通过后提交合并并 fast-forward main。
6. 将其余工作树切到合并提交的 detached HEAD，使用 git branch -d 删除已包含分支。
   保留工作树目录与原始产物；确认 main 包含原各分支提交且只剩 main。
7. 重建主树产物，核对 native fingerprint、主树搜索路线及拥有型状态/portable 合同。

## 验收与范围

完整构建/CTest/L2/L3/L4 门禁通过；新增比较工具能运行；主树已有删除保持原样。
报告记录命令、原始结果、峰值内存定义、预算软边界和并行限制。
不将有限初始战斗或 Env 批处理吞吐推广为单棵搜索树加速；不改规则、搜索政策或外部项目。
