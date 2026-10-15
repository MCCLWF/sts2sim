# CombatSolver 与 Env 性能优化

## 结果

同一机器、相同输入/Beam30/10000节点/10000ms、单线程串行对照，原21场搜索从
87.632秒降至46.576秒，耗时减少46.85%，约1.88倍速度。上游本次34.820秒，
当前约1.34倍耗时；未宣称达到或超过上游。

21/21同根检查通过、21/21均胜；优化版与首版的动作序列、质量、最终完整Env hash
全部一致。总战损仍349 HP，上游350 HP，最差单场仍多15 HP。
展开节点仍125892，宽度/预算/评分/保路没有缩小，提速来自复用和存储优化。

额外五角色 × Bygone Effigy / Queen / Kaiser Crab、A10、ActIndex2、起始牌组，
Beam16/3000节点/60000ms/12回合的15个探针，从35.636秒降至15.756秒。
15/15根、动作、质量、完整最终hash及展开数相同。双方均未找到胜利；这是
复杂展开与部分路线的优化回归，不是自然中后期牌组或Boss胜率证明。

完整门禁、6500张牌API/SDK合同及真实旧容量失败重放全部通过。

## 实现与边界

- 不可变搜索分支只计算一次完整Env hash；Fork仍深复制并继承该值。
- 请求内缓存Board值和真实EndTurn预测。保留真实规则执行，未改成近似意图评分。
- 普通Combat动作后继缓存8192条/512MiB估算payload；只保存完整拥有型状态，
  命中时深复制。Choice/Boundary不进入状态缓存，其身份与值缓存仍绑定完整Env hash。
- 节点已计算的无选择EndTurn结果可直接深复制用于真实EndTurn边；
  返回路线从根无缓存重放，完整hash必须一致。
- 转置表中完整状态与五项路径成本相同时，在昂贵评分前剪掉重复候选。
  上游六维支配政策、最终排序及Smart/Disabled/Required用药反事实保持原样。
- 手中牌的动作去重键与Board一起采集，移除每次展开的额外检查副本。
- 私有源状态已验证覆盖，后继用公共Clone+Execute/Step并验证新内容一次；
  公共source_hash、合法性、执行前缀仍验证，不增加绕过公共API的执行入口。
- Env Clone在无pending/Choice/Boundary时不保留上一已完成动作的无用base。
  需要重放的状态完整保留不可变base，新的顶层动作仍保存新快照。
- Pool改为最多8槽的地址稳定拥有型块。逻辑extent/cursor/最低空闲复用顺序不变，
  未用块尾不参与hash、快照或分配。复制只保留live值，包含嵌套列表的深复制。
  移动保留对象地址，移出源成为可复用的空池。没有原版新增的容量上限。

未修改`game/`、SDK/Python或Env接口签名。内部存储头的来源digest和native build
fingerprint已更新，portable仍按现有合同要求同构建，旧构建产物不能冒充新构建输入。
nosl/训练/部署不属于本轮，普通智能体信息边界保持原样。

缓存释放计入搜索耗时。代表场景峰值358520 KiB（约350MiB），默认缓存以内存换复用，
可以下调条数或字节预算，也可以关闭。字节数是原生payload估算，不是RSS硬上限。

## 性能证据

基线`b47cf9c`二进制SHA256：
`3ec4b0e3baa44d5d053cdffe73ef6ade6876e332b587bc0770d3f8298354facb`。
最终二进制SHA256：
`294fd9e7acdd61dcf8b58ddf42eac4b02c45dfb9f7578af82fec0c932b3c5149`。
上游固定`d7b46203593beb51f792628a2587b04f0850d1c2` / 0.47.3。
上游在本轮第一组重新运行，最后组复用其同配置输出并逐场核对harness输入。
原生新旧每组串行运行，交替先后顺序，不和构建/测试并跑。

```bash
python3 tools/combatsolver/compare_search.py --upstream /path/to/CombatSolver \
  --native /path/to/final --baseline-native /path/to/b47cf9c --out /path/to/evidence
python3 tools/combatsolver/compare_native.py --baseline /path/to/b47cf9c \
  --native /path/to/final --out /path/to/extra-evidence
```

原件在任务目录`artifacts/legacy/combatsolver-port-2026-10-01/`：
`performance-21/`为第一轮，`performance-final-21/`为中间轮，
`performance-blocks-21/report.json`为最终21场，`performance-extra-15/report.json`为额外15场。
每场保留输入、两端输出、原生完整路线及上游根/路线/质量数据。

| 工作量 | 首版 | 优化版 |
|---|---:|---:|
| 展开节点 | 125892 | 125892 |
| 分支请求 | 818893 | 647638 |
| 实际Env预测（不含返回重放） | 818893 | 436681 |
| 结束回合预测分支请求 | 375249 | 203994 |
| 评分前转置剪枝 | 0 | 13219 |
| 无缓存返回路线重放 | 410 | 410 |

最终请求内命中：Board180040、forecast158036、后继152630；
直接复用已计算EndTurn58327次。终局评分没有EndTurn预测。
上游的节点/转换计数口径不同，不用此表推导跨语言单步倍率。

代表`CSPR0BE1`阶段计时：旧总5.300秒，动作后继2.166秒、结束回合预测2.537秒、
Board0.308秒。主要成本在预测，不是额外Board复制。
`perf record`被宿主`perf_event_paranoid=4`拒绝；未修改系统权限，采用可关闭的阶段计时。
阶段计时是作用域累计，没有提供每个底层函数的CPU占比。

普通Env固定Cultists/DUAL001、预热32、9×1000次（含销毁）：

| 指标 | 首版均值/中位数 μs | 优化均值/中位数 μs |
|---|---:|---:|
| Clone | 5.602 / 5.759 | 4.548 / 4.578 |
| Hash | 13.277 / 10.300 | 15.491 / 10.456 |
| Predict EndTurn | 73.112 / 73.173 | 69.487 / 69.465 |

Hash没有优化，均值波动大，中位数基本相同；不宣称hash加速。
GameState本体12536→12152字节，不包含池块/列表堆存储。
根hash8784066943715545080、后继hash17067682832684709515、预览trace数量与摘要一致。
原件`env-api-baseline.csv`、`env-api-blocks.csv`；当前普通Clone仍未达到历史1μs目标。

## 正确性验证

- `check_policy.py`：直接编译未改上游政策，34082例、0差异，输入SHA保持
  `8a740239197205248e9c7a7af54052272da07ad8a830f2d0eb012d5a6f60c94f`。
- 搜索合同增加五角色A10缓存开关差分、条数/字节预算饱和、EndTurn复用与提前剪枝开关、
  Choice根与三种用药反事实；节点数/完整路线/hash/质量必须相同，返回动作逐步检查合法。
- 动态存储合同：70000引用、嵌套拥有型克隆、恢复、跨块trim/复用、移动地址稳定及空源复用。
- 旧deque与新块池的10000步随机alloc/free/trim/clone/move/restore，逐步逻辑摘要完全相同。
  原件`pool-differential.cpp`、`pool-baseline.h`、`pool-diff-{old,new}.txt`。
- 普通Env clone后续动作/EndTurn完整快照与hash对照，加上原有选择、胜负边界及portable合同。
- 完整门禁初次因隔离工作树缺少默认相邻反编译目录而失败；随后设置实际`STS2_SRC`重跑。
  此失败没有修改或放宽源覆盖检查。

完整门禁：`STS2_SRC=/path/to/v0.111.0/src/Core scripts/check.sh`，exit0。
57/57 CTest、L2 200/200；L3 81完整战斗+2有界探针、1629/1629状态；
L4 111局、25143/25143决策一致。原件`performance-check-full-v2.log`。

容量专项命令：

```bash
cmake --build build --target env_api_capacity_fixture
python3 tests/api/dynamic_capacity.py --output-dir /path/to/capacity-6500
dotnet run --project sdk/dotnet/Sts2Env.ContractTests --configuration Release -- \
  /path/to/libsts2env_c.so /path/to/capacity-6500/large-1.portable.json
python3 tools/differential/batch.py replay --out /path/to/capacity-real \
  --traces /path/to/frozen/oracle.stdout.jsonl --jobs 1
```

6500张牌普通/挂起状态均qualified：快照6058774/12543897字节，
挂起6500动作/6500 mask/32588 observation，2次剩余回答完成，克隆/恢复/预测独立。
损坏INT32_MAX容量元数据按宿主预算拒绝；.NET同一portable输入6500动作前缀通过。
原件`performance-capacity-6500/summary.json`、`capacity-final.log`、`capacity-dotnet-final.log`。
历史Regent容量失败`FEFE3B6F36DC37D`严格重放1067/1067一致，结束于death@act2；
每点后继快照复验，未改原始trace。原件`performance-capacity-real/{manifest,summary}.json`。

本机小样本证据不能推广到全部实战牌组或跨机器性能；本项完成的是当前实现的
性能优化与回归，不是M5的1万局规模或完整Env/CombatSolver覆盖验收。
