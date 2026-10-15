# NoSL 离线教师：CombatSolver 0.50.0

用户要求拉取并使用最新 CombatSolver。当前学生部署仍是 Silent A10 整局 NoSL；搜索只用于离线教师和诊断。已启动 Q4-l 控制/候选小实验保持冻结，回收产物后停止云实例。

## 任务与边界

- 拉取 Torch1230/CombatSolver 当前 main，固定为 `0d290fbe`（0.50.0）。
- 保留 `external/CombatSolver` 的本地 Env 桥接提交 `cb4d8bcd`，独立 worktree 构建未改动上游版本。依赖锁和工具显式选择新目录，不 reset 旧集成分支。
- 更新离线宿主路径发现，兼容旧 `tools/OfflineSearchHarness` 与新版 `tools/search/OfflineSearchHarness` / `.local/tool-build` 布局。
- 用新版执行 Silent A10 固定离线根及一个选择边界哨兵，保存根、路线、质量和完整请求指标；在可运行的旧版原始求解器上使用相同输入/预算，交错串行比较。时间数据只说明当前离线样本，不能外推官方 2–3 倍或最高 30 倍宣传。
- 按依赖仓库规则将同源码 Mod 精确部署到已确认的本地游戏目录；不启动可见游戏，不发布、不推送。

## 验收与产物

- 新版 Mod / OfflineSearchHarness Release 构建成功，离线根实际运行成功，失败如实保留。
- 调用工具能使用新版且记录上游 SHA；输入检查和旧行为路径继续可用。
- 计划、报告、README/路线图记录实际版本、原始产物路径与未验证范围。
- 必要的本仓库合入检查通过后提交/合入；游戏 DLL、本机路径、大体积原始数据不入 Git。

## 涉及文件

`tools/combatsolver/compare_search.py`、新版依赖锁/启动入口、`docs/reports/2026-10-05-nosl-combatsolver-0500.md`、README、`docs/roadmap.md`。不将最新 C# 引擎当作原版规则真值，不自动重写 C++ 搜索或启动大规模蒸馏。
