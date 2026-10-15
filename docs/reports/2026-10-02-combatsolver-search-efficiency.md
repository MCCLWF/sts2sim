# CombatSolver 搜索效率与多线程

用户将目标更新为继续优化并添加多线程支持；接受搜索组织不同、效果接近，暂不开发
nosl/训练。基线为 main `76abc78`。本轮仅修改搜索消费者、CLI、基准和文档，
game/engine/Env/ABI/Python API规则未改；CMake为搜索库传播已有Threads依赖。

## 为什么节点很多而效果接近

旧21场展开125892、97个Pass。每个宽度/集中目标从根重新搜索，计数包含不同成员再次
经过的状态。相近的权重和Beam排序常保留相近的路线，增加宽度并不保证改善战损。
例如Ironclad/Cultists/CSPR0BE1的30宽普通/目标1/目标2分别展开862/820/834、战损55/43/55；
60宽为1669/1604/1403，完整候选仍为55/43/55。Regent/BMS0001则确实在后续集中成员
把战损11降到4，所以不能普遍省掉后续搜索。

原生与上游的展开口径、搜索组织不同，125892/39354不能解释成等量独立新局面。
对手是固定的CombatSolver 0.47.3 / `d7b4620`，不是sts2-ai搜索器；sts2-ai的Env基准是另一批。
之前关于第二道展开准入的推测也得到检验：3个代表场景的完整状态父节点去重命中均为0，
没有证明它能解决当前瓶颈，试验代码已撤销。

## 最终实现

- 省去可证明重复的集中成员：同宽度、同药水阶段，普通成员完成且所有展开父节点的
  合法动作都不会被该目标过滤时，重复集中成员省略。检查包含根之后出现的敌人；
  普通成员被预算中断时继续保留集中成员。仅省去重搜，不改原版执行或完整状态转置键。
- `CombatSearchConfig::ThreadCount`默认1，CLI新增`--threads`。普通成员先建立基线，
  集中目标成员按请求线程上限分批并行。节点预留配额不重叠，未用完的额度进入后续批次；
  请求时间起点相同，无药/用药两阶段仍共用总预算。结果按目标顺序汇总，全部线程结束后返回。
- 线程只读主请求的大状态缓存，使用后继的独立深复制。工作线程不再各建立一份大状态
  缓存；评分/预测小值缓存按批次任务数分配，全部线程join后回收到主缓存。取消回调串行执行。
- 每轮记录宽度、目标、药水阶段、是否完成、展开、实际预测、耗时和候选质量。
  `PeakConcurrentPasses`是同时提交的成员数，不能当作实际CPU核数。
- CLI增加受控牌组/遗物/药水及SafeOutcome准备动作、portable根导出；比较工具记录
  GNU time CPU/RSS，按场景交替执行顺序，进程串行运行。默认预算和宽度未缩小。

这是成员级并行。有效集中目标数限制并行度；单敌人已覆盖成员直接省略。
没有实现一个Beam内部的逐节点并行。Env与搜索仍在同进程，无逐节点JSON/IPC。

## 21场同预算测量

每场初始Beam30、总节点上限10000、总软时间10000ms、Smart、A0/Act0。
使用旧21个输入，所有新旧根的完整原生hash相同。此次每配置一轮，不能把小差异推广为
普遍速度保证。所有配置均21/21获胜，总战损349；21条动作、质量及最终hash与旧版相同。

| 配置 | 展开总数 | 实际Env预测 | 搜索墙钟合计(s) | 进程CPU合计(s) | 平均CPU核数 | RSS峰值中位/最大(MiB) |
|---|---:|---:|---:|---:|---:|---:|
| 冻结main旧版 | 125892 | 436681 | 46.514 | 47.65 | 0.998 | 350.61 / 411.02 |
| 新版1线程 | 116565 | 430032 | 45.512 | 46.68 | 0.998 | 355.18 / 411.26 |
| 新版2线程 | 121103 | 472287 | 42.005 | 52.67 | 1.215 | 335.83 / 514.72 |
| 新版4线程 | 121103 | 472287 | 41.705 | 52.42 | 1.217 | 335.16 / 514.40 |
| 新版8线程 | 121103 | 472287 | 42.019 | 52.77 | 1.218 | 335.20 / 515.05 |

1线程省掉7次Pass，97→90，展开减少7.41%；耗时仅改善约2.2%，因为重复工作大多命中缓存。
2线程比旧版墙钟少约9.7%，CPU总量增加约10.5%。并行成员的预算分配和缓存可见时机
不同，实际预测会增加；多线程没有使搜索自动更聪明。该组最多两个集中目标，因此4/8
没有增加实际任务并行度。RSS最大值增加约25%，中位值降低不能掩盖这个峰值成本。
512MiB只限制状态缓存的估计payload，仍不是整个进程RSS上限。

旧固定上游结果为34.820s、21/21胜、战损350，保留作历史参考。此次核心对比是新旧原生
同轮交替测量，没有重新测量C#，不能把不同轮次相除宣称最新严格跨项目速度。

## 扩展场景

`tools/combatsolver/efficiency-fixtures.json`包含19个受控输入：五角色各两组12张含升级牌
及遗物/药水的A10战斗，三组SafeOutcome Survivor选择根，三种Ironclad药水政策，
Slimes/Mytes/Scrolls多目标场景。这些是受控合成负载，不是自然run采集或中后期胜率认证。
18个接受路线的输入全部保留原版获胜：15胜、3个选择根部分路线。1线程与冻结搜索的
动作和质量全相同；并行仅Slimes更换路线/评分，HP、药水和结束回合仍相同。

前17个接受场景：旧版37.797s，新1/2/4线程38.234/34.458/34.275s，均战损34。
这是另一批负载，不能与21场耗时相加形成跨项目结论。1线程并非每批都更快。

Scrolls四目标单场旧版/新1/2/4/8线程为4.535/4.517/3.363/2.997/2.787s，
均5000展开、同胜利动作、同零战损。4线程实际提交4个集中任务，平均CPU约1.96核，
峰值RSS427.68MiB（旧版394.78MiB）。8线程仍只有4个任务；单轮4与8的耗时差异
不能解释为8核扩展。该场体现成员级并行收益，也保留普通成员和其它串行工作。

Ironclad RequireAtLeastOne输入在5000总节点内拒绝：
`CombatSolver potion policy unsatisfied within the request budget`。
冻结旧版和新1/2/4/8均同样拒绝；没有用放宽政策或增加预算掩盖它。单元合同另有能实际
完成Required路线的并行用例，覆盖总预算、重复运行次序、取消及状态独立。

原15个A10精英/Boss起始牌组探针仍15/15动作/质量/hash/展开一致，全部部分路线，
0胜。新16.306s、旧15.550s；不宣称这一组提速，不当作复杂完整战斗强度证明。

## 试验、原始证据和复现

任务产物目录相对MCC根：`artifacts/legacy/combatsolver-efficiency-2026-10-02/`。
`expansion-probes`为零命中展开去重试验；`focus-skip-21`为首次单线程相同路线对照；
`retained-probes`/`retained-10k`改为只缓存保留节点，预测次数反增，已撤销。
`threads-21`为回收线程值缓存前的并行试验；`final-threads-21`为最终搜索测量，
`portable-loadouts`与`additional`为扩展结果；`extra-15`为起始牌组部分路线探针。

初次汇总因旧版缺少新指标失败；原始21场测量完整，修复可选指标后用
`--summarize-existing`重建汇总，没有重跑或替换数据。扩展脚本初次误用local快照，
全部被runtime_id检查拒绝；随后显式portable导出重跑。Required初次记为错误后单独
复核全部配置的一致拒绝，并登记为显式预期负例。失败日志保留。

冻结旧二进制SHA256 `a8275bc3e282e5c98fd2a0091edac5ecc5f94bc8134cdac59affda46d0a85841`。
21场测量二进制SHA256 `2c0ea60a9e9f21004227c2eebdbf7971eea68dd980af631c4b8083aa385a31c4`。
其后仅CLI增加portable导出选项，搜索源码未变；扩展CLI SHA256
`e144123c4a9f88876ddcafc8519654a10547f0450f109a60db095488ad9b62f5`。
跨进程扩展的旧求解器源码固定为76abc78，链接当前同一组Env静态库，以满足CMake变化后
portable构建身份要求；二进制SHA256 `a05a88f9aa618982807db0be0d3f33ae5bb94a4eff3e33d5f141507a434d1479`。
最终solver.cpp SHA256 `8e8f17db628b6127357c58e3c67db1e7a14041100a834d7c154ff5f45a967fc0`。

```bash
python3 tools/combatsolver/compare_threads.py --native build/combatsolver_search \
  --baseline /path/to/frozen/native --thread-counts 1,2,4,8 --out /path/to/21-evidence
python3 tools/combatsolver/compare_threads.py --native build/combatsolver_search \
  --baseline /path/to/compatible/native --cases tools/combatsolver/efficiency-fixtures.json \
  --thread-counts 1,2,4,8 --out /path/to/loadout-evidence
python3 tools/combatsolver/compare_native.py --baseline /path/to/compatible/native \
  --native build/combatsolver_search --out /path/to/15-evidence
python3 tools/combatsolver/check_policy.py
ctest --test-dir build -R '^combatsolver_' --output-on-failure
STS2_SRC=/path/to/v0.111.0/src/Core scripts/check.sh
```

纯政策差分34082例、0分歧；线程合同通过。完整门禁第一次未指定隔离工作树的C#源目录，
生成器报告Abrasive升级覆盖缺失；设置真实v0.111.0 `STS2_SRC`后重跑，不改任何卡牌逻辑。
完整门禁与主树合入结果见下方最终验收记录。

## 最终验收

`STS2_SRC=/path/to/v0.111.0/src/Core scripts/check.sh` exit0，57CTest全部通过，
L2 200开局，L3 81完整战斗+2探针/1629状态，L4 111路线/25143决策，全部无分歧；末行`all checks passed`。纯政策34082例0分歧，Python工具语法检查
及`git diff --check`通过。没有修改生成器判据、卡牌覆盖或oracle来通过门禁。

实现提交`72105b3`已fast-forward合回main。主树完整重建（包括ABI/CLI/测试）和57CTest
全部通过；新进程Ironclad/Cultists/CSPR0BE1、Beam30/10000节点/10000ms、2线程与本轮
原始测量的根hash、胜利动作、质量、终点hash、7411展开完全相同。
本轮临时分支收尾删除，附属工作树detach；主树原有`CLAUDE.md`删除保留。
