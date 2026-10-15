# 怪物 StateLog 的等价 POD 表示

本项属于E3/M4长局可达状态补覆盖。初始基底f63ae51，收拢分支后对齐main 73d053e；原始数据只来自v0.111.0反编译源码与正版DLL。完整API由另一聊天继续。冻结0c6c596的完整2000身份复验已结束，1993完整一致、0状态分歧、7执行拒绝；本报告记录其具体资格。最后一次快照验证跟进的完整门禁和两条长局也已通过；本项表示修复可合入，不宣称全部身份或完整Env/API通过。

## 首因与表示

原版MonsterMoveStateMachine.StateLog每次记录向List追加，没有1024条玩法限制。deep-v1的MA73D889EDE60DF/REGENT/A10/p535937513和deep-v2的DC209E17DB81C54/IRONCLAD/A0/p45722707均在1025回合被C++旧定长列表拒绝。

全源码消费者是RandomBranchState.Contains(UseOnlyOnce)、连续同状态后缀、过滤IsMove后的末尾cooldown，以及Creature.StunInternal、IllusionPower、FlutterPower的Last。候选保存seen/seen_moves mask、总数/Move数、last/consecutive和每个Move距末次Move的age。源注册ID唯一，索引代表同一注册对象。只在Move条目追加时推进age；全部历史对Contains保留。RepeatMultiplier按原版float阈值与同状态后缀长度比较；零/负maxTimes和float整数舍入保留。权重callback调用与RNG顺序不变。

该表示为120字节POD，候选GameState为782664字节；仅表示体积，不是Clone或Step性能验收。原私有列表不逐项导出，登记D-010。schema变更明确拒绝旧版本，不声明原版任意live历史映射。

## 已执行证据

证据根为workspace `artifacts/legacy/mass-seeds-2026-09-30/`。

- `tools/MonsterHistoryChecks`直接调用正版DLL的私有GetStateWeight，TestMode=false。4组历史每组4097条，含non-Move条目、once、不同repeat阈值、cooldown及1023/1024/1025边界。`monster-history-native-clean2.tsv`经`tools/monster_history_checks.cpp`逐项bitwise比较：**123552权重查询、624 Last/Count边界、16388追加全部一致**。没有用模拟器输出生成期望。
- 候选Release构建、49/49 CTest通过。最终单项snapshot测试通过mask/count/age畸形拒绝与f63旧schema精确拒绝。
- `monster-history-long-candidate-replay`对两条原始长局完整EOF，exit0、**2/2 identical、4907/4907决策**，逐点snapshot后继开启；终点death@act1和death@act2。manifest SHA256 `57b473267ab3e40cb99cc2be621b1fef5aadd22748354278333799ba49021535`。

命令：`dotnet build tools/MonsterHistoryChecks -c Release`；运行其DLL保存TSV；`c++ -std=c++20 -O2 -ffp-contract=off -fno-fast-math -I. tools/monster_history_checks.cpp -o <checker>`；`<checker> <native.tsv>`。长局使用`python3 tools/differential/batch.py replay --out <新目录> --traces <两条原JSONL> --jobs 2 --timeout 180 --cli build/sts2sim_cli`，不更改参考动作。

## 集成后的完整证据

- 冻结版本 `0c6c5961b78cf92d2bcb00f3b0993a27557a449b`，源码 SHA256 `69307f8cfb7a12d771a2297de51717f9be5eca70791f88c53ce58dc9f8e78cf3`，CLI SHA256 `18877f5c4fe0a0478707da671f9b31f2be2b10f4b719ff6a2b508b04d960f28b`；执行时干净，至 EOF 未更改源码、提交或运行时。
- `monster-history-integrated-check.log` 完整 `taskset -c 0-2 scripts/check.sh` 已 EOF、exit0：52/52 CTest、259/259非hero遗物、L2 200/0 mismatch、L3 81完整战斗1625/1625状态及2有界probe4/4、L4 103局18956/18956决策，到达all checks passed。日志 SHA256 `200c1fac942b16214d87dcb9eccfe7ff16d43cd5e88d9dfe2caa4946b2a351a9`。
- `monster-history-integrated-long-replay` 两条原始长局已完整EOF、exit0，2/2 identical、4907/4907决策，逐点快照后继开启，manifest SHA256 `0f87f750c1c6c04b334e5d8d32bae2e9e226528ec2f0f6474662b586f0b67e7a`。
- `deep-v2-full-monster-history-replay` 严格离线重放全部2000原始captures，4进程、timeout180，已完整EOF、exit1：**1993 identical、0 diverged、7 native_error，2340221/2341451决策**。完整匹配终点158 death@act0、849 death@act1、335 death@act2、**651 victory@act2**；原版651个胜利终点全部匹配。manifest SHA256 `cb91cbfe64cfa53e4d17ed0842e3067ddb2af6579472923a7b4139420c424db4`。全部先前通过身份也在此版本复验，不仅重放旧失败集。
- 7条拒绝中的2条是仍未解决的活卡池1023容量：D840A2150523CC0/IRONCLAD/A0/p1676726483，第2023点；DE7CEB1029A341C/SILENT/A0/p917732361，第1425点。原版仍活卡不能删除来掩盖容量。
- 另外5条均已逐原动作核实为首次重抽后连续第二次 `REROLL`：DAA098F90159CD9/DEFECT/A10/p139444214，D0E0BD1A5D76239/REGENT/A10/p559205956，DE81E134F207D5C/IRONCLAD/A10/p1703975294，DFB23FA35736500/REGENT/A0/p1773316207，DAA098F90159CD9/SILENT/A0/p1631262927。原版 `CardReward.OnSelect` 缓存 alternatives，但 UI 在 `Populate` 后按 `Generate` 刷新并移除已不可用的重抽。旧 coverage-v2 driver 在缓存中采样；API拒绝禁用选项符合原版 UI，详见O-017。原结果保留native_error，不改旧轨迹、不放宽API，也不把5条计为通过；triage为 `deep-v2-repeat-reroll-triage.json`。
- 重放包装器只在结果完整落盘后压缩该job的复制输入和输出，逐份解压核对原SHA后落盘ledger，再移除冗余未压缩副本。3992份gzip净减少6725049272 bytes；原外部2000份oracle输入仍为原始JSONL。wrapper和batch均已终止，batch exit1忠实保留。manifest/results及gzip-ledger保留原artifact SHA；恢复原路径用gzip -dk，不能将压缩布局直接当作可resume的原布局。

完整对拍仍限定原始captures的TestMode/headless、导出投影、RNG计数和静止点snapshot successor，不升级成实机或全部逐抽RNG认证。最后跟进只加强snapshot的seen_moves必须精确等于seen内Move状态；新增拒绝遗漏已见Move的用例，规则、POD字段、schema和RNG调用不变。全2000结论明确属于上面的0c6c596冻结版本。

最终跟进源码 SHA256 `46c3850333fb899632febbb87b9890630d027bf140cc2145c646136c7b72c891`、CLI SHA256 `5f575509f7c9a3341b817df607818532830cc5b68a4cc4872edc697dd17ff163`。执行时父提交为0c6c596，validator和snapshot测试未提交；之后的提交封存相同代码及报告。`monster-history-final-validation-check.log` 已完整EOF、exit0、all checks passed：52/52 CTest，259/259遗物，L2 200/0 mismatch，L3 81完整战斗1625/1625状态及2有界probe4/4，L4 103局18956/18956决策。日志 SHA256 `7986e88cb12dc3b6e9c5ba89974e2aedfa83708d770b7aeb58c23bf0180c56ab`。`monster-history-final-validation-long-replay` 同两条原件也已完整EOF、exit0、2/2 identical、4907/4907决策，逐点snapshot successor开启；manifest SHA256 `deaeea75a7232dea40109ebca39a116abb87dd689bf0cd7dddc59d516956033f`。没有把旧全2000资格改写成最后跟进源码上的再次全量运行。

## 磁盘中断与剩余范围

最初最终门禁和两轨迹复验因磁盘ENOSPC未能启动，不计通过。用户随后要求收拢分支：main集成最新API/规则版本，9旧树/14分支移除并归档，已结束大文本逐份无损压缩校验，空间恢复到约8.8G。上述证据是在对齐新main后重新冻结取得。

f63全2000重放因ENOSPC退出2，只有1231完整记录（1213一致/2容量/16工具错误）以及1截断末行；该批不完整，不能将工具写入故障当玩法规则首因。其原输入和已输出保持，旧CLI已按SHA封存；后续在当前版本使用新输出目录，不盲目resume已删除旧树。

两条活卡池超过1023的失败与旧采集策略重抽资格未解决。原生完整合法集合、双端逐抽RNG、全部条件覆盖和完整Env/API仍未认证。
