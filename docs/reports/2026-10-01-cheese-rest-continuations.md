# 新深层 seed 的奶酪房间与嵌套休息奖励续接

## 前置状态与原始基准

基底main `702d14f0689c630637f164c11c992b358bb4d1b4` 已包含portable/history及coverage-v3。API伙伴独立开发RNG call trace；该功能尚不在本基准。当前两工作树保留，修复复用主线目录，未创建新工作树。用户CLAUDE.md删除保留。

200个全新seed ×5角色 ×A0/A10，HP10000注入、Neow开启、coverage-v3、stop victory、每决策快照后继，完整2000采集EOF exit1：1993 identical、3 diverged、4 native_error，2321496/2332668决策。原生终点671 victory@act2、330 death@act2、859 death@act1、140 death@act0，基准完整匹配668胜利、330 death@act2、855 death@act1、140 death@act0；不能将这些原生终点全部写成通过终点。manifest SHA `4c8a4a3133e7d06e5796b80e4d9fed71b7995f250d2ae059d791b1a25b094503`；源码SHA `af320c20a78a1a22f1924b2fed34f419b4c420154bb1430a9aa5d41251de93f2`，CLI SHA `7386e11afcfe667fbeac725380c630cfd4cc55aa3a08df8318cdb9aa17729cf0`。包装进程EOF后重新核验Git/source/runtime/Oracle外部依赖/environment/system全部一致。

原件目录 `artifacts/legacy/mass-seeds-2026-10-01/deep-v3-fresh2000` 和 completion-evidence 在仓库父目录，未提交大产物。原版stdout与动作stdin保留；1999份已结束CPP输出逐份SHA与gzip解压SHA核验后归档，净节省3247846294字节。原始manifest/results与失败行保留，压缩布局不可盲目resume。

## 首因与改变

- `F87A20DE9374C77` / DEFECT A0 policy144745002 和 SILENT A10 policy783630005：原版RoomFullOfCheese.Gorge调用EventModel.SelectCardsToAddToDeckFromGrid，后者逐张await CardPileCmd.Add。CPP误用批量Add，BingBong触发后牌组为A,B,A,B，而原版为A,A,B,B。仅事件改为逐张Add；不改变公共批量Add、BingBong或原版明确批量加牌的ZenWeaver。
- `F4C2E1F1DA91DCC` / SILENT A10 policy1041113654：MiniatureTent保留第二次休息操作，TinyMailbox的HEAL奖励产生嵌套药水选择。原版采集策略完成一次操作后Proceed到Map；CLI只在最初rest/select后做这一步，奖励proceed返回RestSite时遗漏。策略收尾统一放在CLI ApplyAction末尾，last_chosen/proceed_enabled确认选项已完成。公共Env仍保留MiniatureTent额外操作。

## 已完成的外部候选验证

外部overlay仅替换两处源文件，链接冻结基底对象；构建recipe和各输入SHA独立记录。有效候选源码SHA `5ac2832cb2c7b5729405bd4403cd521ec3e442350cd95d25cfe3210d9d4f0f87`、CLI SHA `23381c186e2370583621bcafb3a5eb57755a94ea56d37d13083522ae8d2f051b`，其普通manifest源码SHA仍是冻结基底，须与overlay构建record配对，不替代正常源码或build fingerprint认证。

`tools/differential/batch.py replay` 使用三个未改故障原件和两条原始长局，5/5完整一致、8419/8419决策，快照后继开启，EOF exit0；manifest `90de9d14abc77d87b981301d70e4fa330f74337c1bd75fa56208d3d138044898`。

筛选已结束原件中有MiniatureTent（136）、ZenWeaver（68）或BingBong+RoomFullOfCheese（11）的206条独立身份，外部候选206/206完整一致、256159/256159决策，95胜利、其余真实死亡，快照后继开启，EOF exit0；manifest `fa66aff4e48b7498ec8047dd933124417ba8bffc0759507e320e805363d1db42`。presence用于选样，不证明每个hook/组合实际执行。206份CPP输出、206份native回放副本逐档验证压缩，原生首次采集原件保留。

新门禁 `tests/l4/run_continuation_regressions.py` 不只检查一致性，还确认两个角色实际Gorge+BingBong选择不同牌及TinyMailbox嵌套奖励后Map；外部候选fresh native3/3、3512/3512通过。该测试已纳入正常仓库完整门禁，3/3、3512/3512及三条触发合同通过。

## 正常仓库验收

`taskset -c 0-2 scripts/check.sh` 完整EOF exit0：53/53 CTest、L2 200/200，L3 81完整战斗1625/1625状态及2探针4/4，L4 110/110路线24183/24183决策，最终all checks passed。日志SHA `b3bb2f15054d26fab1bfbc66ceef5528ada65acac21442738ddfdb8ed2817cd7`。正常源码SHA `7344776dc2041cb1e0d670a318820eb393ee8d40fd16002dd0bfdc27d60d38d3`、CLI SHA `008d5f68c35d457c06507c0cb83b24177eaf6a90fecebab838bb0cfb65e84c29`。

正常构建随后严格重放三个未改原始故障文件，3/3完整一致、3512/3512决策、三个原生victory@act2均匹配，快照后继开启、EOF exit0；manifest `0b48f28a02df798df85bf9e219cd854f52f83c27cf892adbe399a9c61fa59710`。新2000全部原件在修复版本仍待冻结复验；本段不把3/3提升为2000通过。

另外逐档重新校验全部2000 native原件SHA，并审计23919次带原生合法性元数据的奖励回调：17858卡牌选择、5948 Skip、61 SACRIFICE、52 REROLL；全部选中索引属于legal_reward_indices，52次禁用后残留的缓存REROLL全部排除。此证据限定原生记录的CardReward回调，不证明全部战斗/房间合法动作集。

## 保留缺口

4个新容量失败保持原始状态；完整原版牌堆峰值1933/6348/1278/1924，6348的身份为F2DA2AE8FF3FF1D/NECROBINDER/A0/p846742400，尚未包含deck原件、临时牌和allCards非pile根。2048/4096不足覆盖；8192仅尺寸探针，使GameState由782664增加到5750088字节，不是完整容量或实际Env性能证明。旧深层2000的资格仍归其冻结0c6c596，不能转移到此版本。

本批比较范围为导出状态/选择/RNG计数和同构建快照后继；未升级逐抽RNG、全部原生合法动作集、TestMode/live完整接受证明、自然HP长局覆盖、全内容条件、完整Env/RF或性能资格。
