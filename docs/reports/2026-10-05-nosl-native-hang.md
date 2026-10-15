# NoSL 原生挂起排查与修复

日期：2026-10-05。计划：[NoSL 原生挂起排查](../plans/2026-10-05-nosl-native-hang.md)。
基线：`2a25f3e`，v0.111.0，Silent A10、actor filter 1、auto_forced。
现有 Env/记录动作重放/oracle 可用；本次不是全内容或训练胜率资格认证。
原始证据与全部输出保存在忽略目录 `artifacts/nosl-native-hang/`。
失败日志 SHA256：`3f7aef215f13c644c2e3698f34885d82e78e03489dd0455339b181774c1697ce`；
checkpoint SHA256：`9679e3a7d28819856868db3333e51fca68f61f605a219c87b8d941262f0212d0`。

## 结论和首因

**是模拟器规则移植错误，合法轨迹可永久循环，不是正常的分钟级计算。**

80 条记录中只有 `NT100652020008441` 重现；另外 79 条只是同 worker 的在途前缀。
第 191 个策略决策（索引 190）在第二幕第 18 层的 Darv 事件取得 Sozu；
第 199 个策略决策（索引 198，动作表行 0）在第 19 层
`EXOSKELETONS_WEAK` 战斗、HP 56 时使用此前携带的 Entropic Brew。
调用前已经执行 262 个 Env 动作，包含自动强制动作；该调用从未返回。

`game/Models/Potions/EntropicBrew.h` 原先忽略 `TryToProcure(...).success`。
Sozu 的 `ShouldProcurePotion` 始终拒绝给持有者获取药水，药水槽一直为空，
`while (player.HasOpenPotionSlots())` 一直成立，反复消耗 RNG 并分配被拒绝的药水。
GDB 的 worker 栈为 `CreateRandomPotionsOutOfCombat → CreateRandomPotionOutOfCombat →
PotionModel::OnUse → OnUseWrapper → RunFlow::UsePotion → Env::Run → Diagnose`。
线程没有等待外部 IO；原版规则有退出条件。

真值为反编译 `Models/Potions/EntropicBrew.cs:24–32`、`Models/Relics/Sozu.cs`、
`Commands/PotionCmd.cs`，以及下述正版 DLL 探针。C++ 修复逐字保留生成/获取顺序，
获取失败时退出；没有提前按遗物跳过生成，没有新增迭代上限或修改 RNG。

## 重放、回归和 oracle

所有 Python 均通过仓库 `.venv`，重放 `taskset -c 0 nice -n 10`、每次一个进程。
首次逐条 5 秒限时筛查为 **79 ok / 1 timeout**，返回项 0.018–0.147 秒。
加入 `nosl_decision_diag --progress` 后，以下命令重新确认同一个动作边界：

```bash
taskset -c 0 nice -n 10 .venv/bin/python tools/nosl/replay_timeouts.py \
  --trajectories artifacts/nosl-q4f/kt1/failures.jsonl --status-name hang \
  --ascension 10 --actor-filter 1 --out artifacts/nosl-native-hang/before --timeout 3
```

原始结果：`{"runs":80,"statuses":{"ok":79,"timeout":1},"timeout_seconds":3.0}`。
每次 timeout 由 `subprocess.run` 杀死并等待退出后才开始下一条。
最后的 flushed `before_step` 是上述索引 198；原始日志没有变更。

同一命令将输出目录改成 `after`，修复后结果为
`{"runs":80,"statuses":{"ok":80},"timeout_seconds":3.0}`。
80 条合计 6.12 秒，单条最慢 0.214 秒（含进程启动与进度日志）。
故障局执行到第 265 个 Env 动作后的 Choice；记录在选牌中间耗尽，不能称整局完成。

`tests/test_entropic_brew.cpp` 覆盖战斗内外 × 有无 Sozu × 有无保留药水，共 8 例。
验证用掉熵之酿、保留已有槽位、正常补满，以及失败仍生成一次（2 次 RNG）的完整 RNG 后继。
修复前同一测试被 3 秒限时杀死；修复后 `ctest -R '^entropic_brew_procure_failure$'`
通过（0.08 秒）。CTest 配置 5 秒 timeout，回归不会长期占用机器。

```bash
STS2_SRC='/home/wst/Desktop/MCC/Slay the Spire 2/src/Core' taskset -c 0 nice -n 10 \
  /home/wst/.dotnet/dotnet build tools/EntropicBrewChecks -c Release -v quiet -nologo -m:1
STS2_SRC='/home/wst/Desktop/MCC/Slay the Spire 2/src/Core' taskset -c 0 nice -n 10 \
  timeout --signal=KILL 10 /home/wst/.dotnet/dotnet \
  tools/EntropicBrewChecks/bin/Release/net9.0/EntropicBrewChecks.dll
```

正版 DLL **TestMode=false**，实际执行 `OnUseWrapper` 和原生 `EntropicBrew.OnUse`。
四个战斗外探针结果：

| Sozu | 预留 Fire Potion | RNG 抽取 | 使用后槽位 |
|---|---|---:|---|
| 无 | 无 | 4 | POISON_POTION, WEAK_POTION |
| 无 | 有 | 2 | POISON_POTION, FIRE_POTION |
| 有 | 无 | 2 | null, null |
| 有 | 有 | 2 | null, FIRE_POTION |

探针第一次运行缺少 `Launch()` 初始化 local NetId，抛 `Nullable object must have a value`；
补正常启动后四例全过，没有修改游戏规则或增加 Harmony 玩法补丁。

```bash
STS2_SRC='/home/wst/Desktop/MCC/Slay the Spire 2/src/Core' taskset -c 0 nice -n 10 \
  .venv/bin/python tools/nosl/oracle_replay.py \
  --trajectories artifacts/nosl-native-hang/NT100652020008441.jsonl \
  --out artifacts/nosl-native-hang/oracle --jobs 1 --timeout 30
```

原始结果：`prefix:1`、`decisions_matched:250`，无分歧；包括使用熵之酿后的决策。
`script_reason` 为 `the rows end inside a card selection`。这个既有整局驱动默认使用
历史 TestMode/正式分支补丁设置；不能把它写成全轨迹 TestMode=false 认证。
决定本次退出条件与 RNG 的独立探针已显式关闭 TestMode。

## 第二次贪心挂起与合入门禁

以 `checkpoint-00489.pt`、原始 CUDA bfloat16/argmax 路径、修复前 native 库，
逐局尝试了 **NQGRDY00000–00435，共 436 局，全部死亡返回，未重现 hang**。
仅一个 native worker、一个活动局，3 秒通信 timeout，保存了全部已完成局的动作历史。
为串行执行完整门禁停止扫描，余下 1,564 个种子未完成；输出见
`artifacts/nosl-native-hang/greedy-before/`，修复前库为同目录上级的
`libsts2nosl_c-before.so`。
与旧评估 4 worker/500 局的 batch 形状不同，浮点近似并列动作可能变轨；
原始第二次失败没有日志，不能据此确认或排除它也是 Entropic Brew 导致。

完整门禁以 `.venv` 提供 Python、构建并发 2，执行：

```bash
PATH="$PWD/.venv/bin:$PATH" STS2_BUILD_JOBS=2 \
  STS2_SRC='/home/wst/Desktop/MCC/Slay the Spire 2/src/Core' \
  taskset -c 0-5 nice -n 10 scripts/check.sh
```

第一次在 60/60 CTest、目录检查和 smoke 通过后，因同树 Trial 移植使源码清单过期而停止；
按当前源码刷新清单后，第二次 **退出码 0，末行 `all checks passed`**。
覆盖 60/60 CTest、L2 200 局、Trial 专项、全部 L3，以及 L4 **111/111 局、
25,143/25,143 个决策一致**。日志为 `artifacts/trial-check-20261005-2.log`；
第一次日志副本为 `artifacts/nosl-native-hang/gate-first.log`。
这次完整门禁验证包含本修复与 Trial 修复的组合版本；本修复提交 `03673ec`，
Trial 独立提交 `ebb1805`，全绿后组合分支已快进合入 main。

`CLAUDE.md` 的删除仍未暂存、未提交；Trial 修改由对方独立提交。
本任务仅暂存显式路径，未推送。
