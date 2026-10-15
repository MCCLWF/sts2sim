# NoSL 离线教师：更新到 CombatSolver 0.50.0

计划：[本轮计划](../plans/2026-10-05-nosl-combatsolver-0500.md)。原始产物：`artifacts/combatsolver-0500/`。

## 实际交付

已 fetch 上游 main/tag，锁定原始上游 `0d290fbee7e2779d2cebd8b8f652d82d00b6e8fc`（0.50.0），独立 worktree 构建并实际运行离线根。原有 `external/CombatSolver` 的 `cb4d8bcd` Env 桥接分支保留。

`tools/combatsolver/upstream.json` 记录依赖锁，`upstream.py` 默认选择该版本、核对源码 SHA、发现新旧宿主布局并保存调用/退出码/超时。`compare_search.py` 默认使用新锁，增加单根入口，并拒绝跨版本复用旧结果。用法见[工具说明](../../tools/combatsolver/README.md)。

NoSL 在线 actor 不调用搜索。本轮没有将 C++ `nosl_run --policy teacher` 更新到新版，也没有把诊断路线直接当成 BC 标签；最新版原始 C# 是独立的离线教师/诊断入口。

## 构建与实际运行

本机游戏 v0.111.0，.NET 9，使用已安装的 RitsuLib。以下 Release 构建全部通过、0 warning / 0 error：

```bash
dotnet build CombatSolver.csproj -c Release -m:2 -p:CopyModOnBuild=false -p:SteamRoot="$STEAM_ROOT"
dotnet build tools/search/OfflineSearchHarness/OfflineSearchHarness.csproj -c Release -m:2 -p:SteamRoot="$STEAM_ROOT"
```

旧版 `d7b46203593beb51f792628a2587b04f0850d1c2`（0.47.3）另建未改动 worktree，构建它原有的 `tools/OfflineSearchHarness`。旧宿主一次构建被会话中断，没有成功证据；仅重新完成该阶段，没有重建已成功的旧 Mod。

新旧使用单独 .NET 进程串行交错 `新/旧/旧/新`。Silent A10、Beam 30、节点 10000、请求预算 10000 ms、DOP 1、Smart、Coordinator、未开组合、普通 GC。两个根的新旧完整 `rootContinuationStamp` 完全相同；这证明记录范围内根一致，不是 actual/simulated 全状态部署验收。

| 根 | 新版两次请求秒数 | 旧版两次请求秒数 | 新版完整胜利 / 战损 | 旧版完整胜利 / 战损 |
|---|---|---|---|---|
| CULTISTS_NORMAL / BMS0001 | 1.316、12.950 | 13.231、13.051 | 第一次胜 / 61；第二次 TimeLimit 未胜 | 两次 TimeLimit 未胜，部分路线损血 64 |
| FUZZY_WURM_CRAWLER_WEAK / BMS0002 | 1.699、1.077 | 3.538、3.513 | 两次胜 / 8 | 两次胜 / 8 |

Crawler 中位请求耗时比为 2.54；新区间 1.077–1.699 秒，旧区间 3.513–3.538 秒。展开 3546→698，转移 11233→2017，worker 分配约 343→67 MB。这是减少搜索工作与每步成本的综合变化，不能称为相同工作量提速。

邪教徒新版有显著运行间波动：1207 节点完整胜利与 750 节点 TimeLimit；未建立稳定收益。测量期间另一会话在本机执行 CPU 基准（独立产物 `artifacts/env-sts2ai-20261005`），没有将其任务取消；本轮时间只作有负载条件下的离线诊断。**不认证统一 2–3 倍或最高 30 倍，不把 Passed 等同战斗获胜。** 官方宣传的基线是更早期优化前版本，与本轮 0.47.3 基线不同。

另用上游 Silent A10 弃牌压力输入 `coverage/corpora/novelty/sentinel-discard.json` 运行新版，Beam 30 / 节点 2000 / 10秒：宿主 Passed，2000 展开、39547 转移、27948 选择，7.601 秒，NodeLimit，未得到完整胜利。它证明新入口穿过弃牌选择路径，不构成胜路或质量不退化证据。考虑并发负载，不追加重测或扩大预算。

全部调用、根、路线、质量、搜索总工作与失败字段保存在 `runs/`，摘要 `comparison.json`。速度与内存的公开来源是上游[0.50.0 更新日志](https://github.com/Torch1230/CombatSolver/blob/0d290fbee7e2779d2cebd8b8f652d82d00b6e8fc/docs/releases/0.50.0-RELEASE_NOTES.md)，本报告仅转述其范围，不把它当成本机验证。

新版路径适配的最小调用检查：`compare_search.py --native build/combatsolver_search --case SILENT CULTISTS_NORMAL BMS0001 --beam 8 --nodes 100 --budget-ms 1000 --out artifacts/combatsolver-0500/compatibility-fixed`，1/1 compared，HP/最大HP/格挡/能量/星能/回合、有序四牌堆、两个敌人身份/HP/招式及九条 RNG 检查全部 true。两个短搜都未完整获胜，只证明工具/输入合同能穿过新版路径，不作质量或性能结论。第一轮暴露旧工具把相对输出路径传给不同 cwd 的宿主而写错位置，已统一输出绝对路径；失败摘要在 `compatibility/`，修正后直接证据在 `compatibility-fixed/`。Python 语法、CLI 与新旧宿主路径发现检查通过。

主仓库行为门禁复用 Q4-l 对 `6567b19` 的 60 CTest / L2/L3/L4 完整通过证据（`artifacts/nosl-q4l/full-gate.log`）；随后只改教师调用工具和文档，未改变引擎/ML 行为输入。上游代码未修改，不将主仓库门禁说成最新版搜索部署验收。

## 本地部署与限制

一次性备份旧 Mod 到 `pre-update-mod/`，将本轮 Release DLL、0.50.0 manifest、LICENSE、THIRD_PARTY_NOTICES.md 精确复制到已确认游戏 `mods/CombatSolver`；`deployment.json` 保存来源与文件清单。Linux 不需要 Windows MemoryCleaner，未修改其他 Mod 或旧 native 动态库，未启动可见 Steam。

尚未验证：新 C# 教师在学生实际中后期快照上的恢复/标签合同、最新搜索的原版动作回放、NoSL 蒸馏收益、实机 UI/FPS、强策略整局胜率。下一步教师实验应采学生状态并给出可执行标签与预算，不重复旧随机构筑示范的已失败路线；该适配需要独立计划，不通过跳过状态检查扩大采集。
