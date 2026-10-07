# sts2sim

《杀戮尖塔 2》（Slay the Spire 2）的非官方 C++ 无头模拟器，以及在它上面训练的整局 AI。

- 游戏版本基线：v0.111.0（`41cef1ea`），Linux。
- 最终目标：静默猎手 A10 的整局 **NoSL** AI。NoSL 指实际游玩时只做策略推理，不搜索、不模拟未来、不读档回滚。
- 本项目与 Mega Crit 无关。

## 现在有什么

| 部分 | 状态 |
|---|---|
| 模拟器 | 按游戏代码逐文件移植到 C++。随机数、Decimal、排序与 .NET 逐位一致。五个角色的卡牌、遗物、药水、事件、商店、休息点和三幕整局都已接入 |
| 验证 | 进程内 oracle 直接运行正版 `sts2.dll`（不需要 Godot），逐决策对拍。最近两批新种子整局对拍：2000 局中 1996 局一致，1000 局中 997 局一致，规则分歧 0，其余 7 局是牌池容量上限。全部内容的条件覆盖还没有验收完 |
| Env | Reset / Step / LegalActions / Clone / Hash / 快照。提供 C ABI、Python（ctypes）和 .NET SDK |
| CombatSolver 移植 | 做了简化实现，已停止移植，见下文 |
| NoSL AI | 当前重点，还很弱，见下文 |

## Env 性能

2026-10-05，i7-14650HX，单线程，180 个战斗根的中位数：

| 操作 | 耗时 |
|---|---:|
| Clone（完整 Env） | 2.0 µs |
| Clone + 出一张牌 | 36 µs |
| Clone + 结束回合 | 122 µs |
| LegalActions | 13 µs |
| Hash | 12 µs |

- 连续推进（取合法动作、选择、执行）：14,670 决策/秒；加上 NoSL 观察编码后 9,753 决策/秒。
- 8 线程独立 Clone + 结束回合：约 4.4–6.1 万次/秒。
- 同机对比 [iRyougi/sts2-sim](https://github.com/iRyougi/sts2-sim) 的公开 Core（`2ac1c87`）：Clone 约快 5.8 倍，连续推进约快 1.6–1.8 倍，单次出牌对方略快（33.5 µs）。两边克隆的范围和观察的内容不同，这些数字只作工程参考。

## CombatSolver 移植

来源是 [Torch1230/CombatSolver](https://github.com/Torch1230/CombatSolver) 0.47.3。

1. **第一版（9 月下旬）**：给原版 Mod 做专用的预测后端。已冻结，保存在 `archive/combatsolver/`。
2. **第二版（10 月 1–2 日）**：`search/combatsolver/`，原生 C++ 跨回合 Beam 搜索，直接调用 Env。评分权重、路线比较、药水政策和转置支配规则照上游翻译（34,082 例政策差分通过），搜索的组织方式是简化实现。
3. **结果**：21 场起始牌组战斗，路线质量与上游相当（总战损 349 对 350）；耗时单线程 46.6 秒、双线程 42.0 秒，上游约 35 秒。
4. **没有移植**：上游的实验排序模型、完整的铺垫/成长/资源估值、续算缓存和并行调度器。
5. **决定（10 月 3 日）**：不再继续移植。上游更新很快（0.47.3 → 0.50.0），追平它的投入不值得，而且最终目标本来就是不带搜索的 AI。现有移植只用于离线诊断和当特权教师；需要强教师时直接调用原版 CombatSolver。

## NoSL AI（当前重点）

目标是静默猎手 A10，从开局到通关的所有决策：选路、战斗、选牌、商店、休息、事件。

已经有的：

- 只含公开信息的观察编码、批量采样器、PPO 训练、原生前向推理（`nosl/`、`python/nosl/`）。
- 正版回放：把策略轨迹放到正版 DLL 上逐决策重放，1,651 条轨迹、约 31.6 万个决策 0 分歧。
- 实机执行器 Mod（`tools/NoslPilot`）：在正版游戏里无人工输入打完整局。

现状：

- 基线策略 X1 在 A10 留出的 1024 个新种子里平均 18.3 层，**0 通关**，28.7% 的局进入第二幕，没有一局进入第三幕。
- 10 月 4–6 日做了三十多组小实验（教师蒸馏、奖励塑形、信用分配、结束回合决策、输入特征等），都没有稳定超过 X1。
- 第一个站得住的提高（10 月 6 日）：战斗仍用 X1，局外决策加一条学出来的价值规则，进入第二幕的比例提高到 38.3%（配对差 +9.6 个百分点，95% 区间 [+6.5, +12.6]），平均 19.3 层，3 局进入第三幕。仍然 0 通关。
- 失败分解的结论：以 X1 现在的战斗水平，休息点不升级、绕开精英是对的。第一幕 Boss 主要检查的是牌组：在同样的战斗开局上，读真实状态的搜索胜率和 X1 相同。选牌对结果影响大，药水时机影响小。
- 正在做：改局外决策的信用分配，并在新的牌组分布上重新训练战斗。

## 构建

需要 C++20 编译器（GCC 或 Clang）和 CMake ≥ 3.20。模拟器和单元测试不需要游戏文件。

```bash
git clone https://github.com/MCCLWF/sts2sim.git
cd sts2sim
cmake -B build -S . && cmake --build build -j
ctest --test-dir build --output-on-failure
build/sts2sim_cli smoke --seeds S1,S2,S3 --no-neow 1 --greedy 1   # 整局冒烟，每局一行 JSON
```

- oracle 对拍和内容导出需要正版游戏和 .NET 9 SDK，入口是 `scripts/check.sh`。游戏 DLL、反编译源码和本地化资源不在仓库里。
- 训练需要 Python、NumPy 和 PyTorch。
- 构建模拟器不需要 `external/CombatSolver` 子模块。公开仓库里它固定在上游 0.47.3（`d7b4620`）；我们本地另有几处没有上游的补丁，依赖这些补丁的工具暂时不能直接构建。

## 目录

| 路径 | 内容 |
|---|---|
| `core/` | 与 .NET 逐位一致的基础组件：随机数、哈希、Decimal、排序 |
| `engine/` | 状态、开局、地图生成 |
| `game/` | 逐文件移植的游戏逻辑 |
| `content/`、`data/` | 从游戏导出的模型数据和生成的表 |
| `env/` | Env 与 C ABI |
| `nosl/`、`python/nosl/` | NoSL 的观察、采样和训练 |
| `search/combatsolver/` | CombatSolver 的简化移植 |
| `archive/` | 冻结的旧实现 |
| `tools/` | oracle、导出器、对拍工具、实机 Mod |
| `tests/` | 单元测试与对拍脚本 |
| `docs/` | 设计文档、偏离登记和各项源码审计 |

## 致谢

- [Torch1230/CombatSolver](https://github.com/Torch1230/CombatSolver)（MIT）：`search/combatsolver/` 的评分权重和搜索政策翻译自它，原版也是我们的离线教师。
- [hotwords123/StS2.RandomForeseer](https://github.com/hotwords123/StS2.RandomForeseer)（MIT）：CombatSolver 的部分代码源自它；我们的预测 API 也参考了它的功能。
- [iRyougi](https://github.com/iRyougi) 的 sts2-ai / [sts2-sim](https://github.com/iRyougi/sts2-sim)：项目规范、偏离登记的做法和随机数抽取账本的思路参考了他们的项目，Env 也用他们的公开 Core 做过同状态对拍和性能对照。

上游的版权声明保留在 [`search/combatsolver/LICENSE`](search/combatsolver/LICENSE) 和 [`THIRD_PARTY_NOTICES.md`](search/combatsolver/THIRD_PARTY_NOTICES.md)。

## 许可

本项目自己的代码以 [MIT](LICENSE) 许可发布。《杀戮尖塔 2》的游戏内容和设计归 Mega Crit 所有。
