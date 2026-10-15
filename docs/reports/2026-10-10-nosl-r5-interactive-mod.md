# R5 原生导出与正常游戏控制面板

2026-10-10，分支 `feat/nosl-r5-interactive-mod`，基线 `main 0ca49e8`。
用户选择部署 R5，要求正常进入猎手游戏后自动推理/操作，控制面板可移动/收起，
信息以推理为主，不显示楼层和生命。前置的 R5 bundle、C++ actor、正版观察编码与
整局动作执行器均已存在；此前 Mod 只支持开发用环境变量启动，普通游戏启动不接管。

## 实现

- 重新导出 R5：combat-01690、X1 base、f3c strategy、table-g4、chain-g5。
  六个功能文件与 `artifacts/nosl-r5/bundle-final/` 的 SHA256 完全一致。
- `Interactive.cs` 在普通启动后挂接现有猎手单人 RunState，使用 `NativeActor` 的直接观察入口。
  不创建模拟器，不更改游戏种子、进阶、解锁、档案或游戏速度；原 autorun 诊断路径保留。
- `NativeRuntime.cs` 从 Mod DLL 所在目录加载 `.so`；模型使用 `model/policy.bundle` 相对结构。
- `PilotPanel.cs` 使用 Godot 原生控件；标题拖动、折叠、展开、F8 开始/暂停/继续、F9 交还，
  自动接管和动作间隔设置。显示决策类型、策略路径、所选动作、筛选后候选数、推理/动作计数、
  最近和平均 native actor 调用时间；不提供未校准的“胜率/置信度”。偏好写游戏 user 数据目录。
- 当前动作/嵌套选择完成后暂停并释放选牌 selector；交还后销毁 native actor，本局不自动重试。
  新对局可再次自动接管；非猎手/多人不接管。退出对局时按帧检查取消，游戏结束后不退出程序。
- 正常开局比 autorun 更早看到 CurrentRoom，故等待事件同步器初始化再观察。
  奖励接管按当前可见奖励屏幕匹配 CardReward，支持手动打开奖励后重新开始。
- `package.sh` 构建发布 DLL / C++ 库，复制六个模型文件、使用说明和 SHA256 清单，生成 tar.gz；
  不携带游戏程序集、训练脚本或 Python。测试入口由 `NOSL_PANEL_TEST` 编译开关排除于发布 DLL。

## 验收记录

原始证据统一位于 `artifacts/nosl-r5-interactive/`。

### 模型与原生策略

```bash
PYTHONPATH=python .venv/bin/python -m nosl.export_bundle \
  --combat artifacts/nosl-r4/p/combat-01690.pt --base artifacts/nosl-q4u/start.pt \
  --strategy artifacts/nosl-r4/f3c.pt --margin .02 \
  --table artifacts/nosl-r4/table-g4.json --table-delta .01 \
  --readiness artifacts/nosl-r5/chain-g5.json --readiness-chain \
  --readiness-lambda 2 --readiness-scope cards,upgrade \
  --out artifacts/nosl-r5-interactive/package/NoslPilot/model
build/nosl_actor_check --trajectories artifacts/nosl-r5/heldout/final-first256.jsonl \
  --weights artifacts/nosl-r5-interactive/package/NoslPilot/model/policy.bundle --limit 64 --threads 4
```

结果 `actor-check.json`：64 局、18113 个决策全部一致，0 失败，0 战斗/局外分歧。
这是原生导出的一致性检查，不是新的模型强度评价。

### 正版直接观察整局

```bash
tools/NoslPilot/run_live.sh --seed NR5EVAL000067 \
  --weights artifacts/nosl-r5-interactive/package/NoslPilot/model/policy.bundle \
  --greedy 1 --mode direct --out artifacts/nosl-r5-interactive/autorun-direct --timeout 900
.venv/bin/python tools/NoslPilot/compare_dry.py \
  artifacts/nosl-r5-interactive/autorun-direct/pilot.jsonl \
  artifacts/nosl-r5/live-final/dry-NR5EVAL000067.jsonl
```

`compare-direct.json`：776/776 动作及摘要一致，第三幕 48 层真实死亡，557.4 秒。
涵盖战斗、嵌套选牌、地图、奖励、事件、商店、休息点、宝箱与终局；0 shadow digest 检查，
确认使用直接观察模式。native actor 调用耗时中位 8.83 ms、p95 19.03 ms、最大 46.44 ms。
测试期间另有渲染游戏和验证任务并行，耗时不能当作独占基准；不含游戏动画时间。

### 面板及生命周期

`panel_smoke.sh` 将正版游戏复制到隔离目录，使用独立存档，通过正常主菜单/选角/出发流程开局。
输入由游戏自身 Godot 控件及 viewport 输入事件驱动，截图直接来自渲染帧；未操作用户存档。
命令（依次执行）：

```bash
bash tools/NoslPilot/panel_smoke.sh artifacts/nosl-r5-interactive/package/NoslPilot/model full
bash tools/NoslPilot/panel_smoke.sh artifacts/nosl-r5-interactive/package/NoslPilot/model restore
bash tools/NoslPilot/panel_smoke.sh artifacts/nosl-r5-interactive/package/NoslPilot/model missing-model
```

三次进程均正常退出 0，各自 `panel-{full,restore,missing-model}/result.json` 为 `passed: true`。
完整流程 11 项检查通过：普通启动不新建对局；鼠标拖动；折叠缩小；暂停无动作/释放 selector；
暂停后原版结束回合按钮可用；游戏菜单挂起推理；F9 释放 actor 且不自动重启；重新接管手动打开的
卡牌奖励；退出对局清理；新猎手对局自动接管；非猎手对局不接管。中间还等待 F8 恢复推理、
关闭游戏菜单后恢复和 F9 后重新开始的实际动作。正常模式无 NoslPilot 异常。
恢复模式验证重新启动后的保存位置/折叠状态；缺失模型模式故意移走 `policy.bundle`，
面板提示错误，0 动作，actor/selector 已释放，没有崩溃。最终渲染截图检查：
`panel-full/running.png`、`paused.png`、`collapsed.png`、`panel-restore/restored.png`、
`panel-missing-model/missing-model.png`，中文可见，错误文本受限并保留 tooltip。

### 发布包与本机安装

```bash
bash tools/NoslPilot/package.sh artifacts/nosl-r5-interactive/package/NoslPilot/model \
  artifacts/nosl-r5-interactive/release
cd artifacts/nosl-r5-interactive/release/NoslPilot && sha256sum -c SHA256SUMS
```

构建成功（现存 Observer.cs:331 的一条 nullable warning）；10 个打包文件校验均为 OK。
`NoslPilot-R5-linux-x86_64.tar.gz` 为 12,517,127 字节。发布 DLL 没有 `PanelSmokeAsync` /
`SmokeNewRun` 测试符号。`native-dependencies.txt` 只有系统 C/C++ 数学/运行库，无 Python 或 ML runtime。
完整目录已部署到本机正版游戏 `mods/NoslPilot/`，安装后的同一 SHA256 清单再次 10/10 OK。
未更改用户游戏存档/设置；其他 Mod 不属于本次部署。

开发过程中修正了视口缩放下合成鼠标输入与全局鼠标位置不一致的问题（改用拖动事件相对位移），
以及普通开局事件尚未初始化就读取的时序问题。测试按钮角色选择在 press 上处理，测试驱动
改用原版 `Select()`；菜单按钮仍走原版 `ForceClick()`。早期直接在游戏地图旅行途中调用
`ReturnToMainMenu` 触发了游戏 TextureRect 已释放错误，驱动取消成功，最终正常退出用暂停边界。

### 仓库完整门禁

`scripts/check.sh > artifacts/nosl-r5-interactive/check-full.log 2>&1` 完成，退出码 0
（`check-full.exit`），末行 `all checks passed`。60/60 ctest 通过，L2/L3/L4 对拍与正常
Silent A10 Pandora / Winged Boots 整局适配回归通过。没有更改模拟器游戏规则。
`git diff --check` 和两个发布/测试 shell 脚本的 `bash -n` 也通过。

## 已知边界

- 这是 R5 部署，不是 R10/R11，也未提高模型强度；R5 A10 验证尚无通关。
- 只打包 Linux x86_64，对应 v0.111.0 / 41cef1ea。部分解锁档案、扩展内容 Mod、其他版本未验收。
- 暂停请求在当前动作和必要选择完成后生效；自动操作期间不要同时手动出牌或启动其他自动操作器。
- 对战斗/商店等任意异步步骤中强行退出、不同窗口比例、其他 Mod 组合没有穷尽测试。
- 模型/库丢失会停止接管并提示；游戏自己的退出资源警告与 Mod 逻辑错误分开记录。
- 测试为自动化正版 UI 与截图检查，未把它宣称为用户已经手工玩过或取得 A10 胜利。
