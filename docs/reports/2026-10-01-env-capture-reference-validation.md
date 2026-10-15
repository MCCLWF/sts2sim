# Env capture 引用校验与 HistoryCourse 组合验收

基线 `bd4af26` 已包含模板身份 `317054f`、HistoryCourse 规则修复 `30e6f94` 和其全1000原件关闭证据 `b2fddca`。
计划为[引用校验计划](../plans/2026-10-01-env-capture-reference-validation.md)。本批修复公共 snapshot restore 的真实缺陷；完整 Env/RF、对象 UID/寿命和容量仍未完成。

## 缺陷与修正

另一对话在主线 public ABI 复现：将合法 local v4 文档的 state_hash 设为协议允许的 null，
写入 capture 的 `(65535,65535)`、`(0,0)` 或重复 `(0,0)` 后，restore 会接受并原样重导出。
API 前版冻结库 `9dc9cb4…` 独立复现同三类接受。原始请求与基线结果保留在外部
`capture-validation-baseline-v1`；新版使用当前 schema 新导出的有效 envelope 装入同样坏行，
不修改原请求、不把 schema/runtime 不兼容拒绝当成引用校验成功。

`validate_game_state` 现在要求每条 active_card_play_power_captures 的 power/card 非零、
位于对应池内且 live，并且 `(power,card)` 唯一。current 与 replay_base 共用此校验；
portable 解析继续经过严格 local validator。合法 pending 项及任意 signed amount 均保留。
未改游戏效果、RNG、容量、POD 字段或捕获辅助方法。

依据 v0.111.0 `MonologuePower.BeforeCardPlayed/AfterCardPlayed` 等原版私有
`Dictionary<CardModel,int>`：键是强对象引用，同一 power/card 不可重复。
回归通过真实 Monologue Before 方法形成合法 pending 项；原分支、local 恢复、portable
恢复的完整输入及 After 结算后完整后继相同。公开 C ABI 负例覆盖两个字段各自的
null/out-of-pool/dead、live duplicate、两层 hash 均有效的 portable duplicate，及真实
Discovery Choice 的 replay_base 坏引用；均无 successor，诊断对应首因，原 Env 完整状态不变。
portable 负例在专用 corrupt clone 上生成正确 canonical state hash 与 wire hash，避免
只因校验和错误而假通过。测试链接同一 sts2env_c，避免两套注册表。

## 实际关闭验证

所有下列 wrapper 已取得外层 EOF exit0。外部证据根为 `../artifacts/legacy/env-rf-api-2026-09-30/`，
API 构建日志保留在 `.local/env-capture-*.log`。

| 命令/证据 | 实际结果 |
|---|---|
| `DOTNET=… scripts/check.sh`，`env-capture-validation-full-gate-v3.log` | 54/54 CTest；L2 200/0差异；L3 81完整战斗/1625状态及2有界探针/4状态；L4 111/111、25143/25143，无 Unported/截断/错误，包含真实 HistoryCourse 触发回归 |
| `capture-validation-fixed-v1/verify.py` | 三类原坏行装入新版 envelope 全部 public 拒绝，successor=null、源完整状态不变；前版 portable 显式 native-build 不兼容拒绝 |
| `capture-qualified-native-v1/capture.py` | TestMode=false 正版 DLL 206输入、2272模型调用、2464帧、14550对前后参数、290 out对完全一致；10类损坏方法输入各 checker exit1 |
| `rf_operations.py`/`rf_crystal_sphere.py`，`capture-rf-{relic,crystal,shop,reroll,transform}-v1` | 全部 TestMode=false，RNG+causal 开启；57/57用例、125选择前缀；包含36个无选择变牌用例；source/runtime 首尾一致 |
| `portable_rf_replay.py`，`capture-fresh-rf-replay-v1` | 两新进程各实际恢复125选择前缀及125后继；完整frames/方法值/RNG/原版最终事实一致。各检查57父case，36无选择变牌仅检查父产物，未称为 fresh 操作执行 |
| `portable_snapshot.py --matrix`，`capture-portable-matrix-v1` | 两新进程各157源/214后继/12非法拒绝；111怪物部分仍只是定义库存 |
| 外部 sizeof 编译探针 | AbstractModel=16、GameState=782672、trivially_copyable=true；比前模板批的8字节增加属于 HistoryCourse POD 历史修复 |

失败/修复过程不覆盖：v1全门禁因共享链接目标缺 include 路径构建失败；v2在新测试中
因 portable canonical hash 先拒绝而失败。定向 v3诊断明确该原因，v4夹具指针编译错误，
v5定向合同 exit0 后，完整 v3门禁通过。原始失败日志全部保留。

## 冻结身份

Source SHA256：`a5488276150d8e308f9d176e9aa6b2685f5ebb9ca77742f5769116ea44f3249b`。
Native build：`df085dfa51f650388c225cf48f8eb89665a91bf57ba22b780790945bd14c59f9`。
Snapshot schema：`e8a6e6331573f94cb3ef2a264bd22cf3f702b976702322443cf20ee24a361870`。
Library SHA：`875f52ca1d2c162110bd8019b73c23ebc160a3f7473ddeb1bf5d0222e33de73b`。
CLI SHA：`f67215380c050c67b1435deabbe2985818bda4e1ef9ac7cbccf5700aaa847923`。
Native NDJSON SHA：`7643d976487b32b903af06ba05ac856b181e6e1f453c52b9100c4dba90105671`。

`capture-runtime-v1` 保留21实际文件、27722558字节，manifest SHA
`681df6c5f90ec8cf1ac53a9218127e3f25ee887e772d8bc1731cbf56315f346c`。
关闭证据 `capture-completion-evidence-v1.json` SHA
`f66bdcab99090d29ea95116175395642240f29d0c98b9d4334bd587306439a7f`。
源码 patch、失败日志、原生输入、process exit记录与各RF矩阵均保留，不提交游戏程序集/raw。

## 资格边界

HistoryCourse 的旧 main 修复全1000资格仍为997完整一致/0规则差异/3容量错误，
1184425/1186866决策、357原版胜利全部匹配；本批未将它改称 API 组合候选全1000重跑。
本批没有新的有效耗时比较，前批并发漂移约8倍的测量不作为速度结论。

capture 引用校验不解决捕获项上界：正版组件32个 Instanced Monologue 对835张不同牌
接受26720项，旧池1023以内的C++组件在第26625项拒绝。该组件不证明自然合法嵌套835张
可达，但否定“8种power就是最多8个实例”的容量论证。不得据此缩小容量或报全支持。
完整稳定实例UID、附件/pending身份、退役与物理回收、真实档案/live、全RF功能/条件与规模性能继续待补。

## 主线整合关闭

提交 `8728b09` 后main由b2fddca fast-forward合入；保留用户的未暂存 `D CLAUDE.md`。
主树 `scripts/check.sh --quick` 实际EOF exit0，54/54 CTest及烟测通过；API树完整差分
门禁资格保留于前述精确候选代码。主树独立构建 source/native-build/schema、CLI/CABI SHA
与API冻结候选逐位一致。`capture-main-integration-v1/verify.py` 实际wrapper EOF0：
主线checker对原版206行完全一致，两worker各跨树portable157/214/12，及RF125前缀/125
后继通过；不重复声称无选择操作fresh执行。summary SHA256：
`bf33c078cc23ffdee7e53156118641ccc484fbd77504036255010ce3b27a3f9c`。
另一对话独立重绑3原坏数组于新版envelope，公共status=-1、successor=null、源完整
snapshot不变及库首尾SHA一致；重复zero例先被null规则拒绝，live duplicate另由本批合同证明。
证据 `mass-seeds-2026-10-01/peer-api-capture-fk-root-recheck-v1.json`。
