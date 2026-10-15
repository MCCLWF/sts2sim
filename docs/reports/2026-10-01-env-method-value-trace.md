# Env 特权方法值增量

基线 `5fdae84`，分支 `feat/env-rf-complete`；执行
[方法值计划](../plans/2026-10-01-env-method-value-trace.md)。完整 Env/RF 目标继续 active。
本报告只验收观察增量，原版流程、GameState/RNG 和既有容量未改变。

## 实现范围

`tools/ModelExecutionChecks --signatures`从正版v0.111.0 DLL反射530个实际方法签名；
DLL SHA256为`2b40d2df538db1ceb5fa48d958c80ab730ada1e07db88a870aff01a661768b9f`。
导出数据`data/execution_signatures.json` SHA为
`0a23789a2a95a3e5f061b727d5d75f8c87241aa1fa13c966534e7e35181fc2a4`。
生成器精确绑定263个模型派发方法和144个Hook定义；缺失/歧义拒绝生成。
`scripts/check.sh`检查两个生成器的幂等输出，防止新增方法漏观察。

方法原始执行体只调用一次；逐体比较5f源码证明263个模型体和144个Hook体全部保持。
显式preview bit4记录参数入口/正常完成值、原版名/类型/ref/out方向和返回值。
out入口undefined，不读取未赋值out；Choice/Boundary或异常展开没有完成参数/返回值。
参数按实际声明顺序，decimal/float/double保留位模式，引用不输出地址，
集合保留逻辑顺序，复合值读取现有原始字段，不调用lazy getter/delegate。
Task记等待完成，Task<T>记等待后的T值，没有Task对象身份。
无法冻结delegate身份的值明确`complete=false`；对象引用仍没有寿命UID。
所有新增数据只在借用诊断observer里，普通predict/default preview不采集隐藏模型值。

## 冻结构建与实际结果

native build `d4d9f24053fd497a08f2060f45f3c6037e83ffda3f767f35ab8dd59c787044a2`；
共享库 SHA `8be7cf0d94d85eea9057b21791bfc355d34ae5e531e6344536bb11fa719effc3`；
CLI SHA `77dea743d8a96e74f7a8b79a471dfe1d98123ab87188dc36a7080b4a05e98056`。
schema仍为`184ae97c4efc8b56e97e642136b825fab851363d53c142700de771e9e7af6f70`。

| 验收命令/范围 | 实际结果 | 原件 |
|---|---|---|
| `DOTNET=~/.dotnet/dotnet scripts/check.sh` | wrapper实际exit0；54/54 CTest；L2 200/0 mismatch；L3 81完整/1625状态及2有界/4状态；L4 110/110、24183/24183，无Unported/failure | API树`.local/env-method-values-full-gate-v1.log`，SHA `ab1fbb1c387e5bfad04b00e996b210dda4e83cf220e714db53f94627700fa3ff` |
| `dotnet ModelExecutionChecks.dll`及`build/model_execution_checks <native.jsonl>` | exit0；32输入/720模型调用/784帧；4752入口与4752完成参数、64个out入口undefined及完成集合；返回值/参数/元数据逐字段、顺序/完整decimal位值严格相同 | 外部`method-values-native-v4/native.jsonl`，SHA `0288a9b16a8bdcd422941ebc61ee82fe457b39816192d4ca5766176d8e037544`；API树`.local/env-method-values-checker-v4.log` |
| 当前`build/rng_execution_checks`消费不变DLL原件 | exit0；8seeds/2880顶层/11376调用/288洗牌 | 原`rng-execution-v1/native.jsonl`与`.local/env-method-values-rng-checker-v1.log` |
| TestMode=false `rf_operations.py` / `rf_crystal_sphere.py`，开启RNG和causal | 全部wrapper exit0；遗物8/36前缀、水晶球6/57、商店5/2、重抽2/30；合计21case/125前缀；开关完整后继状态相同，source/runtime冻结复验 | 外部`method-values-{relic,crystal,shop,reroll}-oracle-v1` |
| `portable_rf_replay.py`消费上面4份原件 | 两个新进程均exit0；每进程125源/后继，参数/返回记录、所有frame/RNG逐项相同，最终原版结果一致 | 外部`method-values-fresh-rf-replay-v3` |
| `portable_snapshot.py --matrix` | 两新进程exit0，各157源/214后继/12非法拒绝；111怪物仅不可变定义库存，非功能覆盖 | 外部`method-values-portable-matrix-v1` |

以上外部目录在`artifacts/legacy/env-rf-api-2026-09-30`下。证据manifest
`method-values-evidence-manifest-v1.json` SHA
`38714e16c1384f6c3e6f97a62ff94c9edb49c2d57a73e0f4020a0ea3e71052a4`。
21个实际运行文件（27602778字节）已复验SHA保存到`method-values-runtime-v1`；manifest SHA
`bc75cb5c023f5f09f1fd4350a7d78140d76b3167f0e2a223419f9233d45750f4`。

Native探针先计算未安装观察器的原版32组结果，再核对安装后的完整decimal位值和out集合。
早期两个失败发现Harmony的Postfix `__args`会带回旧out值；修正为只读实际命名out，
不在Postfix注入`__args`，并增加每行未观察基线核对。失败v1/v2原件保留；
没有修改游戏逻辑或放宽比较。v3已通过，v4进一步加入未观察基线。
fresh replay的两个错误CLI调用没有启动worker，失败日志保留，正式证据是v3。

## 有限成本复验

外部`method-values-performance-v1/benchmark.py`使用冻结旧库和当前库，ABBA顺序的4个新进程，
同一Ironclad/Cultists固定根的EndTurn。固定CPU23；每项40次预热、15块×100次调用，
包含ctypes调用/branch销毁，排除Python JSON解码；源码/root/所有完整POD后继/库SHA核对。
宿主仍有其他任务，不能作为无噪声或C#/C++语言上限结论。

| ABI操作 | 旧库两进程中位数 | 当前库两进程中位数 |
|---|---|---|
| Clone | 24.80–31.31µs | 23.90–25.29µs |
| Predict EndTurn | 0.409–0.412ms | 0.419–0.420ms |
| 普通preview带queries | 1.041–1.044ms | 1.058–1.059ms |
| RNG诊断 | 1.065–1.068ms | 1.086–1.088ms |
| causal诊断 | 3.720–3.725ms，仅帧 | 17.781–17.853ms，额外全部方法值 |

关闭方法观察路径在该夹具有约2%成本增加；完整值诊断成本明显更高，不能当作高速搜索默认模式。
Clone有预热/分配波动，不能宣称加速。全类型追踪成本、长局容量改动和公平C#/C++整局性能仍待验收。

## 主线规模证据与剩余条件

另一对话已在main **5f**完成新组合全2000原件重放并解除冻结：1996完整一致、0分歧、
4真实活牌池容量失败；2322917/2332668决策，全部671原版胜利完整一致；wrapper实际exit1。
HP10000是注入深度探针，没有完整原生合法动作集/双端逐抽RNG资格。
main5f manifest `f1bcf32303b91ed7b7f10378c15be1231acb274753ff6584037f804405064866`，
关闭证据`artifacts/legacy/mass-seeds-2026-10-01/deep-v3-main5f-full2000-completion-evidence.json`。
这个2000资格不转移给本方法值构建。

本批观察增量可合入；完整Env/RF仍未完成：263/144所有参数类型/条件的独立真值，
Task异步完成全矩阵、delegate值、非generated调用、创建/退休/引用重用账本、
真实档案/live capture、RF21项功能全条件、10k零缺口整局和规模性能尚未达成。
`trace_complete=false`、`causal_model_parameters_and_returns_complete=false`、
`causal_object_lifetimes_complete=false`、`rf_full_matrix_verified=false`继续保持。
容量候选的外部6原件实验不构成公共ABI/SDK已支持大状态的生产资格。

## 主线合入与可调用产物

2000原件wrapper EOF并解除冻结后，main已FF到`b3004d5`，原有`D CLAUDE.md`保留。
主线完整build wrapper实际exit0；shared library/CLI/native build SHA与本报告API树完全相同。
7项Env ABI/恢复/操作/preview/Python合同和独立`.NET SDK`共8项实际exit0。
main native checker再次32/720/784及全部参数/返回严格一致；两个新进程消费API RF原件，
每进程125恢复源/后继和全部方法值/frame/RNG严格一致，wrapper exit0。
主线共享库反向预热恢复API portable矩阵157源/214后继，12非法拒绝，wrapper exit0。
全部关闭证据保存在外部`.local/env-method-values-main-completion-evidence-v1.json`；
各日志为`.local/env-method-values-main-{build,contract,dotnet,native,fresh-rf-replay,cross-checkout}-v1.log`。
随后仅保存本段报告，不改变运行代码/已验收二进制。主线可继续采集新seed，完整目标保持未完成。
