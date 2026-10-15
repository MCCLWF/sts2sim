# R5 猎手助手 · Linux x86_64

适用游戏：Slay the Spire 2 v0.111.0（41cef1ea），静默猎手、单人对局。
R5 用 A10 训练；其他进阶可执行，但没有相同的能力验证。全解锁老档案是验证基线。

## 安装与使用

1. 将整个 `NoslPilot` 目录放入游戏安装目录的 `mods` 中：
   `mods/NoslPilot/NoslPilot.dll`、`NoslPilot.json`、`libsts2nosl_pilot.so`、`model/`。
2. 正常启动游戏，在游戏 Mod 设置中启用 Mod 并按提示重启。
3. 正常进入猎手单人对局。默认自动接管，沿用你选择的存档、进阶和种子。
4. 拖动面板标题栏移动；右上角 `−` 收起，`+` 展开。收起后仍可点击开始/暂停。
5. **F8** 或面板按钮开始、暂停、继续；**F9** 交还手动。

暂停和交还会先完成当前动作及附带选牌；显示“已暂停”或“手动模式”后即可手动操作。
继续会重新读取实际游戏状态。打开游戏菜单时等待，关闭后继续；若需要一直暂停，用 F8。
交还后本局不会自动再次接管，点击开始可重新启用。新猎手对局会按自动接管开关运行。
非猎手和多人对局不会接管。死亡或胜利后正常保留游戏结束画面。

面板仅显示推理与控制信息：R5 / CPU、运行状态、当前决策、推理路径、最近动作、合法候选数、
推理次数、动作数、最近/平均推理耗时。不显示楼层与生命。动作间隔滑块控制额外等待，
不是游戏动画速度。位置、收起状态、自动接管开关、动作间隔会保存。

## 模型与依赖

R5 组合：combat-01690 + X1 base + f3c strategy（margin 0.02）+ table-g4（delta 0.01）
+ chain-g5 readiness（lambda 2，cards/upgrade）。默认贪心选取。

**游戏运行时不需要 Python、PyTorch、ONNX Runtime、CUDA 或额外推理服务。**
C# Mod 调用随包 C++ `.so`，CPU 本地推理。读取公开游戏状态，不进行未来模拟、搜索或回滚。
游戏自带 .NET/Godot 运行时，玩家无需另装 .NET SDK。

0.2.1 的 Linux 库以 Steam Runtime 3（glibc 2.31）为兼容基线，内置并隔离 C++ 运行库。
修复了 0.2.0 在 Steam 启动时报 `Unable to load shared library` / `GLIBC_2.43 not found`
的问题；更新整个 Mod 目录并完全退出、重新启动游戏即可，无需更换系统库。

这是 R5 的可交互部署，没有通过导出提升模型强度。R5 已能自动执行战斗和选路、奖励、
事件、商店、休息点等局外动作；A10 大样本验证尚无通关，不能保证赢下整局。
内容扩展 Mod、新游戏版本、未验证的部分解锁档案可能产生未知状态。
不要同时运行其他自动操作 Mod 的接管功能。

## 配置和问题定位

偏好文件：游戏 `user://nosl-pilot-panel.json`；动作日志：`user://nosl-pilot.jsonl`。
`user://` 是游戏实际使用的用户数据目录；Steam/非 Steam、Mod 档案可能使用不同路径。
面板出错时保留错误提示并停止自动操作；完整详情见游戏日志与动作日志。
在当前动作/选牌仍在进行时开始，面板会等待或提示先完成该动作。

模型文件必须保留 `model/` 内的相对结构。`SHA256SUMS` 可用 `sha256sum -c SHA256SUMS`
在 `NoslPilot` 目录核对。缺库、缺模型或格式不兼容时不会尝试运行 Python 替代。

## 从源码构建（开发者）

先用 `python -m nosl.export_bundle` 导出 R5，或使用已有的导出目录，然后：

```bash
bash tools/NoslPilot/package.sh /path/to/exported-model /path/to/output
```

打包需要 CMake、binutils、curl、dpkg-deb、.NET 9 SDK 和对应版本的正版游戏程序集。
脚本按 SHA256 锁定下载 Valve GCC 14 / Debian glibc 2.31 开发包，解包到
`artifacts/steam-sdk` 并在 `artifacts/nosl-steam-runtime/build` 单独构建，不安装到系统。
发布前检查 ELF：不得依赖高于 GLIBC_2.31 的符号、外部 libstdc++/libgcc 或导出 C++ 符号。
可用 `STS2_STEAM_SDK` 指定 sysroot；`NOSL_PILOT_BUILD` 指定已配置的构建目录，仍需通过检查。
不会把游戏程序集打进包。
源码里的 `NOSL_PILOT_AUTORUN=1` 是独立的开发诊断入口，正常启动不要设置它。
`panel_smoke.sh MODEL_DIR` 编译独立测试驱动，在隔离游戏副本/档案中验证面板和接管；
打包脚本会明确关闭测试编译开关，发布 DLL 不包含该驱动。
