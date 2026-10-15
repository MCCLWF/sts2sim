# 卡牌奖励共享选项首因修复

日期：2026-09-30；分支：`fix/card-reward-shared-options`；基底：`5266daf`。本次只修改奖励规则和对应验收，不修改其它独立工作区、批次工具或 Oracle。主线 E3、通用 Env/RF 和规模验收仍未完成。

## 原版依据与修复

`Runs/CardCreationOptions.cs:143–169` 的 `WithCardPools`、`WithFlags` 修改同一个引用对象；`Factories/CardFactory.cs:89–107` 每次单卡生成与末尾 `TryModifyCardRewardOptions` 使用同一创建选项。`DingyRug.cs:31` 与 `PrismaticGem.cs:46` 更新卡池后，LastingCandy 必须看到更新后的池。旧 C++ 单卡 helper 按值复制，末尾 Hook 因而只收到未修改的池。

现在 `CardFactory` 对可修改选项使用引用贯穿整批生成，并允许调用方保存修改；常量/临时输入在入口建立可修改副本。`Reward::Populate` 同步普通 CardReward 的 Options/RerollOptions，以保留原版构造函数中两个属性共享同一对象的效果。手工设置卡的特殊奖励不做此同步。

`LastingCandy.cs:136–140` 为附加 Power 创建新选项，只保留卡池、稀有度概率及组合筛选条件，并重新设置 `NoModifyHooks | NoCardPoolModifications`；它不继承父 NoUpgradeRoll/RNG override。C++ 按此重新构造，保留既有筛选约束和排除项的交集。

## 原始首因与最短前缀

原件位于工作区 `artifacts/legacy/mass-seeds-2026-09-30/deep-v1/jobs/`，未提交大体积产物。此处索引均从 0 开始；失败决策的因果动作是前一条动作，结果摘要的 `context.action` 表示失败决策之后的下一动作。

| 原 job | 最早失败决策 | 原版 / 旧模拟器 | 原始最短失败前缀 |
|---|---:|---|---|
| `a1150551525bb662b57c`：M9AA996BBF59348 / Regent / A10 / p1552014862 | 485 | DingyRug+LastingCandy 第四张 `FURNACE` / `BLACK_HOLE` | 485 动作；末动作 `play 4 2` |
| `bb3a5ae3987515064af3`：M74AB08194DB28F / Ironclad / A0 / p1434571045 | 652 | PrismaticGem+LastingCandy 第四张 `SLEIGHT_OF_FLESH` / `STAMPEDE` | 同一原 trace 全前缀 |
| `e664b1c0d4c6a82178f9`：同 seed / Necrobinder / A10 / p518260554 | 714 | PrismaticGem+LastingCandy 第四张 `SLEIGHT_OF_FLESH` / `PAGESTORM` | 同一原 trace 全前缀 |

其它两桶只读取证：`9fd7c16fce1e8ca54c77` 的第 721 决策由 `play 4 14` 打出 Supermassive 引发，原版打 6 把 Zapbot 15 HP 降到 9，旧模拟器因 Noisebot 错把生成者传为玩家而额外增伤并杀死目标；这项由并行规则分支修复。本次没有修改 Noisebot。`ab22ec75f6e0e190001e` 的第 1619 决策由第 1024 回合 end 引发，原版进入 1025 回合，旧模拟器触发 `StateLog` 1024 容量错误；没有通过扩容或裁剪历史改变规则。

三份原始最短失败前缀和前后边界 JSON 位于 `triage-new-buckets/{supermassive-noisebot,monster-log-capacity,dingy-candy-shared-options}/`。日志容量的完整消费者审计与未实现的 bitmap/后缀保真候选方案位于 `monster-log-capacity/source-analysis.md`；其涉及快照/状态合同，保持明确失败，不能计为完整对拍成功。

## 实际验收

所有回放以原始 Oracle 轨迹为真值，开启逐决策快照恢复及后继校验。高 HP=10000 均属注入测试，不代表自然通关或全条件覆盖。

- 最终代码的三条奖励回放：Regent **991/991 identical** 到 Act 3 死亡；Ironclad **1273/1273 identical** 到 Act 3 死亡；Necrobinder 首因由 714 推进到 **1165/1382**，随后 Dampen 移除时错误升级早先未降级的 DEFILE/Strike。该后续首因由并行分支独立修复；本分支基底未包含其补丁，不能宣称这条完整一致。三条原 trace 未删除或替换。
- 五角色各一条没有 DingyRug/PrismaticGem 的旧完整轨迹：**5/5 identical、5144/5144 决策**，四胜一死，验证常规奖励未回归。它们属于一个旧 seed 的有限固定回归。
- `tools/RewardOptionsChecks` 在正版 DLL 上 **TestMode=false** 执行原生奖励 primitive：Options、RerollOptions 与传入 creation 引用相同；DingyRug 移除后 reroll 仍保留 `[REGENT_CARD_POOL,COLORLESS_CARD_POOL]`；LastingCandy 父选项带 NoUpgradeRoll 时额外卡仍消费 **3** 次 Rewards 抽取（稀有度、选卡、升级）。这是 primitive 验证，不是完整真实客户端认证。
- Release 独立构建成功。相关 CTest **3/3**：`env_snapshot`、`relics_common_m4`、`card_reward_options`。新增测试的选项持久化及三次抽取判据来自上述原版 primitive。`git diff --check` 通过。完整 main 门禁留给集成方运行，未在本分支宣称完成。

执行命令：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4 --target sts2sim_cli test_card_reward_options test_relics test_snapshot
ctest --test-dir build --output-on-failure -R 'card_reward_options|relics_common_m4|env_snapshot'
~/.dotnet/dotnet build tools/RewardOptionsChecks/RewardOptionsChecks.csproj -c Release -o .local/reward-options-oracle
~/.dotnet/dotnet .local/reward-options-oracle/RewardOptionsChecks.dll
```

严格批次分别使用 `tools/differential/batch.py replay --jobs 2 --cli build/sts2sim_cli --traces ...`，输出 `triage-new-buckets/candy-final-three/`（退出 1，保留 Dampen 分歧）与 `non-mixed-regressions-replay/`（退出 0）。实际输入、每 job 命令、原始 stdin/stdout/stderr、源码/CLI 指纹及终点在各 manifest/result 中；五条输入清单在 `non-mixed-regressions.json`。

| 指纹 | SHA256 |
|---|---|
| 最终执行/编译输入源码（严格工具定义） | `f75aa1e956d24b629a80a017e2ebd457fb0f37afef11e963b71ba55adc8dfc62` |
| 最终 CLI | `ba2661025cdfc1558408b78756abfd2ce0b9a68684bf94fdd604b1ec4ae37c31` |
| 原版 primitive probe DLL | `70daf2463d8868db6d959596704bf607fd08e65035befbdc3f47e1f13817c090` |
| 正版 sts2.dll | `2b40d2df538db1ceb5fa48d958c80ab730ada1e07db88a870aff01a661768b9f` |

源码指纹来自实际受验代码而非 commit 意图；本报告/现状 Markdown 不改变该执行指纹。原版工具曾遇到缺少 reroll history 的夹具失败，补入原生 `AppendToMapPointHistory` 后再执行，失败产物及最终成功产物均保留；没有修改游戏玩法规则。既有深幕 Oracle 的 TestMode/production-adapter 资格限制依旧保留。
