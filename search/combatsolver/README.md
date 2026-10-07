# CombatSolver Env consumer

独立 C++ 搜索库 `sts2combatsolver`，来源为 CombatSolver 0.47.3 / `d7b4620`。
按用户要求，以实战效果为验收，不要求复制上游全部搜索组织。
许可与对应源码见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

```cpp
#include "search/combatsolver/solver.h"

sts2::combatsolver::CombatSearchConfig config;
config.Profile.BeamWidth = 30;
config.Profile.MaxExpandedNodes = 10000;
config.Profile.SoftTimeBudgetMilliseconds = 10000;
auto result = sts2::combatsolver::CombatSolver::Solve(env, config);
// result.Actions 使用既有 Env Action；根 env 保持不变。
```

搜索与 Env 在同一进程调用，分离为两个库，没有逐节点 JSON、IPC 或进程启动。
完整快照只用于输入和诊断。客户端要保存跨进程输入时，可使用现有
`Env::ExportPortableSnapshotJson()`；它仍要求相同构建。

2026-10-02 [性能优化](../../docs/reports/2026-10-02-combatsolver-performance.md)：
同21场、同预算串行复测，87.63秒降至46.58秒；21条路线、质量和最终Env hash完全相同。
上游该轮为34.82秒，原生慢约1.34倍。额外15个A10精英/Boss起始牌组探针也保持
相同部分路线；它们没有获胜，不作为中后期强度证明。

同日[继续优化与多线程](../../docs/reports/2026-10-02-combatsolver-search-efficiency.md)记录
重复集中成员省略、成员级并行、21场和受控非起始牌组的效果/CPU/RSS。

实现包含跨回合 Beam、牌/目标/药水/嵌套选择展开、原版 EndTurn 预测、
集中击杀与扩大宽度的共享预算组合、无药基线和 Smart/Disabled/RequireAtLeastOne
政策、胜负与安全终局停点。返回前完整重放路线并验证 hash。

请求内以完整Env hash缓存评分事实和真实EndTurn结果；动作后继缓存仅保存已完成的
普通Combat状态，复用时深复制，选择/胜负停点继续保留原版前缀。
已计算且没有选择分支的EndTurn也可用于真实搜索边。同完整状态且五项路径成本相同的转置可在
评分前丢弃；保路、评分权重、搜索宽度、节点预算及返回前无缓存重放保持原样。

`EvaluationCacheEntries`默认100000；`TransitionCacheEntries`默认8192，
`TransitionCacheBytes`默认512MiB，限制估算的原生payload，包含池块和嵌套列表。
后者不是进程RSS硬上限。代表场景峰值约350MiB，状态缓存有明确的内存成本。
设`TransitionCacheEntries=0`或`TransitionCacheBytes=0`可关闭拥有型后继缓存；
`EvaluationCacheEntries=0`关闭值缓存。缓存属于单次请求，结束时释放；耗时计入释放成本。
`ReuseEndTurnForecast`、`PruneBeforeScoring`可关闭以做差分。

评分与多样性保路采用上游权重及简化特征。近似去重保留实际执行的物理牌实例；
正确性转置仍用完整 Env hash 和上游六维支配政策。
搜索预算耗尽时可能返回可执行的部分路线；以 `Quality.Won` 判断是否找到完整胜利。
默认在下一轮宽度未改善已有胜利的 HP/资源质量时提前结束；
`StopAfterStableWidth=false` 可在预算内继续扩大搜索。
`SkipEquivalentFocusPasses=true`省去已完成普通成员所覆盖的相同集中搜索；逐个已展开
状态检查目标是否会改变动作过滤，不能仅凭根的敌人数判断。关闭可做差分。

`ThreadCount`默认1。设为2或更高后，普通成员先行，独立的集中目标成员并行；
每批预留不重叠的节点配额，所有成员使用同一请求起点/时间上限，无药与用药两阶段
仍共享总预算。汇总按固定目标顺序进行。大状态缓存仍为单份：工作线程只能读它并
深复制后继；各自的值缓存按任务数分配，全部任务结束后回收精确事实到主缓存。
取消回调串行调用，工作线程结束后才能返回。此版本没有逐节点并行展开；
有效集中目标数限制并行度，单敌人已覆盖的集中成员会直接省略。
`PeakConcurrentPasses`记录一批同时提交的任务数，CPU利用率以外部测量为准。
`MaxTurns`/`MaxActionsPerTurn` 及选择分支上限也会限制搜索。
部分路线适合客户端执行动作后重新搜索，不能当作完整战斗解。

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target combatsolver_search combatsolver_policy_probe
build/combatsolver_search --seed CSPR0BE1 --character IRONCLAD \
  --encounter CULTISTS_NORMAL --beam 30 --nodes 10000 --budget-ms 10000 \
  --out result.json
```

CLI 支持 `--snapshot`（公共导入格式）、`--export-root`（local v4诊断）、
`--potion-policy`、`--ascension`、`--act-index`、`--max-turns`、`--threads`。
控制场景可用`--deck CARD[+N],...`、`--relics ID,...`、`--potions ID,...`在战斗前通过
游戏命令配置。`--prepare-action kind,index,target`可重复执行并保留SafeOutcome选择前缀；
`--export-only 1 --export-root-format portable --export-root root.json`仅生成跨进程根。
默认local格式只用于同进程诊断。这些是合成负载，不代表自然通关牌组。
诊断 `--replay-plan` 为每行 `PlayCard CARD_ID occurrence targetCombatId`、
`EndTurn` 或 `Answer index`，只用于核对动作路线。

政策差分直接编译固定上游 C# 源文件，不使用 sts2-ai 作为真值：

```bash
python3 tools/combatsolver/check_policy.py
python3 tools/combatsolver/compare_search.py --upstream /path/to/CombatSolver \
  --native build/combatsolver_search --baseline-native /path/to/previous/native \
  --out /path/to/evidence
python3 tools/combatsolver/compare_native.py --baseline /path/to/previous/native \
  --native build/combatsolver_search --out /path/to/extra-evidence
ctest --test-dir build -R '^combatsolver_' --output-on-failure
python3 tools/combatsolver/compare_threads.py --native build/combatsolver_search \
  --thread-counts 1,2,4,8 --out /path/to/thread-evidence
python3 tools/combatsolver/compare_threads.py --native build/combatsolver_search \
  --baseline /path/to/compatible-baseline --cases tools/combatsolver/efficiency-fixtures.json \
  --thread-counts 1,2,4 --out /path/to/loadout-evidence
```

`compare_search.py` 对五个角色、两个初始遭遇和固定种子运行21场。
逐场检查 HP、block、energy、stars、turn、牌堆顺序、敌人身份/HP/block/move 和
九条战斗 RNG；不声称整个 RunState 与上游相同，也不把该小样本推广至全部内容。
预设、源码或预算变化时应重新跑上游；`--reuse-upstream` 只供同一配置开发迭代。
`--profile 1`启用阶段计时；各计时按作用域记录，未覆盖全部管理开销。
CLI还支持`--cache-entries`、`--transition-cache-entries`、`--transition-cache-mib`、
`--reuse-end-turn`、`--prune-before-scoring`和`--skip-equivalent-focus`（后三者为0/1）。
`pass_details`记录每轮宽度、目标、药水模式、完成状态、展开、实际预测、耗时与质量。
各并行Pass的时间/阶段时间可能重叠，不可相加当作请求墙钟耗时。
`transitions`为分支请求数，包含缓存命中；`native_predictions`才是实际执行的Env预测，
返回前实际路线重放另计`route_replay_transitions`；值缓存命中的评分不产生分支请求。

支持普通 Combat、携带 SafeOutcome 政策的 Action Choice 和冻结 SafeOutcome 根。
普通 `Step` 留下的无停点 Choice，以及 EnemyPhaseDone 根明确拒绝；可从正常 Combat
根启动搜索，后续选择由消费者保留既有前缀。无需修改任何 Env API。

未覆盖全部上游实验排序、成长/盗取/复杂遗物目标与完整设置估值。
死救恢复 HP、单动作同时伤害与回血的毛战损目前为近似值；最终幕 Boss 使用保守 HP
估值。默认关闭的未支持实验配置会明确报错。搜索是全信息消费者；普通智能体观测的
信息边界保持原样。本轮不包含 nosl、训练或部署。
