# Sts2LiveDiff

这是独立的游戏内诊断 Mod。把 Release 构建生成的 `Sts2LiveDiff.dll`、本项目的 `Sts2LiveDiff.json` 和 `build/libsts2env_c.so` 放入游戏 `mods/Sts2LiveDiff/`。要求正版游戏 v0.111.0 的 Linux 构建。

```bash
cmake --build build --target sts2env_c -j4
~/.dotnet/dotnet build tools/Sts2LiveDiff/Sts2LiveDiff.csproj -c Release \
  -p:CopyModOnBuild=true -p:ModOutputDir=/absolute/path/to/game/mods/Sts2LiveDiff
```

游戏启动后默认只观察实际战斗，输出 JSONL 到用户数据目录 `SlayTheSpire2/sts2-live-diff.jsonl`；`STS2_LIVE_DIFF_PATH` 可指定绝对输出路径。`STS2_LIVE_DIFF_AUTOPROBE=1` 是开发时的自动开局探针，不供日常游戏使用。每次不一致会记录游戏状态、Env 状态和具体差异路径，并停止当前场的 Env 预测；游戏动作继续正常执行。

当前还验证了铁甲战士从 Neow 选项经真实地图移动、完整首战、领取金币、选择或跳过卡牌奖励，再选择第三层事件并移动到第四层的连续状态。战斗奖励列表在屏幕显示及领取后逐项比较，包括领取标志；卡牌奖励候选与实际游戏选择索引也同 Env 对拍。Neow 的 Lost Coffer 嵌套奖励屏已验证领取药水或卡牌后 Skip，以及两项全领取时的自动关闭。尚未接通的其他选择会明确停止对拍。此 Mod 没有两模式 UI，也没有接管 CombatSolver 搜索；见[首批报告](../../docs/reports/2026-09-29-live-game-env-diff.md)、[连续路径报告](../../docs/reports/2026-09-29-live-run-state-diagnostic.md)、[奖励边界报告](../../docs/reports/2026-09-29-live-reward-boundary.md)、[选卡报告](../../docs/reports/2026-09-29-live-card-reward-choice.md)、[嵌套奖励报告](../../docs/reports/2026-09-29-live-custom-reward-choice.md)和[后续事件报告](../../docs/reports/2026-09-29-live-next-event-route.md)。
