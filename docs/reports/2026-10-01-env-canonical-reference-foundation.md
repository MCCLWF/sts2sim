# Env 模板引用基础增量

基线 `9e3fa62`，分支 `feat/env-rf-complete`，计划为
[对象寿命计划](../plans/2026-10-01-env-object-lifetimes.md)。本批完成计划第1–2项的有限
Card/Relic/Event模板合同。第3–4项的实例UID、创建/退役/物理回收、附件与pending对象
寿命、完整类型/条件及规模性能仍未完成；完整Env/RF资格保持false。

## 修正

原版 `AbstractModel.IsCanonical/IsMutable` 和 `ModelDb` 将不可变模板与 `ToMutable()`
实例区分开；重复取得同一模板是相同引用，模板与副本、不同副本不相等。
C++的CanonicalCard/Relic临时池对象及EventModel.Canonical scratch原先没有这一身份。
不能将scratch的分配/回收误报成原版对象创建/销毁，也不能用scratch槽表示永久模板。

模板handle保留kind+定义ID；所有2519个生成派发case使用Rebind保留该事实。
对当前Dispatch仅逆向替换Rebind为旧构造表达式后，与9e3版本全文逐字一致。
未增加GameState字段、不改RNG/原始方法体；本机AbstractModel仍16字节，GameState
782664字节且可平凡复制，snapshot schema未变。

模型帧新增source_is_canonical/source_is_canonical_known；模板source_ref=0，
source_ref_kind=canonical_definition。非null模型参数新增is_canonical；模板ref=0。
未表示类型返回null事实和value_complete=false。普通实例和pending定义的现有ref仍无
稳定寿命UID。保留的模板诊断ID不随scratch复用改变，失效scratch不能继续任意字段访问。
公共字段合同见[API指南](../env-preview-api.md)。

## 真值与关闭证据

`tools/ModelExecutionChecks`实际运行正版v0.111.0 DLL，TestMode=false；构造显式身份
对应的单人组件夹具。所有damage和bool/out调用先在无观察补丁时执行，再装观察补丁；
每行保留结果全部decimal位和真实out结果一致断言。Postfix只读取具名out，不注入
会写回旧out槽的__args。Frame和全部方法值严格比较字段集合、JSON种类、顺序和值。

输入格式v2包含预先固定case IDs和完整关闭footer。206输入为：96个damage
（null/canonical/mutable card × 8 flags × 4 damage）；98个bool+decimal out
（Card/Relic/Event模板和实例、默认及SpikedGauntlets override，含负数、带符号/刻度
的零和1e-28）；12个Card/Relic/Event真实ReferenceEquals身份病例。

外部证据根沿用 `../artifacts/legacy/env-rf-api-2026-09-30/`：

| 实际命令/产物 | 结果 |
|---|---|
| `DOTNET=… scripts/check.sh`，API `.local/env-canonical-full-gate-v2.log` | wrapper EOF exit0；54 CTest，L2 200/0 mismatch，L3 81完整/1625状态及2有界probes/4状态，L4 110/110、24183/24183，无Unported/截断/错误 |
| `canonical-qualified-native-v1/capture.py` | native和checker均实际EOF exit0；206行、2272模型调用、2464帧、14550对前后参数、290 out对全部一致 |
| 同目录10种损坏输入及独立process记录 | 缺footer/漏case/重复case/结果位错误/frame bool类型错误/模板伪ref/bool返回错误/无unpatched资格/资格bool类型错误/声明返回类型错误全部exit1 |
| `rf_operations.py` obtain_relic / crystal_sphere / merchant_purchase / Orrery+Driftwood | TestMode=false，RNG+causal开启，21/21例、125选择前缀；源/runtime首尾一致；四wrapper均EOF exit0 |
| `rf_operations.py --operation transform_cards --seeds RFOP1,RFOP2` | TestMode=false，RNG+causal开启；36/36例、0选择前缀；3 RNG来源×batch/sequential×3有序选牌组×2 seeds；EOF exit0 |
| `portable_rf_replay.py`，`canonical-fresh-rf-replay-v1` | 两新进程各检查57case、实际恢复125个选择前缀及125后继；完整frames/method值/RNG/最终原版事实一致，两个worker及wrapper EOF exit0；36无选择case仅检查父产物，未称作fresh操作续接 |
| `portable_snapshot.py --matrix`，`canonical-portable-matrix-v1` | 两新进程各157源、214后继、12非法拒绝；EOF exit0；111怪物部分仅定义库存，不是功能覆盖 |
| canonical_references C++合同（上述CTest内） | 模板派发保留、真实模板相等性、scratch复用后冻结ID不变、源完整snapshot/RNG不变、未知类型不伪造mutability事实 |

Native原始NDJSON SHA256：`7643d976487b32b903af06ba05ac856b181e6e1f453c52b9100c4dba90105671`。
关闭证据 `canonical-completion-evidence-v1.json` SHA256：
`a1c7354fb0d96ace87cfeedd99218a579dc7197ff63b0a053e37fc239a831e3c`。
实际source指纹 `0fd472d423cee1e901afc1f6af8923e6df277db1f1b5058364f92ba4f8adb91b`，
native build `a56a3c74474950846b4ab60081c3de6766da137bfa5dd69f6f624467a918cfdc`，
schema `184ae97c4efc8b56e97e642136b825fab851363d53c142700de771e9e7af6f70`。
shared library SHA `9dc9cb4b18ae66d098b631d8f8e7399c5b0bace5c5829df24b8c5039e4d48e19`，
CLI SHA `84aab0221b707f32d8dcd581eed3adaeb867e7c91292cf1f60240ab01e404b78`。
`canonical-runtime-v1`归档21个实际文件、27718190字节，manifest SHA
`9b100f4874a6cb81b3df91892884b8373a210dfc101d6a3e91c102d4e0ed2127`。
版本保留源码patch、DLL probe/checker、SDK夹具与Oracle运行时；不提交游戏程序集和raw产物。

## 性能与限制

`canonical-performance-v1`使用冻结前版库与候选库，4新进程ABBA，固定CPU、40 warmups、
15×100调用，记录clone/predict/plain/RNG/causal及全部输入/后继和库首尾SHA。
四worker和wrapper实际EOF0，所有模式及跨库完整状态完全相同。
但并发负载下旧库clone中位数236.78µs与30.28µs、Predict4.334ms与0.519ms漂移约8倍；
本批**不认证耗时比较、不报告速度改进或开销比例**。raw、原comparison和独立
timing-assessment均保留，需负载稳定后复测；完整Env及C++/C#性能上限没有据此验收。

另一对话冻结main9e3的新1000局实际结束为996一致/1 HistoryCourse规则分歧/3容量失败；
1184399/1186866决策，原胜利中356完整匹配，HP10000注入，不能代表普通HP通关分布。
本模板候选尚未组合30e6f94历史修复，也不将旧规模结果改称候选通过。
根工作树正在该修复的全1000原件重放，保持其Git/source/build/Oracle冻结；本批尚未合入main。
完整创建/退役/池回收与.NET GC不同，pending Affliction的ID编码也不能替代真实对象身份。
RF全部来源/条件、真实档案/live捕获、完整trace/Task/delegate及容量仍待完成。
