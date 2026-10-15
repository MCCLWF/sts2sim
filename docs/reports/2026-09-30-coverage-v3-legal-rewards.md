# 原版奖励合法选项采集与执行覆盖

属于 E0/E3，基底 main 575e099；复用同一工作区，分支为 fix/coverage-v3-legal-rewards。完整API由另一聊天开发。本报告记录采集策略修复和有限执行证据，不能作为完整Env/RF、全条件或实机认证。

## 奖励采集首因与修复

[历史摘要完整复验](2026-09-30-monster-history-summary.md)的2000旧原件有1993完整一致，2活卡池容量及5连续第二次REROLL保持失败。v0.111.0 `CardReward.OnSelect`为headless选择器保留初始alternatives；`Reroll`设置CanReroll=false，随后Populate调用Generate并刷新真实UI，去掉重抽。旧coverage-v2在缓存中继续随机采样，与刷新后UI不同。API拒绝禁用选项的行为保留；旧轨迹、结果和原资格不改写。

coverage-v3在原生RewardsSetSynchronizer选择奖励期间绑定实际Reward，用try/finally恢复嵌套选择上下文；采样时读取CardReward.CanReroll，保留回调原索引但排除不可用REROLL。没有额外调用Generate/Hook或游戏RNG，只更改独立policy RNG的合法候选宽度。legacy-v1和coverage-v2的候选顺序/随机调用保留；新采集默认v3，版本进入begin、batch config和job身份。

v3奖励动作另外记录原生native_reward_can_reroll和legal_reward_indices；解析时缺事实、候选不符、非整数或选中不可用项均拒绝。动作审计字段不改变已比较的游戏决策/状态/终点字段，未宣称双端全合法集合差分。旧v2轨迹继续可解析，其第二次重抽仍会在实际API执行中失败。

## 已执行验证

证据根为workspace `artifacts/legacy/mass-seeds-2026-09-30/`。

- Oracle Release构建已exit0；旧宝箱RelicPickingResult.player的可空性警告仍在，没有玩法构建错误。
- `python3 -m unittest discover -s tests -p 'test_differential*.py'`：**49/49通过**，日志`coverage-v3-protocol-check.log`。新增拒绝用例由真实DAA098F90159CD9第331决策的缓存候选及原版Reroll后的false事实构造；不以模拟器输出生成规则期望。v3默认身份、历史版本匹配、禁止L3/replay策略参数及续跑改版本拒绝均保留。
- 5个旧故障身份以同seed/角色/升阶/policy_seed **新采集v3**，HP10000注入、Neow、stop=victory。`coverage-v3-reroll-cases`已完整EOF、exit0：**5/5 identical、4301/4301决策**，4 death@act1、1 death@act2，每决策snapshot successor开启。每个身份都实际重抽至少一次；61个原生奖励选择，7次首轮重抽，7次紧接的选择均读到CanReroll=false并排除REROLL。不是重写旧v2动作或仅比较非法点前缀。
- 该批执行时parent为575e099，策略/协议源未提交；source SHA256 `849425cc5ec906b9e78fc2a909d3fea21bd2d3c1263f3574a8dbec5e48436240`，CLI SHA256 `5f575509f7c9a3341b817df607818532830cc5b68a4cc4872edc697dd17ff163`，Oracle DLL SHA256 `7204ea1d58ef1a7e072d0244472353807d3c04b46a066022384a1f746255c578`；manifest SHA256 `ea39bbd2f75727cecd1508bccfb944740c969562ba570e36ebb050e34420a88c`。逐动作原生合法性证据`coverage-v3-reroll-native-legality-evidence.json`，SHA256 `d0894e2ee8be59311d8392748702f7d9242937f8ad452bf80f9d4155c8054539`。
- 本候选完整 `taskset -c 0-2 scripts/check.sh` 已EOF、exit0、all checks passed：52/52 CTest，259/259非hero遗物，L2 200/0 mismatch，L3 81完整战斗1625/1625及2 probes4/4，**L4 107局20671/20671决策**。新增4个v3 Driftwood/GamblingChip注入用例1715/1715，旧v2显式回归保留。日志`coverage-v3-final-check.log`，SHA256 `6f9a4c51f3bb61788347cda3ec673b3a0d83ed11eea8a6478aaa33e28d7880c3`。该门禁是575e099基底上的v3候选；另一聊天后来合入的portable增量不据此宣称已组合验收。

## Portable 组合版本验收与合并

随后已对齐 API portable/history 主线 68420b4，冻结干净提交 aec8a8fff3e767fb18f6bd0ae5be8318f1da68ce，到所有进程 EOF 后才收尾文档。组合 source SHA256 `af320c20a78a1a22f1924b2fed34f419b4c420154bb1430a9aa5d41251de93f2`，CLI SHA256 `7386e11afcfe667fbeac725380c630cfd4cc55aa3a08df8318cdb9aa17729cf0`，Oracle DLL SHA256 `b2a347c090b927f0b9307f2f75d7b1adf4bda80056274b456ef0ac39e948df37`。

- 完整 `taskset -c 0-2 scripts/check.sh` EOF exit0：**53/53 CTest**，259非hero遗物，L2 200，L3 81完整战斗1625/1625及2 probes4/4，**L4 107局20671/20671决策**；全部通过。日志 `coverage-v3-portable-integrated-check.log` SHA256 `9509efbda003b2cffc7613de8794ef2c86ead15dba154bf6c87cafd98a62a408`。
- 5个故障身份再次从原版新采集，`coverage-v3-portable-integrated-reroll` EOF exit0：**5/5 identical、4301/4301**，每点快照后继开启。manifest SHA256 `946e3683008650c46f4f0cbc95429c18ae5c9e643666a24940ee1a61e0e6f33b`。直接复核这批原件的61次奖励动作、7次重抽及其7次禁用后继，均满足原生合法候选；证据 `coverage-v3-portable-native-legality-evidence.json` SHA256 `fa8bef53f823ffbc30ee25fedc3521fd4495449f8edceecd47526c1bfd62dd8b`。
- 两条长轨迹 `coverage-v3-portable-integrated-long-replay` EOF exit0：**2/2 identical、4907/4907**，每点快照后继开启；manifest SHA256 `cc14eb6581e4717ef29cbb85c29a526e6768b4d77ad205ab7b1ca6035af23d72`。

检查后与 b3dad8f 合并仅加入 API 的10行验收文档，未改动冻结的执行源。本项合入 main；此前2000局全量结果仍属于0c6c596历史摘要版本，不能据这7条组合轨迹改称全2000已在portable/v3版本重跑。

## 卡牌执行证据和范围

603个原版CardModel按原导出元数据分为37个MultiplayerOnly、7个Mock、27个带Unplayable关键字和532个不带该关键字的单人非Mock模型。此分组只用于手动CardPlay清单，不把Unplayable模型的抽牌/弃牌/回合结束/自动出牌等Hook排除出总体目标。Status类型也不代表不能主动打出。

冻结0c6c596的1993完整匹配轨迹，**526种不同CardPlay基本模型**的每个首次原生动作证据都属于完整通过轨迹，不借用7条失败的未来动作。`deep-v2-qualified-card-execution.json`有原始路径、行号、身份、捕获/复验manifest和输入SHA；文件SHA256 `d778359f054608791f2b04935057e5a5d3bd5a189e1cf405f8ba4e9b3d71b51d`。这是实际手动输入覆盖，未包含所有autoplay/Hook/条件/升级组合。

剩余6个可打基本模型在同一冻结版本补了20个有界注入用例，合计60/60观察一致，均从原版捕获实际play动作，分开记录而不计为完整整局：

| 模型 | 用例与观察 | 实际执行证据 |
|---|---|---|
| Caltrops及升级 | 2 seed × A0/A10 × base/+1，8 probes、24状态 | 8次真实出牌；施加3/5荆棘；随后SludgeSpinner/Nibbit攻击，8次分别3/5反伤与玩家HP、RNG计数一致 |
| DeprecatedCard | 2 seed × A0/A10，4 probes、12状态 | 4次实际出牌、抽牌、耗尽及敌方后继一致；本L3投影不独立证明DeckVersion移除 |
| Disintegration/MindRot/Sloth/WasteAway | 1 seed × A0/A10 × 4模型，8 probes、24状态 | 负基础费用不直接等于不可打；原版实际打出，资源、牌堆和敌方后继一致 |

对应目录`caltrops-bounded-probes`、`deprecated-card-bounded-probes`、`knowledge-demon-card-play-probes`的manifest SHA256分别为`eada13f531b520c6a2575c6fd728bb5863a67f9b9b66c83482fc14ecc49149e8`、`9eae1e0d9ba115125bc62600fb7b259f6e8f560cd7394c538a25fad24a0d3a6f`、`00f41dd87d501987aea3a390219872f31f67c5718f79adf11715f1bd2cffe030`。实际出牌/反伤审核文件为`caltrops-probe-verified-evidence.json`（SHA256 `db14fb3a9b1e109e843083cc2a0f4292d87c8f9a3f089ff7adf621f9f546e5b6`）和`status-card-probe-verified-evidence.json`（SHA256 `8766769bd54310845c1b48403a8896bc9e61562befc8e8d9f8dbb717e1f1d574`）。

KnowledgeDemon真实选牌调用IChoosable.OnChosen也已另计：1993通过轨迹中Disintegration 486、MindRot 163、Sloth 147、WasteAway 143次实际选择。首个后继combat可见能力分别485/163/147/143；另一次Disintegration选择后立即转入奖励、换幕，新战斗已清除能力，不能把这次算作后继能力观测。源码和原件证明这是选择回调，不是CardPlay计数。所有手动可打基本模型至少一次执行的532/532集合结论仅由526完整轨迹模型与6个注入probe模型相加，不代表532种都在自然整局达到。

DeprecatedCard牌组移除另有`deprecated-card-first-act`：4个HP10000/替换牌组新采集，**2 identical、722/722决策**到第一幕boss前；两条实际打出DeprecatedCard，随后原版run.deck不再含它，逐点快照后继通过。另两条在Neow领取NewLeaf时，原版因DeprecatedCard没有合法变牌候选抛错，保留**2 oracle_error**，不伪称通过或改原版规则。该批manifest SHA256 `c885737fd3e1b5fa43bafeb83c8b0ff35fea6cea16723244adb3c8e898982d29`。

## 剩余

两条活卡池容量未修复。所有本项整局与卡牌probe保留既有TestMode/headless资格，不能代替TestMode=false真实完整对局。全部升级/组合/条件/Hook、原生完整合法集合、双端逐抽RNG、真实性能、任意live状态和完整Env/RF仍未验收。后续大批新seed采用v3、固定源/CLI/输入到EOF，继续保留严格失败和原始证据。
