# RF 初始操作的完整跨进程续接增量

基线 `202afd9`（capture FK修复8728b09已合入）；[本批计划](../plans/2026-10-01-env-rf-initial-replay.md)。
补的是对拍工具的真实漏检：旧fresh工具只恢复Choice前缀，无选择case静默跳过操作执行。
本批只改独立测试消费者及capture测试注释，不改游戏规则、API、ABI、POD、RNG或容量。
完整Env/RF仍未验收。

## 改动与资格

RF父消费者现在对每个主操作保存initial local/portable/完整queries/typed request及completed
portable/完整queries；保存过程验证源完整snapshot不变。商店禁买请求同样保存。
新进程先恢复initial portable，重绑定运行时source_hash，再真正执行原typed operation：
有选择精确比较第一个prefix；无选择精确比较完整canonical最终状态、完整查询、operation result
和原版最终事实。所有125 Choice答案继续独立恢复、执行、比较完整后继及完成状态。
4个禁买请求必须得到相同明确拒绝、无successor且完整原状态不变。

旧prefix-only父件缺initial会失败，不再默默给无选择case加一而不执行。输入冻结包含
原Native原件、所有父文件、library、消费者与Python binding代码SHA；每个child oracle.json
必须逐字段/JSON类型与实际Native原始行一致，case目录数与Native行数一致。

原版状态/RNG末状态/规则事实来自TestMode=false新Native捕获；C++逐方法frames、完整
方法值及逐抽RNG诊断与父C++执行完全相同。这不是原版整局所有方法的Native trace认证。
已有206输入Native方法组件资格继续属于其冻结组件；本批CABI/CLI字节与该组合候选完全相同。

## 实际关闭证据

外部证据根 `../artifacts/legacy/env-rf-api-2026-09-30/`，所有wrapper已实际EOF exit0：

| 执行 | 结果 |
|---|---|
| `DOTNET=… scripts/check.sh`，API `.local/env-rf-initial-full-gate-v1.log` | 54/54 CTest、L2 200/0差异、L3 81/1625及2有界探针/4状态、L4 111/111与25143/25143，无Unported/截断/错误 |
| 新RF父采集 `initial-rf-{relic,crystal,shop,reroll,transform}-v1` | 全TestMode=false、显式RNG+causal；57/57 case、125选择前缀，源/runtime首尾一致 |
| `portable_rf_replay.py`，`initial-fresh-rf-replay-v1` | 两新进程各57初始成功操作，40无选择操作（36变牌+4商店），4禁买拒绝，125恢复prefix答案，总182成功后继；完整canonical源/后继、查询、方法值/frames、RNG及Native最终事实精确相同 |
| `initial-rf-replay-negative-v1` | 缺initial、缺completed、错typed request、错completed完整状态、child Native原件不符、缺case共6类，各实际wrapper exit1；负例runner EOF0，原36变牌父件首尾SHA保持不变 |

六个负例诊断分别确认为initial/completed文件缺失、parent.initial_request.kind、
fresh.canonical_state…gold、parent.exact_native_case.version、parent.case_directories；
没有用其它失败掩盖所测漏检。变牌矩阵为2 seeds×3 RNG源×batch/sequential×3有序选牌组；
不是57种RF功能，也不是所有来源/条件。2个Driftwood setup由父消费者执行并对Native校验，
不计为fresh重放的主操作。

Source SHA：`697404726efb9f128e3bcd4a3b3e21ce0ce235248cd6748dfd2ff2e74b30f14c`。
Native build：`df085dfa51f650388c225cf48f8eb89665a91bf57ba22b780790945bd14c59f9`；
schema：`e8a6e6331573f94cb3ef2a264bd22cf3f702b976702322443cf20ee24a361870`。
CABI SHA：`875f52ca1d2c162110bd8019b73c23ebc160a3f7473ddeb1bf5d0222e33de73b`；
CLI SHA：`f67215380c050c67b1435deabbe2985818bda4e1ef9ac7cbccf5700aaa847923`。
与8728组合冻结文件逐字节一致，没有新的游戏运行时或性能变化。

关闭证据 `initial-completion-evidence-v1.json` SHA：
`79da8ca74f315e9425c6f8c6a4149271694b79b8862cddc323ae6b6929f60402`。
fresh summary SHA：`161069e1674ae2e3e8d58b54f57b0850a3d258ee2964ab758fc471eab375a94c`；
negative summary SHA：`44bad8a44558edc0626ed300708807862c16deef6508f3b0f9bfacd921feb63f`。
原旧captures、坏输入及独立process记录全部保留；没有把旧结果改为新版重采结果。

## 空间与剩余范围

已关闭API证据的独立去重runner实际EOF0，1697路径首尾SHA复验，266个完全相同输出通过
SHA+逐字节比较后hardlink共享，真实释放99180544 allocated bytes（约94.6MiB）。
原文件路径/字节全部保留，排除源码/build/库/DLL及当前initial-*证据；fsync操作账本与初始
每文件SHA保留于 `storage-maintenance-v1`。不是删除测试原件或重写旧资格。

另一对话的b2独立封存新500原件已关闭500/500、542311/542311；此结果只属于冻结b2。
当前main202afd9全500复验待其实际EOF，期间main源码/build/Git commit保持冻结。
本工具增量仍在API分支等待该窗口关闭后合入。稳定实例UID/附件pending寿命、退役与回收、
容量、真实档案/live、RF完整功能条件、全部Native执行覆盖与规模性能继续待补。

## 合入与新版主树复核

另一对话当前main202/sourcea548的全500原件重放已实际wrapper EOF exit0：
500/500、542311/542311逐决策及snapshot后继一致，127胜利、75第三幕死亡、247第二幕死亡、
51第一幕死亡的完整终点匹配；HP10000注入资格保留，不能代表普通HP通关分布。
manifest SHA `0c003dad639ce73b6ba27e08516d1bf8e2634611b741bd0e663cbd4510f813bd`，
证据 `mass-seeds-2026-10-01/deep-v3-main202-full500-completion-evidence.json`。
Git/source_files/dirty/CLI/CABI/Oracle/external/.NET/环境及全部500未改Native SHA在EOF后复核一致。
该500资格属于main202/sourcea548；源码697工具增量没有另跑全500，不转移其源码资格。

冻结释放后本增量提交a7bf695，main fast-forward合入。主树quick实际EOF0，54合同与烟测
通过；CLI/CABI再次独立保持与202/f672/875f完全相同。跨树新版消费者
`initial-main-fresh-rf-replay-v1` wrapper实际EOF0：两个main-library新进程各真正执行57初始
操作（40无选择）、4拒绝、125Choice答案与182成功后继，完整源/后继/查询、方法frames
及RNG与原版父捕获事实通过。summary SHA：
`161069e1674ae2e3e8d58b54f57b0850a3d258ee2964ab758fc471eab375a94c`。旧工具125-only资格仍按原版本保留。

只保留main与API两树两分支，用户的D CLAUDE.md未动。完整Env/RF及稳定UID/附件pending
对象寿命仍未完成；源码又发现未附着Affliction的Amount原版可写、当前表示无独立存储，
须和对象寿命一起补齐，不能只给定义编码贴一个UID标签。
