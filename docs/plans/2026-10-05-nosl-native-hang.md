# NoSL 原生挂起排查

日期：2026-10-05；分支：`fix/nosl-native-hang`。

目标：查明 2026-10-04 两次 worker 超时是否来自合法轨迹上的原生循环或异常慢路径。
本项属于 Silent A10 NoSL 的 E01/E03 环境保真与失败可复现工作；现有轨迹重放、
反编译 v0.111.0 源码和正版 oracle 已具备，训练资格不作额外前置。

1. 将 `artifacts/nosl-q4f/kt1/failures.jsonl` 中 80 条 hang 按 Silent/A10/filter=1
   及 auto_forced 语义逐条重放；每次一个进程，5 秒限时，超时立即杀死并保存结果。
2. 对复现项输出每个策略/强制动作执行前后的进度，定位决策与 C++ 调用栈；
   必要时单进程重跑 checkpoint-00489 的贪心种子，避免 500 局集体丢失。
3. 对照反编译源码与 oracle 确认首因，仅修真实规则错误；加入来自真值的有界回归。
4. 修复后重放原始 80 条及故障 oracle 路径；更新报告、路线图与 README。
5. 使用 `.venv` 的 Python，低优先级/受限 CPU；仅在
   `STS2_SRC='/home/wst/Desktop/MCC/Slay the Spire 2/src/Core' taskset -c 0-5 nice -n 10 scripts/check.sh`
   全绿后合入 main；为同树 Trial 任务分开暂存，可先提交已通过定向验证的分支修复。
   显式路径暂存，保留 `CLAUDE.md` 未提交删除，不推送。

涉及文件：诊断工具、定位到的规则实现、定向回归、CMake 注册、上述文档。
验收：80 条逐条结果完整，停住动作可复现且定位明确；若修复则源码/oracle 支持、
定向回归与全门禁通过；无法复现则如实报告环境、命令、限制，不虚构原因。
