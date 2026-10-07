# CombatSolver 离线教师与诊断

当前原始 C# 教师锁定在 `upstream.json`：0.50.0，`9a4489d840a3772aef451d5c6dc42dd79719f380`。NoSL 部署继续只执行公开观察的直接推理；此教师只在离线使用。`external/CombatSolver` 保留旧 Env 桥接分支，**不代表本轮最新版**；C++ `combatsolver_search` / `nosl_run --policy teacher` 仍是单独维护的旧移植，不自动获得新版优化。

从仓库根准备独立上游源码（不覆盖桥接代码）：

```bash
git -C external/CombatSolver fetch origin main --tags
git -C external/CombatSolver worktree add --detach ../../artifacts/combatsolver-0500-latest/source 9a4489d840a3772aef451d5c6dc42dd79719f380
cd artifacts/combatsolver-0500-latest/source
# 游戏与 RitsuLib 必须存在；按本机安装指定 SteamRoot 或 local.props。
dotnet build CombatSolver.csproj -c Release -p:CopyModOnBuild=false -p:SteamRoot="$STEAM_ROOT"
dotnet build tools/search/OfflineSearchHarness/OfflineSearchHarness.csproj -c Release -p:SteamRoot="$STEAM_ROOT"
```

执行一个 Silent A10 离线根（从 sts2sim 根目录）：

```bash
python3 -B tools/combatsolver/upstream.py --out artifacts/teacher/example -- \
  --character SILENT --encounter CULTISTS_NORMAL --seed BMS0001 --ascension 10 \
  --beam 30 --nodes 10000 --budget-ms 10000 --dop 1 --search-mode Coordinator
```

包装器默认核对锁定源码 SHA、兼容新旧宿主产物路径，记录 `invocation.json`、退出码、超时与 `harness.log`。宿主另写根 continuation stamp、动作路线、预计战损及请求总工作量；这些是搜索诊断，不能当作原版部署保真验收。默认总进程上限 120 秒，每次使用新输出目录。

`compare_search.py --native <C++ executable> --out <dir>` 默认与锁定的新 C# 教师比较旧有的 21 个 A0 起始牌组根；`--case SILENT CULTISTS_NORMAL BMS0001` 可只运行一个根（可重复指定）。这套工具原有输入范围仍是 A0。`--upstream <checkout>` 可显式指定历史版本。`--reuse-upstream` 仅接受此前报告同一个上游 commit，防止用旧结果冒充新版。同源码、输入及预算未变化时复用证据；大规模 NoSL 教师采集另定实验，避免直接重复已证实不利的旧 BC 方案。

真实学生根使用另一个入口，保持全局 TestMode=false 和原版正常本地化，不调用上游
测试模式初始化器。先构建 `tools/NoSLTeacherHarness`（链接锁定上游搜索辅助源码及 DLL）：

```bash
dotnet build tools/NoSLTeacherHarness -c Release -m:1
PYTHONPATH=python .venv/bin/python -B tools/nosl/student_roots.py collect \
  --checkpoint artifacts/nosl-q4j/x1/checkpoint.pt --out artifacts/student-roots
.venv/bin/python -B tools/nosl/student_roots.py batch \
  --collection artifacts/student-roots --out artifacts/student-teacher
```

collector 只让学生读公开观察。batch 使用冻结动作前缀抵达第 8 层起首个战斗根，证明搜索
前后状态/完整 continuation 相同；再用精确手牌状态键、出现次序、药水槽和选择游标绑定
教师路线，原版真实执行，并通过既有 L4 逐决策和额外终局资源/RNG 比对。战斗路线是整局
前缀，不称整局完成；teacher.json 的 `bc_labels_qualified=false` 表示尚未导出学习表。
每次新目录保存失败、限时和 provenance；Rolling Boulder 视觉伤害回调仍未支持。
小样本门槛和覆盖见 [Q4-n 报告](../../docs/reports/2026-10-05-nosl-q4n-production-teacher-root.md)。

合格原版路线的公开学习表由独立导出器生成，不直接把 teacher.json 当作学生输入。
导出器逐个真实 Env Step 重建 Observe/ActorFilter，选牌包含数量和有序选择，保留 int32。
每个公开表和标签另外通过训练 ABI 从种子开始回放验证；单动作不作为学习行，过滤拒绝
或表/后继不一致则明确失败。隐藏扰动只在可 determinize 的稳定边界检查；待选牌操作
保持原引擎拒绝语义，仍检查准确标签、公开表和快照后继。

```bash
cmake --build build --target nosl_teacher_export
PYTHONPATH=python .venv/bin/python -m nosl.teacher_distill export \
  --batch artifacts/student-teacher --out artifacts/public-labels \
  --min-qualified 96 --min-quality-fraction .25
PYTHONPATH=python .venv/bin/python -m nosl.teacher_distill train \
  --data artifacts/public-labels --checkpoint artifacts/nosl-q4j/x1/checkpoint.pt \
  --out artifacts/distilled --seed 1 --epochs 8 --lr 1e-5 --minibatch 128 --kl-coef .5
```

训练要求学生 SHA 与采集基线相同、公开数据 SHA 相符且采集门槛通过。按根分出独立留出，
只有实际战后至少改善 3 HP 的路线进入 CE；固定学生公开 anchors 约束策略 KL。
value head 参数冻结，共享编码器仍会变化。teacher 一致率和局部 HP 收益都不等于整局
进步；Q4-o 的固定评价为 `NQ4OEVAL000000`–`NQ4OEVAL000511`，使用 greedy_eval 的
`--seed-width 6` 和 `nosl.teacher_distill compare` 保存全部尝试、故障及配对 bootstrap 区间。

完整跑局的离线教师参照使用独立工具。每场战斗以准确原版决策编号捕获稳定根，
`capture-only` 在搜索后停止，附加脚本载体不执行；原版路线、Env/快照、公开表及训练
ABI 全部核验后才继续学生局外动作。每局真实终点再独立作正常规则整局对拍。
该入口使用特权教师，不能作为 NoSL 部署或学生胜率；失败/限时不进入合格学习数据。

```bash
PYTHONPATH=python .venv/bin/python -B tools/nosl/whole_run_teacher.py run \
  --seed TRAIN_WHOLE_RUN_001 --checkpoint artifacts/nosl-q4r/start.pt \
  --out artifacts/teacher/whole-run-001
```

当前固定先导的来源、种子、预算和扩大采集门槛见
[Q4-x 计划](../../docs/plans/2026-10-05-nosl-q4x-whole-run-teacher.md)。
调用原版需提供正确的 `STS2_GAME_DIR`、`STS2_SRC` 和 `DOTNET`；`STS2_SRC` 的 Core 目录
决定本地化表位置。云端仅有 .NET 运行时时可迁移已构建的 Release 程序集，依赖和
运行配置路径须明确记录，不能以启动失败推断策略质量。
