# Q4-ac：冻结X1的公开结束回合头

**结束回合小头能拟合示范，但固定整局对照失败：两候选分别比X1少1.1445/1.2070层，全部3072局真实死亡、0通关。本项不扩训、不替换X1。** 原参数冻结，新增二分类头学习结束回合；局外和其他动作选择仍受原X1控制。固定新种子评价、独立核验和最终正常回放均已完成，实例已取回关机。高水平Silent A10/Linux实机NoSL目标未完成。

计划：[Q4-ac](../plans/2026-10-06-nosl-q4ac-endturn-gate.md)，基线main ae8f0b0c2ca580fbbb6c6535d72be4369ccc8564，实现/完整门禁源码734b426fd78daef515893f20d3ecf399ed2c6159；分支feat/nosl-q4ac-endturn-gate，artifacts/nosl-q4ac。旧Q4-ab已封存并确认关机，目录只读，不扩大旧阴性配置。

## 实际学习与合同

既有Q4-o公开教师缓存SHA 8d30a2c7e3ce51c7c0bb73551d062671a4892b5f5d3f241959864d0d74599f29。1248行中1088训练/28留出行符合公开Combat、有唯一EndTurn及其他合法动作：89/2正例，55/2根；不改原划分、不新增教师搜索。输入为冻结run向量及9个公开数字，详见计划；146→64→1小头。全部X1原参数/缓冲与价值冻结，原model.py/native/filter不改。固定128epochs、Adam .001、minibatch128、普通BCE/grad norm1，每seed1152个优化step；只保留最终模型和完整Adam，不按中途或留出挑选。输出为登记的贪心分层解码，不把其softmax解释成头的二分类概率。

```bash
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/check_endturn.py --start artifacts/nosl-q4r/start.pt \
  --data artifacts/nosl-q4o/cloud/labels --out artifacts/nosl-q4ac/qualification
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/verify_endturn.py learning --start artifacts/nosl-q4r/start.pt \
  --data artifacts/nosl-q4o/cloud/labels --qualification artifacts/nosl-q4ac/qualification \
  --out artifacts/nosl-q4ac/learning-audit.json
```

两命令实际退出0。cache、学习及合同17.280745秒，远低于登记240秒；两头学习本身分别0.711375/0.522573秒。E1训练正例84/89、负例998/999、balanced accuracy .971410；E3正例82/89、负例998/999、balanced accuracy .960174，均通过登记的拟合门槛。两留出正例均1/2，负例分别24/26、23/26；仅2根不能支持泛化结论。

2759教师/锚点公开行合同通过：原参数逐tensor相同，全部value及611个不适用行logit严格不变（含368局外行），2148适用行的旧非EndTurn贪心动作保持，padding忽略/合法mask保持，初始继续、正/负头控制、仅EndTurn/重复EndTurn/parked边界正确，CPU缓存vs实际forward严格一致，错误schema/baseline/非finite权重拒绝。现有generic loader明确拒绝新格式，不能静默丢弃小头。新格式只支持显式Python加载/公开推理；C++/Linux部署还未适配，不能直接宣称实机可用。

独立审计核对原始eligible/标签/根划分、129个固定epoch记录/步数、最终拟合、两保存权重SHA及完整Adam finite/moments/step1152和重新加载一致；不声称重放全部优化器更新。原X1参数digest 6996897c9904d11036f9c89ec00866b05981753e7a2a05a97cf35772ee4f8080。

E1 checkpoint SHA 563fa7fc93d029e26fe395e4f136f13bf176c62987ed8b1705cbb9d70fd209ad，Adam d26f9e370e12babac28170b8f8de9047f0fef71dd4af831785afd1dfa46959dd；E3 checkpoint 51be7593886af4e6cc05285270a8112cdfa130d84ad369bd1f06681251c65239，Adam 73a47b36084650d146453f5e47b470bf1d17336efdb6b5c3eeca250d15c193ff。首次本地学习/采样时HEAD仍是计划0339245、新源码未提交，qualification-source.json保存实际使用工作树源码SHA；随后原样提交734b426，不冒充计划提交已经含运行代码。

## 正常新局资格与门禁

```bash
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/endturn_experiment.py pilot --start artifacts/nosl-q4r/start.pt \
  --qualification artifacts/nosl-q4ac/qualification --out artifacts/nosl-q4ac/pilot
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/verify_endturn.py normal --experiment artifacts/nosl-q4ac/pilot \
  --out artifacts/nosl-q4ac/pilot-normal
taskset -c 4,5 nice -n 19 env STS2_BUILD_JOBS=2 scripts/check.sh
```

三命令实际退出0。两候选各相同4新NQ4ACPILOT000000–000003，8/8真实死亡，采样wall分别.519171/.522558秒；策略决策676/675，其中实际EndTurn16/15，平均14.5层，均未进第二幕。这只作正常路径资格，未设置X1配对实力对照，不用8局代替正式新种子评价。

8/8完整TestMode=false Oracle正常整局、1669/1669原版决策、逐步快照与全部end.run字段一致，failures=[]，无HP/牌组/遗物注入。完整scripts/check.sh在734b426实际退出0、末行all checks passed，2026-10-06 11:27:34.429988–11:30:40.856735UTC（纽约07:27–07:30）。检查后运行源码未改变。

## 正式固定评价与最终核验

本地拟合、独立审计、8条正常资格和完整门禁均通过后，按原计划X1/E1/E3各1024新NQ4ACEVAL整局greedy CUDA float32；最终正式X1不替换。

云启动记录：2026-10-06 11:32:04.679373UTC先确认shutdown；独立1800秒保护PID1335840于11:32:34.223241UTC启动并确认armed，原截止12:02:34.223241UTC，不延长。11:32:36.773894UTC API接受开机指令。远端原76f28f7/tracked干净/新目录不存在，16文件393972字节payload逐项SHA校验后以bundle切至39db908f13aa91941ca3fa7e08a013cc12d4b8bf（门禁734b426后只改文档）。实际launcher1237/1500秒、driver1200秒、自动回收PID1336028；11:34:13.948108UTC实际launcher alive、job.exit缺失，X1固定评价已有384真实terminal。这是当时活动状态，最终结果、取回及关机见下文。

只读故障状态诊断（后补范围已登记，不改正式判据）：旧Q4-ab冻结第190/200/256/511公开状态，来源SHA 3a089f98d564361e778d9094d61fdf24dda2faf2e99695b6cfb87d14b5e28bbc；X1四状态均选择PlayCard，两候选在护甲23的第190状态仍选择PlayCard，在护甲95/599/2894的其余三状态均选择EndTurn。E1头logit分别-4.0083/4.5973/3.6629/3.4053，E3 -3.5627/4.3968/3.6661/3.3801。这是固定权重对4个公开状态的推理，没有训练/新采样/搜索，不证明整个原故障路径已解决或整局实力提升。frozen-loop-diagnosis.json。


CUDA公开合同2759行检查实际退出0，字段范围/原参数/不适用logit/value/其他动作argmax与非法文件拒绝全部通过，不重训或挑权重。实际GPU为RTX4080、Torch2.8.0+cu128、Python3.12.3/NumPy2.3.2、CUDA12.8，全部float32/各1线程，cloud/provenance.json。CPU与CUDA使用各自实际轨迹，不声称逐位跨设备相同。

| 策略 | 平均层数 | 第二幕 | 第三幕 | 通关 | 策略显式EndTurn | 策略决策 |
|---|---:|---:|---:|---:|---:|---:|
| X1 | 18.408203 | 303/1024 | 0/1024 | 0/1024 | 2 | 234434 |
| E1 | 17.263672 | 243/1024 | 2/1024 | 0/1024 | 7001 | 222836 |
| E3 | 17.201172 | 239/1024 | 2/1024 | 0/1024 | 6645 | 221717 |

所有3072尝试均真实death，0故障/截断；显式EndTurn计数只统计策略决策，原生单合法自动推进不计入，不能解释为X1整个游戏仅结束两次回合。本项X1用新NQ4ACEVAL种子，与旧Q4-ab层数不构成相同种子的纵向强度比较。

以独立跑局seed为配对单位、10000次bootstrap/rng20261006，E1-X1平均差-1.144531、95%区间[-1.439453,-.848633]；E3-X1 -1.207031、[-1.506836,-.905273]。两候选第三幕各2、超过本项X1的0，但平均提升与区间判据均明确失败，gate_passed=false。每臂实际0/1024通关；双侧95% Wilson区间[0,.003737406]，不把观测0当总体胜率精确为0。局部EndTurn示范/故障状态选择改善没有带来本项整局净收益；更多显式EndTurn和整体退步相关，不能仅据计数认定所有退步首因或正确的全局结束回合频率。

```bash
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/verify_endturn.py evaluation \
  --experiment artifacts/nosl-q4ac/cloud/experiment --out artifacts/nosl-q4ac/verified-results.json
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/verify_endturn.py normal \
  --experiment artifacts/nosl-q4ac/cloud/experiment --out artifacts/nosl-q4ac/final-normal
```

两命令实际退出0。独立从3072原始流核对种子/结束/全部计数、原登记选择/统计；closure-verification.json另用独立状态枚举和分块bootstrap复算两个区间，完全一致。cloud两个小头文件与本地固定最后权重逐字相同，X1与云native及原参数保持冻结；完整门禁734b426之后运行源码没有改变。

最终候选登记首2局+最深局，E1/E3均为NQ4ACEVAL000000/000001（8层死亡）及000440（分别第三幕45/48层死亡）。**6/6完整正常整局、1909/1909原版决策、逐步快照与全部end.run字段相同**，TestMode=false、无资源注入、failures=[]。加本地资格8/1669，合计14次完整回放执行/3578决策；含重复seed，不是14个独立强度样本，不扩大为全部3072正式轨迹均经过Oracle。4个故障状态只读诊断另列，不计完整回放。

launcher1237实际终止，job.exit=1是固定门槛阴性，driver退出记录存在。自动回收对同一关闭目录只打包一次：37文件、3926536原始字节、978801归档字节，SHA **a0c105d79e84e0a61466e1d69757f3b3829fc80f1963fe47761e19517e19d43d**；单次传输实际退出0，无续传超时，11:40:46.562250UTC全部归档/文件SHA与大小验证。**2026-10-06 11:40:56.178026UTC（纽约07:40:56）首次确认Success/shutdown**，同时间解除原保护，11:43:20.991900UTC实际/proc核对保护/回收进程均已终止，shutdown-verification.json。原1800秒截止未延长。

开机指令接受至首次确认关机499.404132秒，即8分19.404132秒；这是可观察开机窗口，非供应商最终计费金额。三臂实际采样wall合计368.848501秒。44个10秒资源样本，driver/子进程RSS采样峰值1220.027344MiB、GPU内存414MiB、GPU利用率峰值10%/样本均值5.363636%；均为采样值，不冒充连续绝对峰值。final-stats.json。

本项完成固定学习、完整正常评价/核验和回收关机，结果阴性。**不扩大此教师小头配置、不替换X1、不宣称高水平或Linux实机可用。** 下一项应从实际收益检验结束回合与继续行动的选择，补充对应状态分布的可靠标签/学习信号；本项尚未建立全部退步的因果解释。产品目标仍按原完整需求继续。

最终证据于2026-10-06 11:47:36.603668UTC封存：263文件、17216636字节，manifest SHA **65a97ceebdd214c6d12417df1e418a6d0968afc9a84fa0ed510129bfe3f08530**；生成后逐文件SHA/大小复核，目录不再写入。旧Q4-z/aa/ab的372/74/699文件也只读逐项复核通过。README/roadmap只暂存本项新增前缀，其他对话字节保持，CLAUDE.md删除及两份性能文档未暂存或覆盖。
