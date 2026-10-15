# CombatSolver Mod：原版与 sts2sim Env 比较交付

日期：2026-10-02。平台：Linux；游戏 v0.111.0；Env 基线 `90eed1b`；CombatSolver `0.47.3`（`d7b46203`）。

这次交付把两种搜索接入同一个可安装 Mod。玩家在 UI 设置一次预算（时间、节点、Beam、选择分支、线程等），Mod 从同一个实时战斗根冻结一份只读快照，先运行原版 CombatSolver，再运行 sts2sim Env。两次搜索串行执行，各自使用相同数值上限并单独计时；节点定义不同，UI 明确显示差异，不能把节点数当成等量模拟。

Env 结果只转换成可滚动的文字路线和结算摘要，不含原版 `PlanAction`，不能进入自动部署。原版全自动、手动部署、自动重算和取消继续使用原有路径。比较期间会暂停自动搜索，战斗状态变化会取消后台任务并回收结果。

## 实机证据

真实 Godot headless 场景 `ENV-COMPARISON`（Ironclad、Cultists、seed `LIVEROOT003`）通过：

```text
EnvComparison:SharedBudget:Serial:LiveUnchanged:Native=1484.3ms:Env=419.3ms:HP=62/62
EnvComparison:Locale:eng:game=eng:route=286
EnvComparison:Locale:zhs:game=zhs:route=208
EnvComparison:Locale:eng:game=eng:route=286
EnvComparison:BudgetPersistence:LocaleRoundTrip:Cancellation:NoEnvDeployment
NativeOutcome:combatEnded=False:turn=1:hp=80:maxHp=80
```

同一测试还验证了真实 Godot 控件树中的比较面板、四列预算编辑、长路线滚动文本、英文/中文往返和取消。Env 结果从未提交给部署队列，根捕获前后实时状态保持不变。

独立 live bridge verifier 使用正版游戏组件、`TestMode=false`，覆盖 Ironclad、Silent、Defect、Regent、Necrobinder；每个角色均捕获开局和实际出牌后的中途回合，并在根中加入 Fire Potion。四个角色的两个状态均返回胜利；Silent 开局在 500 节点预算下返回 `partial`，中途状态返回胜利，说明 UI/协议保留了真实预算边界。

原版自动执行回归 `SMOKE-001` 通过：Ironclad/Cultists，搜索并实际打出路线，`combatEnded=true`、第 4 回合结束、原版 `NativeOutcome` 检查通过。比较功能没有把 Env 文字路线交给自动执行。

原生 ABI 合同通过，包括实际导入、结果读取、取消、非法输入、损坏输入、源根不变、错误后复用和结果所有权释放。Env 仓库完整 CTest 为 58/58。

## 性能解释

已有同机三轮基准显示，当前 `sts2sim` Env 搜索本身并不慢：同一 21 根、Beam30、10000 节点、10 秒上限下，原生单线程搜索 46.29 秒、两线程 42.11 秒；本地 `sts2-ai` Beam 为 44.42 秒，原版 CombatSolver 为 35.62 秒。原生总战损为 349，sts2-ai 为 456，原版为 350；这是受控 A0 小样本，不是整局胜率结论。

慢点主要来自工作组织而不是 C++ 语言：原生保留更完整的 Env 快照、合法性和回放验证，21 根记录 116565 个父节点和 430032 次 Env 预测；原版记录 39354 个组合父节点和 126165 次 transition，算法口径不同。当前优化已跳过可证明重复的集中成员并复用后继/EndTurn 结果；节点数没有靠缩小预算伪造减少，动作、质量和最终 hash 保持一致。

成员级多线程只并行集中搜索成员。21 根中多数场景只有一个集中任务，理论并行度很低；两线程使搜索墙钟减少约 9.0%，CPU 时间增加约 11.4%，RSS 峰值增加约 25.2%，4/8 线程在该负载没有继续收益。四目标 Scrolls 探针曾得到更明显的 4 线程收益，因此多线程适合批量独立根或集中成员较多的场景，不会让单场搜索线性加速。

## 边界与后续

当前桥接对未知第三方模型、未映射遗物子类和未覆盖选择边界 fail-closed，报告清晰错误，不猜测字段。支持的实时根不是“任意存档完全保真”的声明；自然整局和跨机器性能仍需单独测量。Env 的优势是独立进程可复用、状态快照可校验、C ABI/Python/批量并行和只读展示安全；原版 DLL 的优势是游戏语义覆盖最完整、可直接执行。NoSL/训练不属于本次交付，也不能仅凭当前 Env 证明无预测无模拟的策略质量。

## 构建与安装

在 Env 工作树执行 `cmake --build build --target sts2solver -j8`，在 Mod 工作树执行 Release 构建并传入 `Sts2SimSource`。最终包只含托管 `CombatSolver.dll`、`CombatSolver.json`、Linux `libsts2solver.so`、`LICENSE` 和 `THIRD_PARTY_NOTICES.md`；不含游戏 DLL、源代码、测试日志或工作区缓存。把包内文件复制到游戏 `mods/CombatSolver/`，依赖已安装的 `STS2-RitsuLib 0.6.0+`。
