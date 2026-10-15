# 修复 R5 Mod 的 Steam 运行环境 ABI

用户普通 Steam 启动报 `Unable to load shared library`。实际日志说明宿主构建的
`libsts2nosl_pilot.so` 依赖 GLIBC_2.43，Steam pressure-vessel 提供的 libm 没有该版本。
前置 R5 原生推理和面板功能已完成；上一轮隔离游戏直接启动没有覆盖 Steam 的动态链接环境。

分支 `fix/nosl-steam-runtime-abi`，基于 `d918551`。保留用户现有未提交修改。

1. 使用较旧 Linux C/C++ sysroot/toolchain 构建原生发布库，保持 FP 语义和 R5 模型不变。
2. 检查 ELF 所需 GLIBC/GLIBCXX/CXXABI 版本，加入发布打包兼容门槛，避免主机升级再次产生坏包。
3. 在安装的 Steam Linux Runtime 内加载、运行原生推理，并在同样运行环境内运行隔离正版游戏。
4. 替换实际安装目录和发布包，核对全部文件；记录加载、策略一致性与游戏运行验证结果。

产物 `artifacts/nosl-steam-runtime/`。不替换用户系统/Steam 自带 glibc，不修改用户存档。
