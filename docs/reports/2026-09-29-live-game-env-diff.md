# 游戏进程内战斗对拍首批验收

日期：2026-09-29；分支：`feat/live-game-env-diff`；游戏：正版 v0.111.0 Linux 构建。

## 交付内容

- 独立 `Sts2LiveDiff` C# Mod 订阅游戏 `ActionExecutor` 和 `CombatManager`，记录真实 `PlayCardAction`、`EndPlayerTurnAction`、战斗内 `UsePotionAction`，用相同动作推进 C++ Env 的 owned 后继。Mod 只观察，不替游戏执行动作。
- 新增 `sts2_env_export_oracle_combat_state` 诊断 C ABI；返回既有 OracleCombat.Dump 格式。Mod 对拍结构根和完整战斗 dump：回合、血量、格挡、能量、星、Power、各牌堆、手牌费用和 Affliction、遗物及计数、药水槽、Orb、宠物、怪物意图与 RNG、run RNG。比较失败或 Env 拒绝动作时停止该场 Env 对拍并记录首个路径。
- 修正测试探针种子为游戏 UI 使用的 `SeedHelper.CanonicalizeSeed`；否则 `I` 等歧义字符会使同一显示 seed 对应不同随机流。重复开局时切换 `ActionExecutor` 订阅并释放旧 Env。

## 实际验收

- `cmake --build build --target sts2env_c -j4`：成功。
- `~/.dotnet/dotnet build tools/Sts2LiveDiff/Sts2LiveDiff.csproj -c Release --verbosity quiet`：0 警告、0 错误。
- `scripts/check.sh --quick`：34/34 CTest 通过，整局冒烟通过；此命令跳过差分批次。
- `STS2_SRC='/home/wst/Desktop/MCC/Slay the Spire 2/src/Core' scripts/check.sh`：完整门槛通过；34/34 CTest、L2 200/200、L3 80/80 战斗（784+827 状态）、L4 80/80 第一幕路线及定向二幕、三幕、献祭路线均零分歧。首次未指定 `STS2_SRC` 时，独立工作树的默认相对路径找不到原版源码，覆盖清单检查提前退出；显式给路径后完整重跑通过。原始输出保存在本机 `artifacts/legacy/live-diff-runtime/check-full-with-source.log`。
- 在隔离的正版游戏可执行文件中加载 Mod 和本分支 `libsts2env_c.so`，进入 `CULTISTS_NORMAL`，真实执行一张铁甲战士 Strike 和结束回合。`L1VED1FF009`（全解锁测试开局）、`D1FF5A73`（普通档案开局）、`61C9E915`、`E46301B9`、`5BB68ADB`（后两轮及前一轮随机抽取，普通档案开局）：各 2 个真实动作、4 个对拍边界全部 `match=true`，无 Env block；跨到第二玩家回合。
- 原始日志在本机 `/home/wst/Desktop/MCC/sts2sim/artifacts/legacy/live-diff-runtime/`，包括 `trace-012.jsonl`、`trace-normal-013.jsonl` 至 `trace-normal-016.jsonl`。不提交游戏文件和原始 trace。
- 扩展无目标牌探针后，另抽随机 seed `FA390D0C`，普通档案开局依次真实执行 Strike、Defend、EndTurn；6 个边界全部 `match=true`，无 Env block，进入第二回合。原始记录为 `trace-normal-017.jsonl`。
- 修正 `System.Text.Json` 对 C ABI 动作结构体默认不序列化字段的问题后，再抽随机 seed `7155534B`：Strike、Defend、EndTurn 的 `Kind/Index/Target/Col/Row` 均写入 JSONL；6 个边界全部 `match=true`。原始记录为 `trace-normal-018.jsonl`。
- 一次 `LIVEDIFF010` 无头启动在角色资源预加载时出现 Godot `Invalid Task ID` 并以 139 退出，当时尚未进入战斗或执行对拍。后续 5 次启动完成；该预加载偶发失败未归因到 Mod，不能算通过样本。

## 工作树与空间清理

清理前有 22 个 Git worktree。保留本分支和含用户未提交改动的根工作树；其余 20 个通过 `git worktree remove` 删除。5 个旧脏 worktree 先各自在原分支作 WIP 提交，保存为 `b1c896e`、`79272b0`、`1baf8bb`、`c915ab9`、`0cd439c`。删除 31 个已并入 main 的旧主题分支引用；未合并分支和其 Git 历史保留。确认没有游戏进程后删除 5 个旧 headless 实例目录，关键旧 trace 转存到 `artifacts/legacy/live-diff-runtime/evidence/`。磁盘可用空间从约 3.5 GiB 增至约 21 GiB。

## 当前边界

真实游戏验证范围是铁甲战士单场首回合 Strike、Defend、EndTurn 及第二回合开始。探针通过 debug 入口进入指定遭遇；普通模式使用真实档案进度，但还没有对比完整真实地图路径、Neow、奖励、事件、选择、存档恢复或后续战斗。当前 Env 在每场战斗由 seed 和遭遇重建，尚未把整局游戏状态连续导入；因此不能把本结果当作任意中途战斗的等价性证明。战斗内药水动作已接线，但本批未实机验收。没有图形 UI 或性能双模式。
