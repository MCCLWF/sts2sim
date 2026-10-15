# R5 Mod 0.2.1：Steam 原生库加载修复

## 前置状态与根因

基于 `d918551` 的 R5 交互式部署，分支 `fix/nosl-steam-runtime-abi`。
用户普通 Steam 启动报 `Unable to load shared library`，完整日志保存为
`artifacts/nosl-steam-runtime/user-godot-before.log`。底层原因是 `libm.so.6` 没有
`GLIBC_2.43`，不是模型文件或 `.so` 路径缺失。

0.2.0 用宿主 GCC 15 / glibc 2.43 构建，还依赖 GLIBC_2.38、GLIBCXX_3.4.31、
CXXABI_1.3.15。此前直接启动隔离游戏的检查使用宿主库，没有覆盖 Steam 容器。
本机 Steam Snap 的 sniper 容器实际从 Snap 导入 glibc 2.39；纯 sniper 平台是 2.31。

## 修改

- `prepare_steam_sdk.sh` 按锁文件 SHA256 获取 Valve GCC 14 和 Debian bullseye glibc 2.31
  开发包，解压到本地 `artifacts/steam-sdk`；处理重定位头文件和 sysroot 绝对软链接。
- `cmake/SteamRuntime.cmake` 单独构建发布库；保持原 FP 编译选项，静态链接 libstdc++、libgcc。
- `STS2_PILOT_C_ABI_ONLY` / `pilot_exports.map` 只导出 `nosl_*`，隔离 C++ 符号，避免与游戏
  已加载的旧 libstdc++ 相互覆盖。普通构建默认不启用。
- `package.sh` 默认用上述工具链，并在打包前检查 ELF 架构、GLIBC 上限、动态依赖与导出；
  指定其他构建目录也不能跳过检查。Mod 版本升到 0.2.1。
- `native_smoke.c` 可在无 Python 的环境直接 dlopen 并验证模型加载、异常处理和释放；
  面板测试脚本允许指定兼容构建与独立输出目录。

六个模型/规则文件与已安装 R5 逐文件 `cmp` 一致。未改变策略、游戏规则、用户存档或面板偏好。

## 验收命令与结果

所有原始产物在 `artifacts/nosl-steam-runtime/`。下面 `RUNTIME` 指安装的
`SteamLinuxRuntime_sniper/run`，`MODEL` 指发布包 `NoslPilot/model/policy.bundle`。

### 发布库兼容性

```bash
bash tools/NoslPilot/package.sh artifacts/nosl-r5-interactive/package/NoslPilot/model \
  artifacts/nosl-r5-interactive/release
bash tools/NoslPilot/check_steam_abi.sh artifacts/nosl-steam-runtime/build/libsts2nosl_pilot.so
```

`package.log` 构建/打包成功；`new-abi.log`：最高需求 `GLIBC_2.29`，动态依赖只有
`libpthread.so.0 libm.so.6 libc.so.6 ld-linux-x86-64.so.2`。不依赖动态 libstdc++ / libgcc。
旧库被同一检查拒绝，记录 `old-abi.log`。

```bash
artifacts/steam-sdk/usr/lib/gcc-14/bin/x86_64-linux-gnu-gcc-14 \
  --sysroot="$PWD/artifacts/steam-sdk" tools/NoslPilot/native_smoke.c \
  -ldl -static-libgcc -o artifacts/nosl-steam-runtime/native_smoke
"$RUNTIME" --graphics-provider= -- env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libstdc++.so.6 \
  "$PWD/artifacts/nosl-steam-runtime/native_smoke" \
  "$PWD/artifacts/nosl-steam-runtime/build/libsts2nosl_pilot.so" "$MODEL"
```

`new-loader.log`：RTLD_NOW、actor C ABI、缺失模型异常、R5 加载与释放全部通过。
旧库在同一纯 sniper 环境复现 `GLIBC_2.43 not found`（`old-loader.log`）。
预加载运行环境自带的旧 libstdc++ 后新库仍通过。

### 推理一致性

```bash
"$RUNTIME" --graphics-provider= -- "$PWD/artifacts/nosl-steam-runtime/build/nosl_actor_check" \
  --trajectories "$PWD/artifacts/nosl-r5/heldout/final-first256.jsonl" \
  --weights "$MODEL" --limit 64 --threads 3
```

`actor-check.json`：64 局、18113 决策，same=18113，failed=0，战斗和局外分歧均为 0。
655 次局外替换。这是同一模型跨工具链的回归，不是新的强度评价。

### 正版游戏与面板

先尝试纯 sniper 2.31 的整局测试，正版游戏自带的 Fmod / Spine / Sentry 扩展要求
GLIBC_2.32，故游戏资产加载失败、0 动作（`live-direct/`）。保留这个失败记录，不能算通过。
随后进入实际 Steam Snap，再由其 pressure-vessel 启动隔离游戏，使用 glibc 2.39，
与用户报错时的启动环境一致；`snap-runtime.log` 与 `game-library-maps.txt` 记录实际库路径。

整局命令：在 `snap run --shell steam` 内执行 `$SNAP_USER_COMMON/.../SteamLinuxRuntime_sniper/run --`
后传入 `env NOSL_PILOT_BUILD=... STS2_GAME_DIR=... bash tools/NoslPilot/run_live.sh`
及 `--seed NR5EVAL000000 --weights MODEL --greedy 1 --mode direct --out .../live-snap-direct --timeout 600`。
游戏使用独立副本、XDG 目录和测试档案，强制关闭 Steam API 以隔离云存档。

`live-snap-direct/launcher.json`：exit=0，0 启动崩溃，0 超时；整局正常死亡于第二幕
22 层。`compare-snap-direct.json`：278/278 动作与摘要完全相同，same=true，0 shadow
digest 检查，说明使用的是直接公开观察推理。游戏内耗时 261.1 秒，原生推理中位
7.86 ms / p95 16.77 ms。测试与其他回归并行，不能视作独占性能基准。

面板复测：

```bash
NOSL_PILOT_BUILD="$PWD/artifacts/nosl-steam-runtime/build" \
NOSL_PANEL_TEST_OUT="$PWD/artifacts/nosl-steam-runtime/panel-full" \
  bash tools/NoslPilot/panel_smoke.sh artifacts/nosl-r5-interactive/release/NoslPilot/model full
```

`panel-full/result.json`：passed=true，11 项检查全部通过，包括拖动/收起、F8 暂停和
恢复、暂停时手动操作、游戏菜单挂起、F9 交还、重新接管手动打开的奖励、退出清理、新局
自动接管、非猎手不接管。此项在宿主带窗口运行，使用与发布包相同的兼容原生库；
截图 `panel-full/running.png` 已目视检查，面板显示 R5 已加载与推理次数/耗时，没有楼层/生命。

### 完整门禁与部署

`scripts/check.sh > artifacts/nosl-steam-runtime/check.log 2>&1`：60/60 单元测试，
全部对拍及检查通过，末行 `all checks passed`。

发布包 `artifacts/nosl-r5-interactive/release/NoslPilot-R5-linux-x86_64.tar.gz`；实际
游戏 `mods/NoslPilot` 已替换为发布目录，10 个文件的 SHA256 全部通过。
对实际安装路径再次在 Steam Snap 容器中执行加载探针，通过（`installed-snap-loader.log`）。
面板测试后重新构建关闭测试入口的 DLL，与发布包逐字节一致。
旧安装完整备份到 `artifacts/nosl-steam-runtime/installed-before-0.2.1/`，不在 Mod 扫描目录中。
玩家运行时仍无需 Python、SDK 或额外推理服务；R5 模型能力没有改变。
