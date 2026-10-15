# 新 1000 局：History Course 回合历史

## 冻结基线与真实范围

`9e3fa62e88c2f87a5bdffa49eb31713b6e80be17` 上新生成 100 个 run seed，五角色 × A0/A10 共 1000 个身份，与扫描到的 1482 个历史 seed 字符串没有重叠。采集使用 `coverage-v3`、Neow、HP10000 注入、完整 victory/death 终点、10000 决策预算、四并行和 180 秒单进程预算。严格比对导出状态、选择与 RNG counters，并逐决策校验 snapshot successor；不宣称双端 RNG draws、全部原生合法动作集或正常 HP 晚幕资格。

外层 wrapper 已结束，退出 1；其冻结输入在实际 EOF 后复验一致：Git/source/files/dirty、CLI/Oracle/游戏 DLL/.NET/本地化、环境和系统。保留用户 ` D CLAUDE.md`。正式基线为 **996 完整一致 / 1 diverged / 3 1023 活卡池错误，1184399/1186866 决策**。原版终点 357 victory / 170 death@act2 / 383 death@act1 / 90 death@act0；完整匹配终点分别为 356 / 168 / 382 / 90。失败身份和原件不改写。

本地证据：`deep-v3-fresh1000-method-values-completion-evidence.json`、`manifest.json` 和 `results.jsonl`。manifest SHA256 `0f9d6d21ffe334424bb312b4f643e005dd36cbc5728615e4c89bb1f1866d998f`；source `60a111e91548f032afe26e9651a8fe17866108cccacc450180883415dffce27d`；CLI `77dea743d8a96e74f7a8b79a471dfe1d98123ab87188dc36a7080b4a05e98056`。全部 1000 个已关闭 CPP stdout 经 result 原 SHA、gzip 解压 SHA 和 fsync ledger 核验后无损归档，节省 1660153102 字节；所有原版 stdout、动作 stdin、错误结果保留。

## 规则首因及改动

`FF7429B102A14F2` / Ironclad / A0 / policy seed `892152143`：首个差异在第 934 决策，Test Subject HP 原版252 / C++268，CombatTargets 原版47 / C++46；后继第957动作的非法性是后续现象。

原版 `HistoryCourse.AfterAutoPrePlayPhaseEntered` 读取 `CardPlaysFinished.LastOrDefault`，筛选玩家、`HappenedLastPlayerTurn`、Attack、非 Dupe。第16回合初始抽牌使 Hellraiser 自动打出 Twin Strike，覆盖模拟器唯一的最近非 Dupe Attack 记录。进入 AutoPrePlay 后原版仍能找到第15回合的最后攻击牌，模拟器则跳过该复制，少16伤害及一次真实目标抽取。

单人 POD 历史改为保存最近两个发生过合格完成事件的玩家回合及各自最后 CardRef：同回合更新 latest，首次跨回合更新 previous。History Course 按精确 `TurnNumber - 1` 匹配；Dupe 和 Skill 完成不替换记录，没有攻击的上一回合也不会使用更早记录。两个引用和回合字段都参与 visitor、hash、snapshot 及验证，旧 schema 明确拒绝。没有加入 CurrentSide 条件：原版 `HappenedLastPlayerTurn` 只比较玩家回合号。

已有 CTest 扩展新回合自动牌覆盖、snapshot 恢复、Dupe 排除和空攻击回合。完整路线加入 `run_continuation_regressions.py`，其 native contract 必须实际观察 turn15→16、Hellraiser+Inferno+HistoryCourse、Twin Strike discard、原版 HP293→252 和目标 counter45→47，避免路线偏离触发场景仍算通过。

## 容量原件

另外三局为旧池上限，不归为已修复的规则差异：

| seed / 角色 / policy | 首个 C++ 错误决策 | 原版可见 pile 峰值 |
|---|---:|---:|
| F23CB437F1141FE / Ironclad / 1374733648 | 2777 / round440 | 2216 / round948，包含持续生成的 Anger/普通攻击牌 |
| F7442772E4EC03B / Regent / 1306041266 | 1078 / card_select | 1881 / round625，其中1872 Dazed |
| FEFE3B6F36DC37D / Regent / 328375808 | 1066 / round47 | 1000，其中978 Wound；另有 deck/internal 引用 |

这些只是原版已完成决策点的可见 pile 数，不能当作所有内部/瞬态活牌上界。外部冻结 0b 的 8192/快照预算/host-stack 候选独立诊断，不能当作当前 main 的生产容量资格。

## 验收状态

修复代码提交 `30e6f94e655f53abfe6d94c8317ec70f5c864a3f` 已完成验收。执行命令：

```sh
cmake --build build -j 4
ctest --test-dir build -R history_course_relic --output-on-failure
python3 tests/l4/l4_run.py --trace <原始失败oracle.stdout.jsonl> --verify-snapshots --require-complete --artifacts-dir <新输出> --result-json <新结果>
scripts/check.sh
```

`history_course_relic` 通过；未改的原始失败输入完整 **960/960** 严格一致，最终 victory 匹配。完整 `scripts/check.sh` 外层实际 EOF0，耗时248.249秒：54/54 CTest、L2 200、L3 81完整战斗1625状态加2探针4状态、L4 **111局25143/25143**，包括四条 continuation 原生回归4472/4472。gate stdout SHA256 `c5082c8e532841d24998444afc55feb199181d1494bae08aa1e41986e27ba241`；详细关闭证据 `history-course-gate-completion-evidence.json`。

修复版本对全部 **1000** 未改原件重新冻结重放，外层实际 EOF1，仅因三条原有容量拒绝：**997完整一致 / 0规则差异 / 3容量错误，1184425/1186866决策**。357个原版胜利终点全部完整匹配；其余完整匹配为 death@act2 168、death@act1 382、death@act0 90。三条容量错误的身份、首个索引、错误文本与上下文逐项等于9e3基线。source `206ef4f4d9b5df77f4f2de2f586f730dbbb973f1818561b435dba154b15ece99`；CLI `a28920d311ddc38358e75c9b61a1f1168ba65e53c5b3e7fcc34e446f68a8fdb2`；C ABI `9595ee196769e35989d7264d105a3679c671ff7e346f0ec8ec73860b7896f607`；manifest `b6f2f3208ebd6122cee8238f5c0b1a6d760ef1dbc26d5533e85d804c1a37db1d`。

重放外层耗时850.379秒，实际EOF后重新核验全部Git/source/files/dirty/runtime/环境/系统、C ABI及1000原件SHA。证据 `deep-v3-history-course-full1000-completion-evidence.json`，SHA256 `5ce1fcc2f376492d2b996ddee028a51e701bf86d5314b273fd461259f0dfdea9`。1997个已关闭CPP stdout/成功重复输入按SHA无损归档，节省3407528680字节；失败重复输入、原生stdout、动作stdin和先前结果保留。归档节省是本次输出相对未压缩的字节差，不能全部当成额外释放既有磁盘空间。

三条新增容量原件在同一个外部旧0b 8192/预算/host-stack候选实际EOF0：**3/3完整7362/7362**。allocated card峰2241/1894/1027，visible pile峰2216/1881/1000，active captures峰0/0/1；第一局自产snapshot观察到4501244字节/319279节点。此结果补充外部诊断，仍不提供当前main生产容量资格。

额外只读组件审计发现两个缺口，证据在本地 `active-capture-component-bound`：正版TestMode=false的32个独立Monologue实例×835张不同卡，真实BeforeCardPlayed字典接受26720个唯一pair；冻结30e C++在第26625个pair触发捕获数组容量，牌池只分配853。这里只直接调用组件方法，未证明合法整局835层嵌套可达，不能据此宣布自然整局捕获溢出。另公共local-v4导入在显式允许的state_hash=null时，接受捕获条目中的65535池外引用、零引用和重复pair，原Env保持不变。坏引用校验已交API对话处理，本次规则修复未宣称修复它；禁止以清空真实pending捕获代替校验。

完整 Env/RF、真实档案、live capture、模型所有条件、正常 HP 晚幕、全部合法动作集、双端 RNG draws、1万条零失败、生产容量和公平性能验收仍未完成。本项复用现有根目录，临时分支合并后删除，不新增工作树。
