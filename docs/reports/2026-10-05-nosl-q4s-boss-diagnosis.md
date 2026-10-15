# NoSL Q4-s：正常规则第一幕Boss诊断

计划：[固定32次诊断](../plans/2026-10-05-nosl-q4s-boss-diagnosis.md)。基线main `fe8e9a6`；正式采集源码 `848478e3c4111c4d643d03ea6e82031348d5ac64`，工具 `tools/nosl/boss_diagnosis.py`、`student_roots.py`。原始产物 `artifacts/nosl-q4s/`。

**正常规则学生轨迹全部核对通过，Boss教师学习的扩采门槛失败，不训练新候选、不替换X1。** 32个固定训练种子中20个到达第一幕Boss，学生在这20场中13死/7胜；教师8条路线真实执行合格，仅2条挽救学生死亡，低于预设至少16条合格路线及8/32实际挽救的门槛。全部32局的学生最终都死亡；单场Boss胜利不是整局胜利，高水平Silent A10整局/Linux NoSL仍未实现。

## 输入、版本与实际执行

X1 SHA256 `6fb5f1955a11d55175d348659460ce459017baa018f7cf9242a671ba19266703`，features3、actor filter1、公开三表greedy动作、power_context关闭。仅用新训练种子NQ4S000000..000031，没有读取Q4-r评价种子或按死亡类型选根。32局采样5.228秒、6765个策略决策，32正常death、0采样故障。

每个冻结脚本在正版v0.111.0、全解锁、Silent A10、TestMode=false重放；第17层首个combat决策捕获根，20个根均为run.room=Boss。12个未到根的种子都在第17层前正常死亡，也独立验证完整原版轨迹，不从32次分母中排除。

原始CombatSolver0.50.0固定最新主线 `9a4489d840a3772aef451d5c6dc42dd79719f380`，beam30/nodes10000/软10秒/DOP1、每宿主90秒。每种子一次搜索，预测完整存活胜利才一次真实路线回放，失败不重跑、不调整预算。搜索前后根及continuation/live stamp核对；搜索后继续执行原学生脚本，整局终点/资源/RNG严格与Env比较。

32/32正常规则完整学生轨迹一致，共8389个原版决策；6765个策略决策与8389个原版决策不同，后者还包含强制流程。20场搜索累计74.275秒、51319展开节点；13条预测完整胜利，8条通过真实路线及Env前缀/终点/RNG比较，7条预测不完整、5条执行失败。8条合格路线含1916个原版前缀决策；前缀在不同验证中重复，不能解释成独立样本数。

采集阶段结束6.374秒、教师batch阶段结束189.754秒，另执行12条无根轨迹核验；外层1200秒上限未触发。全部尝试保留，没有学新权重、导出新模型或追加独立dev/test。

## 实测Boss结果

下表按实际入场敌人分类；The Kin是两只Follower与Priest。数量少，仅描述本批，不作为六种Boss总体难度排行。

| 入场敌人 | 根 | 学生死 / 胜 | 教师真实路线合格 | 实际挽救 |
|---|---:|---|---:|---:|
| Ceremonial Beast | 2 | 1 / 1 | 2 | 1 |
| Vantom | 3 | 2 / 1 | 0 | 0 |
| Waterfall Giant | 2 | 1 / 1 | 1 | 0 |
| The Kin | 6 | 6 / 0 | 0 | 0 |
| Soul Fysh | 4 | 2 / 2 | 3 | 1 |
| Lagavulin Matriarch | 3 | 1 / 2 | 2 | 0 |
| 合计 | 20 | 13 / 7 | 8 | 2 |

8条合格真实路线的同场Boss战后HP如下，未将5条失败路线的预测HP计作收益。

| 种子后六位 | 学生HP | 教师HP | 差 | 挽救死亡 |
|---|---:|---:|---:|---|
| 000000 | 0 | 25 | +25 | 是 |
| 000006 | 14 | 11 | −3 | 否 |
| 000013 | 8 | 24 | +16 | 否 |
| 000015 | 20 | 36 | +16 | 否 |
| 000016 | 12 | 9 | −3 | 否 |
| 000019 | 17 | 19 | +2 | 否 |
| 000021 | 20 | 46 | +26 | 否 |
| 000029 | 0 | 5 | +5 | 是 |

5条至少+3HP，2条比学生少3HP。这不是公开信息最优上界，也不能由7条搜索未胜推出原版构筑不可赢。单世界教师有特权未来、预算和启发式限制，且5条预测胜利仍没有真实执行资格。

合格8条的学生与教师战后药水槽均为空；入场000000携Ghost in a Jar、000019携Poison Potion、000021携Block Potion、000029携Gambler's Brew，其余无药水。完整槽位/牌组/遗物、所有20个学生Boss终点及教师状态在 `diagnosis/boss-results.jsonl`，不只保留教师合格的学生结果。

## 两个已定位的采集首因

四条NQ4S000001/000017/000026/000028明确报 `Teacher action has unconsumed choices`。最后动作分别Hidden Daggers、Survivor、Hidden Daggers、Hidden Daggers；出牌前手牌数量分别3、2、3、3，牌离手后候选分别2、1、2、2。原版 `CardSelectCmd.FromHand` 在非手动确认且候选数<=MinSelect时直接返回全部候选，不调用ICardSelector；Hidden Daggers取2，Survivor取1。本回放器只在选择回调消费教师token，所以自动分支留下队列。此时不能清空队列或伪造选择；后续须观察原版实际自动结果并逐身份验证。

NQ4S000008报 `Teacher real terminal differs: actions=40/65, choices=0, hp=24/1`。原版尚未执行完整教师路线就输出max_decisions：冻结学生script共有303个动作，真实回放也恰为303个决策。`OracleRun.RunOneCore` 将脚本最大决策数设为script.Count；教师替代动作不消费原学生动作游标，却继续增加决策计数。这里有25个计划动作尚未执行，不能称真实游戏与教师的最终HP分歧。后续应给替代路线独立有界执行预算，保持原版真实结束、完整动作消费、资源/RNG比较，不能直接放宽终点判据。

7条不完整搜索和5条接口失败分别保留。默认batch把12个no_root归在failed，Boss checker另报no_root=12且32条学生正常规则轨迹合格，不将早死伪装成采样故障或删除。状态原表为route_qualified8/failed17/incomplete_route7；本批门槛固定为8/32实际挽救、至少16条合格路线，结果为2/32、8条，失败。

## 实现、验证与停机状态

student_roots batch新增min-floor选项，默认仍8；Boss driver锁定32种子、版本、预算，拒绝覆盖已开始的实验，不改规则、actor或PPO。完成后补齐了所有正常规则Boss根的学生胜/死统计；只重读已有数据和核验无根轨迹，没有重复搜索/采样，原先报告的字段及门槛逐项相等。

```bash
DOTNET_CLI_TELEMETRY_OPTOUT=1 nice -n 19 taskset -c 4,5 /home/wst/.dotnet/dotnet build tools/NoSLTeacherHarness/NoSLTeacherHarness.csproj -c Release --no-restore -m:2
STS2_BUILD_JOBS=2 nice -n 19 taskset -c 4,5 scripts/check.sh
PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 nice -n 19 taskset -c 4 timeout --signal=INT --kill-after=30s 1200s .venv/bin/python tools/nosl/boss_diagnosis.py --checkpoint artifacts/nosl-q4r/start.pt --out artifacts/nosl-q4s/diagnosis --mode run
PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 nice -n 19 taskset -c 4 .venv/bin/python tools/nosl/boss_diagnosis.py --out artifacts/nosl-q4s/diagnosis --mode verify
```

准备首条命令的taskset参数缺空格，尚未启动构建/采样；错误日志 `harness-build.log` 保留。修正后宿主Release构建3.11秒、2警告、0错误。另修正文档Q4-q/Q4-r中的同类命令拼写，不改变原始实验结果。完整门禁60/60 CTest、L2/L3/L4及固定整局路径通过，末行all checks passed（`check.log`）；最终工具py_compile和实际checker执行通过。后续仅统计/文档变化，C++/Oracle源码未变，不重复已通过的相同游戏门禁。

本批32条正常规则学生与8条教师前缀独立列出，既有其他默认TestMode门禁不能据此改称正常模式全覆盖；本项不证明任何新学生或实机路径。原生库SHA256 `44e8a0bd2737c2304799240f42ffc6493f2eb428bef8377c1bcb65500b4615b6`；原始solver DLL为 `6fb76f3bd2465db90e69aed4162d5174358780196513d418ff4bd54cde286df4`，宿主为 `765c048f1dda977ddaa14b6ec39b8ef61612b9febae047324b565331d3f5e4e3`。

本项本地nice19/单CPU诊断，没有启动云GPU；执行前21:51:28 UTC复核实例pro-7912450e5767为Success/shutdown，结束后22:03:27.659906 UTC的 `cloud-status-after.json` 再次确认。416个产物文件共18,244,264bytes，大小/SHA256逐文件校验通过，清单为 `artifact-manifest.json`（清单自身不计入）。所有原始成功/失败和complete marker保留。保留另一chat未提交性能文档/段落及用户CLAUDE.md删除。

下一项优先修复并用这些冻结失败路线核验替代路线预算和自动选择消费，保持本批阴性结果；再按独立学习计划决定是否采集Boss示范或研究构筑。不能因局部挽救而宣布高水平NoSL完成。
