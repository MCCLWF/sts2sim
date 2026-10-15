# Portable Env 与怪物历史摘要组合验收

受验源为 API `ec0eb1b` 与 main `575e099`（含 `0c6c596`）组合，
分支 `feat/env-rf-complete`。只有生成 schema 的哈希冲突，按组合字段重新生成；
没有调整 FSM 注册/状态顺序、回调、RNG、enabled 或原版比对字段。
历史摘要只替代私有 StateLog 表示，详细真值依据/限制沿用历史摘要报告和 D-010。

## 冻结身份

- native build SHA256：`e9587c7026428b5aa74badc25cf7115d0263d3065077664abe470819d045cdb9`。
- state schema：`5582a04e25c71fe038743d07c0c56024cfa8383dd1cda370149366b5e9c52a07`。
- shared library：`059e21d7692dffb282cc5173db42faddb6b7d3b40341dc69d26c6f2fe62cd79d`。
- CLI：`7386e11afcfe667fbeac725380c630cfd4cc55aa3a08df8318cdb9aa17729cf0`。
- GameState 782664 bytes，历史摘要120 bytes；仅表示大小，不是性能验收。

上一 portable 构建的共享库、CLI、两个fixture及Oracle DLL共5个二进制已逐SHA核对封存于
外部 `portable-ec0eb1b-runtime/`。组合构建明确拒绝上一 portable build 包，
实际diagnostic `portable snapshot: incompatible version or native build`，不做隐式迁移。

## 实际检查

所有下列进程均完整EOF、exit0，原件在工作区外 `artifacts/legacy/env-rf-api-2026-09-30/`：

| 检查 | 结果 | 原件 / manifest SHA256 |
|---|---|---|
| 完整 `DOTNET=~/.dotnet/dotnet scripts/check.sh` | 53/53 CTest；L2 200/0 mismatch；L3 81完整战斗1625状态+2有界探针4状态；L4 103/103及18956/18956决策，零Unported/其它失败 | `.local/env-portable-history-full-gate-v1.log` SHA256 `ab11205d9432bfda898d39b513bb4e8cd649d71ec3983246953a0abe1e98a5d5` |
| 组合POD对原版private GetStateWeight原件 | 123552权重、624 Last/Count、16388追加逐位一致 | `.local/env-portable-history-native-checker-v1.log` |
| 两条原长局及逐点snapshot successor | 2/2完整一致，4907/4907，death@act1和death@act2 | `portable-history-long-replay-v1` / `09015bb57dff44e0cc77d071ce0450f76ac45a1adeb3eb08d4f18cdf6fca2821` |
| 新导出的portable矩阵 | 两新进程各157 source/214后继/12非法拒绝通过 | `portable-history-model-matrix-v1` |
| TestMode=false遗物 | 8/8、36前缀 | `portable-history-relic-oracle-v1` / `268f28363510c0b49aba2021890047c08ccf84a4139b47f1f391f5457a152658` |
| TestMode=false水晶球 | 6/6、57前缀 | `portable-history-crystal-oracle-v1` / `37f92e3ce1f6de340b1717ceb70cd530e160bdc1a1203c334c7bf0e40cefb11a` |
| TestMode=false商店 | 5/5、2前缀 | `portable-history-shop-oracle-v1` / `f61bee91276b5ebe265a52ae7f2bbe0d437948a858848cd6b865d42965dd51ab` |
| TestMode=false Driftwood/Orrery重抽 | 2/2、30前缀 | `portable-history-reroll-oracle-v1` / `5e96fc7c08790a0b529f884c94b1bd1cbf2e1b0baefcfb607a5f00e837b1b298` |
| 两新进程重新消费上述RF captures | 正/反预热各125恢复/后继一致，125来自17有选择操作；原版最终事实/RNG与操作结果对齐，输入/共享库未变化 | `portable-history-fresh-rf-replay-v1` / `5a267142fd3185f7407a1f0952b4b7e33ec6fce351726eb9719e50ef5b01d899` |

命令沿用[portable报告](2026-09-30-env-portable-snapshot.md)，本轮全部输出目录换为上述新的
`portable-history-*`，不覆盖上一构建证据。长局命令及两原输入SHA保存在本轮manifest；
它们是已有native captures的离线重放，不冒充重新采集/实机路径。
157组中的111个怪物只验证immutable definition传输库存，其余状态/后继范围见portable报告。
本轮RF的21个case不是RF21项功能的全条件矩阵。Causal trace仍明确不完整。

## 主线产物复核

组合提交`68420b4`已fast-forward合入main，保留主线原有`D CLAUDE.md`。
主线checkout执行`cmake --build build -j 6`完整exit0；实际加载其共享库确认36导出、
相同native build SHA、portable=true、trace_complete=false、rf_full_matrix_verified=false。
主线9项Env/native/Python/.NET合同全部通过；主线独立编译库在新的反向预热进程中，
消费API树保存的全部157 portable根与214后继严格一致，12非法输入拒绝。
日志分别为外部`.local/env-portable-main-build-v1.log`、`env-portable-main-contract-v1.log`、
`env-portable-main-cross-checkout-v1.log`。主线本地可调用产物已更新，完整目标没有因此标记完成。

## 结论与剩余目标

组合代码达到增量合入门槛。完整通用Env/RF继续未完成：真实profile、任意实机capture、
完整Hook/来源/RNG/对象寿命trace、RF所有来源/条件与停止结果、10k零缺口及公平性能仍待验收。
seed侧1993/2000、651胜利匹配是`0c6c596`冻结版本的证据，本轮没有再次全2000运行。
两条活卡池容量失败和五条旧driver非法二次REROLL仍按原失败保留；不得计为本组合的通过。
