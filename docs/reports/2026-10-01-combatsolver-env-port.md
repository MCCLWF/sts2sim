# CombatSolver → Env 实用首版

## 本批结论

按用户最新要求，以战斗效果接近为验收，不逐项复制上游搜索组织。已实现可运行的
原生跨回合搜索库和 CLI；保留原有 Env API，未修改 game/engine/env/sdk/python。
同进程分库调用，节点间没有 JSON/IPC。nosl、训练、游戏内部署不属于本轮。

五角色、两个初始遭遇、21场固定对照中，两端全部获胜。总战损原生349 HP、
上游350 HP，平均差值−0.0476 HP；最差单场原生多损失15 HP。
原生搜索总耗时87.666秒，上游35.278秒，约2.49倍。
这证明本批开局样本整体效果接近；不证明所有战斗质量相同或全局最优。

## 来源与前置条件

- 分支 `feat/combatsolver-env-2026-10-01`，基线 `5bd4da1`。
- 重新克隆 CombatSolver 最新 `d7b46203593beb51f792628a2587b04f0850d1c2` / 0.47.3；
  submodule固定此版本，保留MIT许可。完整源项目及其OfflineSearchHarness已构建成功，
  `CopyModOnBuild=false`，没有写入游戏部署目录。
- 重新克隆 sts2-ai 最新 `<私有提交>`，只作比较项目；
  本实现不复制其搜索或规则。
- 正版游戏v0.111.0/.NET9可用；Env基线已有owned clone、typed Action停点、选择前缀
  和公共快照。旧归档及主仓用户删除的文件保持原样。
- 上游搜索优化与架构边界技能已阅读；本批按用户最新授权采用简化组织，不要求全部机制逐文件复刻。

## 实现与职责

`search/combatsolver/policy.*` 直接翻译上游纯值政策；权重由生成脚本核对源SHA。
包括HP/遗物回血/死救纯算术、用药阈值/琥珀/替换信用、路线与显示排序、盗取纯比较、
预算窗口和六维转置标签前沿。模型依赖没有混入Env层。

`env_state.*` 拥有独立Env分支，以公共Action operation的SafeOutcome停点保留战中终局。
Answer沿原前缀续用；Resume仍执行普通结算。正HP的pending_loss仍判负，不能按回血误判。
调用方根、兄弟分支和终局状态不互相修改。

`solver.*` 为简化逐回合Beam，展开牌/目标/药水/选择，实际执行EndTurn作威胁预测，
使用HP、易伤、集中击杀、持续buff、延迟伤害和活牌杂质等特征。
正确性转置用完整Env hash；近似多样性桶和重复牌动作剪枝只改变搜索保路，
每条保留路线仍执行实际物理牌实例。无药基线、用药搜索、集中击杀及扩大宽度共享
同一请求节点/时间预算。下一轮宽度未改善已有胜利质量时可提前结束。
返回前从根重放所有动作并验证最终hash；部分路线与完整胜利通过`Quality.Won`区分。

客户端接口与完整命令见[消费者说明](../../search/combatsolver/README.md)。

## 验收记录

### 纯值政策差分

```bash
cmake --build build --target combatsolver_policy_probe
python3 tools/combatsolver/check_policy.py
```

直接编译固定上游未修改C#源文件，结果34082例、0差异。覆盖路线14104、显示2500、
完整胜利64、死救1296、Boss政策1272、遗物回血1218、用药3007、预算216、转置10405。
输入SHA256 `8a740239197205248e9c7a7af54052272da07ad8a830f2d0eb012d5a6f60c94f`。
原件：工作树 `.local/combatsolver-policy-oracle/{report.json,cases.txt,expected.txt,actual.txt}`。
它只证明纯计算/排序一致，不代替字段映射或整搜质量验收。

### 新消费者合同

```bash
ctest --test-dir build -R '^combatsolver_' --output-on-failure
```

2/2通过。适配测试比较普通Env执行的42个动作、10个选择回答，包括五角色根分支、
战前/战后HP区分、正HP pending-loss、选牌、回合开始选择和非法分支纯度。
搜索测试验证实际路线合法/完整重放、已终局根、Disabled/Smart/Required反事实、
带停点Choice根和未支持实验配置的显式拒绝。

### 搜索效果与耗时

```bash
python3 tools/combatsolver/compare_search.py --upstream /path/to/fresh/CombatSolver \
  --native build/combatsolver_search --beam 30 --nodes 10000 --budget-ms 10000 \
  --out /path/to/outcome-21
```

本次开发迭代复用完全相同配置下已保存的上游结果，最后原生二进制SHA256为
`3ec4b0e3baa44d5d053cdffe73ef6ade6876e332b587bc0770d3f8298354facb`。
21/21根检查通过：HP/block/energy/stars/turn、牌堆顺序、敌人身份/HP/block/move和
九条战斗RNG。未把不同根或失败样本混入质量均值。

最初发现`CSPROBE1`经过Env种子规范化变为`CSPR0BE1`，上游工具直接用原字符串。
统一后根检查通过；不能用修正规范化之前的不匹配对照作性能或质量结论。
上游`CSPR0BE1`路线已在Env实际重放，最终HP同为37；该路线原件另存。

| 角色 | 遭遇 | 种子 | 原生战损 | 上游战损 |
| --- | --- | --- | ---: | ---: |
| Ironclad | Cultists | CSPR0BE1 | 43 | 43 |
| Ironclad | Cultists | BMS0001 / BMS0002 | 18 / 38 | 18 / 50 |
| Silent | Cultists | BMS0001 / BMS0002 | 31 / 29 | 31 / 19 |
| Regent | Cultists | BMS0001 / BMS0002 | 4 / 38 | 9 / 33 |
| Necrobinder | Cultists | BMS0001 / BMS0002 | 43 / 38 | 28 / 38 |
| Defect | Cultists | BMS0001 / BMS0002 | 29 / 24 | 40 / 24 |
| Ironclad | Fuzzy Wurm Crawler Weak | BMS0001 / BMS0002 | 1 / 6 | 4 / 6 |
| Silent | Fuzzy Wurm Crawler Weak | BMS0001 / BMS0002 | 4 / 0 | 4 / 0 |
| Regent | Fuzzy Wurm Crawler Weak | BMS0001 / BMS0002 | 1 / 1 | 1 / 1 |
| Necrobinder | Fuzzy Wurm Crawler Weak | BMS0001 / BMS0002 | 0 / 0 | 0 / 0 |
| Defect | Fuzzy Wurm Crawler Weak | BMS0001 / BMS0002 | 1 / 0 | 1 / 0 |

原生展开125892节点，上游39354；原生818893次转换含实际EndTurn预测，上游126165次
转换采用其预测器与工作计数口径。原生另剪掉68687个重复动作，处理12264个选择回答。
不能把不同工作量、不同转换口径的整搜耗时解释成Env单步速度或分库通信成本。
当前性能差距主要是保路/组合组织与大量深复制、完整状态hash和实际威胁展开；没有IPC。

早期纯单Beam一度丢失胜利；集中击杀/扩大宽度组合后21场全部获胜，总耗时131.954秒。
重复动作剪枝降到112.250秒，质量稳定停止降到87.666秒，最终21场HP总损失349对350。
完整数据保留于任务 `artifacts/legacy/combatsolver-port-2026-10-01/outcome-21/report.json`，
每场包含源输出、根快照、合法路线和指标；早期失败/迭代原件未覆盖删除。

### 仓库门禁与边界

```bash
git diff --exit-code 5bd4da1 -- game engine env sdk python
STS2_SRC=/path/to/game/src/Core scripts/check.sh
```

边界diff为空。完整门禁退出0，`all checks passed`：57/57 CTest，L2新局200/200，
L3为81场完整战斗和2个有界探针、1629/1629状态，L4为111局、25143/25143决策。
完整日志在任务 `artifacts/legacy/combatsolver-port-2026-10-01/check-full.log`。
新搜索是消费者，这些既有规则门禁不能扩大21场搜索质量声明。

## 限制

- 只对这21场初始牌组/两遭遇及有限消费者合同作效果声明，尚无中后期复杂根、大规模
  Boss/药水/遗物组合的质量矩阵。最差单场多损失15 HP，不能宣称每场都不差。
- 未实现全部上游成长、盗取、遗物目标、复杂设置估值、并行调度和跨请求续搜缓存。
  不支持的实验排序明确拒绝。
- 死救恢复HP与单动作同时伤害/回血的毛战损使用近似；最终幕Boss保守估值。
  消费者本身不修改任何原版效果。
- legacy无SafeOutcome的Choice与EnemyPhaseDone根明确拒绝。普通Combat根启动搜索后，
  内部动作和嵌套选择正常保留公共前缀/停点。
- 全信息搜索，普通agent观测信息边界不变；本批不包括训练/无模拟AI或游戏部署。

## 空间清理

用户授权清理较早`.local`数据。仅删除9月30日mass-seeds的deep-v1/deep-v2/natural-v1/
natural-v2中`result.status == identical`作业的大型stdout/jsonl及压缩副本。
共23594文件、6026984851逻辑字节；释放约5.66GiB占用空间。
可用空间从1298690048增至7358533632字节（随后构建会再次占用少量空间）。
保留失败证据、输入、种子、manifest、summary、结果元数据、工作树和旧性能结果。
删除清单在任务 `artifacts/legacy/storage-cleanup-2026-10-01/{plan.json,result.json}`。
