# 原版读档/特殊地图被实机捕获拒绝

2026-10-02，Linux、正版v0.111.0。前置条件：完整模型及真实全解锁档案捕获已合入main。本任务属于原版单人状态恢复，修正调用端地图类型支持；native游戏逻辑和快照格式不变。

## 根因与支持面

用户报告持有事件所得恶魔之舌后出现 `live_map_type_unmapped`。桥接 `CaptureMap` 只接受 `StandardActMap` 和 `GoldenPathActMap`。原版 `RunManager.GenerateMap` 从保存地图创建 `SavedActMap`；`SavedActMap.cs` 明确说明原版三种生成地图恢复后共用点/连边网格，生成器类别不影响恢复后的拓扑。`SpoilsActMap` 也是原版具体类别，旧桥接同样遗漏。

恶魔之舌源码只处理伤害后的治疗与回合触发标志，没有地图修改。用原版普通图→原版保存→`SavedActMap`恢复并持有恶魔之舌，在未改行为源码上复现完全相同的拒绝。没有取得用户此次完整现场，所以不把遗物来自事件与地图类别变化的时间关系写成已证实因果。

修复明确增加 `SavedActMap`、`SpoilsActMap`，继续采集实际Grid、节点类型、可修改标志、任务标记、起点/Boss/第二Boss、有序children/parents和起步点集合；不从seed重生成输入。未知具体类别仍拒绝，错误中附上完整类型名。之前纯新局夹具遗漏读档地图；默认managed和Godot夹具现在都覆盖保存恢复。

## 验证

原始结果在忽略的 `artifacts/live-map-fix/`。

- `dotnet build tools/CombatSolverModChecks/CombatSolverModChecks.csproj -c Release` 后运行 `CombatSolverModChecks.dll IRONCLAD`：失败基线退出134，`MAP_INPUT SavedActMap relic=DEMON_TONGUE`，随后原报错，见 `baseline.log`。
- 修复后managed同一命令退出0。8个地图输入：普通/读档普通、黄金/读档黄金、双Boss/读档双Boss、第二幕特殊/读档特殊；期望值直接来自原版Map对象与公共网格枚举，独立核对native完整节点、有序连边、网格、类型、可修改和任务标记、起点与两个Boss。8/8一致。保存地图上的开局/实际出牌后搜索均得到经过重放验证的胜利，live未变。见 `maps-and-search.log`。
- `dotnet build CombatSolver.csproj -c Release -p:CopyModOnBuild=false` 成功，0警告/错误；`tools/verify-refactor-boundaries.sh`退出0。managed工具沿用OracleRun一项已有nullable警告。
- 真实Godot `ENV-COMPARISON` / `LIVEROOT003` / 铁甲战士-邪教徒们，请求注入恶魔之舌、钓鱼竿=2、白银熔炉=2/1、羽翼之靴=1。runId `5b5955ff77c247a6ba5ec5dadfedeb81`，Passed，22.74秒；`EnvComparison:Map:SavedActMap`、真实profile位图、非零计数、串行双后端、live不变、语言切换与取消通过。启动器成功删除整个隔离实例。完整命令/请求/结果见 `godot.log` 与 `godot/`。
- `scripts/check.sh`退出0、`all checks passed`：59/59 CTest、L2 200局0分歧、L3 40完整战斗784/784状态、L4 111局25,143/25,143决策。
- 已成功部署刚完成且验证过的DLL、native库、manifest和两份许可证到已确认Linux游戏`mods/CombatSolver`。逐文件原子替换保留已加载进程使用的旧文件映像；用户重启游戏加载新DLL。证据为`deployment.json`。本任务合入main并删除两个仓库的任务分支。

## 限制

本轮没有重放用户所说事件的完整奖励链，也没有当前现场存档恢复或可见Steam验证。特殊地图夹具直接调用原版生成类，不把它写成整张任务卡获取/完成链验收。未知第三方地图、占位/测试图继续拒绝；原有档案、多人、静止边界限制保持。只做Linux本地修复，不发布渠道。
