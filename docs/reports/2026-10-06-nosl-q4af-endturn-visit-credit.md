# Q4-af：实际访问分布与单动作信用报告

状态：本地采集、独立核验、原版12条完整干预回放及完整主线门禁通过。登记的主要机制优先判据失败；没有训练新权重、云GPU使用或模型替换。高水平Silent A10完整NoSL及Linux实机目标未完成。

## 问题与实际前置

前置main `5532e07f77a70ff81afeb565f36cf4af303efd1e`。Q4-ae候选C1/C3比匹配B头和X1都差，不能把训练拟合更好当强度提升。本项按[固定计划](../plans/2026-10-06-nosl-q4af-endturn-visit-credit.md)使用四臂各前128条**旧开发种子**`NQ4AEEVAL000000–000127`；参考为对应初始Q4-ac E1/E3，而非X1。只研究实际访问/第一次动作改变，不作新留出胜率测试。

原模型、游戏、原生库、奖励与旧封存产物未改，actor仍只读四公共表。开始采集时逐文件验证Q4-ac263/Q4-ad352/Q4-ae360封存文件和manifest SHA；实际头、原1024轨迹文件SHA绑定config。新离线工具持有Env克隆、种子和未来奖励，不进入部署。新源码为`tools/nosl/endturn_visit_credit.py`及独立`verify_endturn_visit_credit.py`，没有改生产模型/加载器。

## 失败记录与数值合同修正

首次采集在`ef10debf3a8385d7fdeab83448f57e86b647d9fd`启动PID1369798，CPU4/nice19，2026-10-06 14:21:28.623267–14:21:30.298989UTC，实际退出1，wall1.675722秒。失败为B1种子000015决策11，32批次选择商店action3，原64批次记录action2。没有完整pair/强度结论；`data/`、collect.log/exit和failure-replay原样保留。

独立从同一正常种子的前11个原动作复现公开状态SHA `63d14750633b1724e2bcac5cda14960e491564ad82afbe585c16cc0860d184a8`，测试1/32/64批次，完整forward与快速路径均一致。两商店action2/3：batch32分别4.638328075/4.638328552（相差约4.77e−7）；batch64两者4.638327599相等，argmax选择较早row2。batch1也选择2。公开状态相同，差异来自浮点批计算，而非环境重放或快速路径错误。

在任何干预终局结果出来前，执行计划明确修正：原轨迹按原batch64重现；每根建立两克隆，首动作分别为原实际更新头与初始E头选择，**两分支后续均固定逐局CPU推理同一更新头**。这保证后续动作计算只依赖该分支公开状态，不受其他slot的死活/形状影响。未改生产近tie行为，未以分数容差放宽游戏比较。新采集仅写`data-v2/`，仍四臂128/900秒，不扩种子。

## 实际采集与资格

```bash
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/endturn_visit_credit.py collect --out artifacts/nosl-q4af/data-v2
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/verify_endturn_visit_credit.py audit \
  --data artifacts/nosl-q4af/data-v2 --out artifacts/nosl-q4af/audit.json
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/verify_endturn_visit_credit.py direct \
  --data artifacts/nosl-q4af/data-v2 --out artifacts/nosl-q4af/direct-audit
```

采集source `794f3a1bc26edf6949ed83f0c7d929d1be1e826f`，实际PID1370535于14:25:25.367264–14:30:47.338089UTC执行、退出0。采集内部wall320.849702秒、宿主wall321.970829秒，全部512原轨迹终局与原八个run_info字段/动作完全相同，59915/59915原策略决策逐步公开推理一致。记录43003个适用访问状态；510局有root（B1/B3各127、C1/C3各128），两局无分歧原样保留。1020/1020克隆在父原轨迹实际终止后仍与原root全部输出buffer逐位相同，证明没有随父状态推进而改变。

所有1020干预分支均实际死亡，0故障/截断/通关。CPU用户/系统317.974737/3.873623秒，kernel child maxrss666.824219MiB（sampler自报656.5625MiB，两种进程范围）；没有GPU、模拟注入、替代种子或补跑。root公共NPZ SHA `f243c2ca461bf5c4e0228a43684e52f3ffe48dc2de7432ce35a48ee7d202aedb`。

两独立核验实际退出0：从原文件/visits重建每seed首次分歧、原前缀、完整终局，510根重新公开forward精确核对EndTurn-vs-继续决策，108715步奖励浮点算术逐值相同，root终局差与配对一致；未用近tie非EndTurn row的跨批逐位分数支持宽泛数值保证。另对每臂首4个**按采集发现顺序记录的root**，各从新局独立重放两首动作，16根/32个完整root及首后继buffer一致。未把510根全轨迹都作原版认证。

原批轨迹继续batch64，两克隆继续batch1；更新首动作分支完整轨迹与原轨迹一致B1/C1/B3/C3为92/81/88/84，共345/510。其余165条存在数值执行形状影响，**不能用克隆均值减原批均值归因于干预**。以下效果只比较同根、同逐局后续计算的两克隆。

## 访问覆盖与单动作结果

Q4-ad352训练root中floor1–3为0、4–9为183、10+为169；其最小层为4。不是泛称训练没有早期战斗，而是这轮二值头训练/约束root没有第1–3层；X1/E旧训练另有数据。

| 头 | 首分歧root | 首分歧floor1–3 | 实际适用访问 | 相对初始E改变动作的访问 |
|---|---:|---:|---:|---:|
| B1 | 127 | 114（89.76%） | 10476 | 1208 |
| C1 | 128 | 110（85.94%） | 10773 | 1317 |
| B3 | 127 | 114（89.76%） | 10807 | 1176 |
| C3 | 128 | 113（88.28%） | 10947 | 1301 |

访问次数不是独立seed样本；同一局多个状态会重复计算。实际首分歧主要在floor2，B1/C1/B3/C3分别111/103/110/105根。新头并非仅结束得更早，首分歧有两种方向；初始继续→新头结束分别90/84/99/96根，新头结束且公开energy>0分别88/82/95/93根。剩余energy本身不证明结束动作错误。

| 头 | 新头首动作后续均层 | 恢复一次E首动作后续均层 | 配对层数差 | 10000次bootstrap95%区间 | 正/负/同层 |
|---|---:|---:|---:|---|---:|
| B1 | 9.763780 | 10.425197 | +.661417 | [.196850,1.173228] | 19/6/102 |
| C1 | 9.710938 | 9.984375 | +.273438 | [−.234375,.820313] | 20/16/92 |
| B3 | 9.976378 | 10.496063 | +.519685 | [−.165354,1.236220] | 21/9/97 |
| C3 | 9.960938 | 10.164063 | +.203125 | [−.351563,.757813] | 21/19/88 |

配对单位是同臂一个旧run seed；bootstrap固定rng20261006、10000次。样本均为实际完整终局，没有剔除不利pair；root下未发生首分歧的2条另报，不纳入不存在的干预。全部1020分支0胜/0第三幕；更新/恢复一次E分别第二幕B1 10/13、C1 5/4、B3 7/6、C3 7/7。不存在新整体策略胜率提升。

既有gamma1/floor50/HP.2/70/boss.3真实后续回报配对均差及95%区间：B1+.021505 [.005085,.039478]；C1+.004489 [−.009713,.019044]；B3+.014098 [−.009234,.037825]；C3−.000147 [−.017389,.016127]。原生死亡才有terminal标签；此处单世界、固定继续结果不是公开最优Q，也没有隔离所有后续随机因果。

预先登记“至少3/4臂多数首改变在floor1–3，且两C恢复一个旧动作的均层差/95%下界均正”门槛**失败**：前半成立，两个C区间都跨0。早期覆盖缺失已证实存在，尚不能认定为整局退步的主要机制；不据这个诊断扩大原配置训练。

事后描述统计（不改变上述判据）：B1/C1/B3/C3在352训练root的初始→最终Bernoulli KL为.546694/.634940/.551959/.615474，在原实际访问状态的均值为.232244/.258688/.220707/.256168。这不是贪心动作分布KL、不是完整整局策略KL，更不能因配置中有`.1*KL`就称更新幅度小。平均训练根KL比访问均值大，证据不支持简单声称只因缺少早期锚点才发生大变化。

## 正常原版与完整主线门禁

```bash
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/endturn_visit_credit.py normal \
  --data artifacts/nosl-q4af/data-v2 --out artifacts/nosl-q4af/normal
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/verify_endturn_visit_credit.py audit \
  --data artifacts/nosl-q4af/data-v2 --out artifacts/nosl-q4af/audit-with-descriptive.json
taskset -c 4,5 nice -n 19 env STS2_BUILD_JOBS=2 scripts/check.sh
```

三个命令实际退出0。第二次审计仅加入上述事后描述统计，并要求全部root实际终止才能满足优先门槛；没有改采集、权重或原判据。第一次审计原件不覆盖。

正常回放按预先固定每臂首次2个记录root+最深干预终局（去重）+所有胜场（实际无），共12次，wall30.324837秒，于2026-10-06 14:32:36.223614UTC终止。四臂前2root均000002/000011、终局floor17；最深B1/C1/B3/C3分别000031/000054/000090/000073，floor33/25/33/33。各臂945/873/946/980原版决策，合计**12/12完整正常TestMode=false、3744/3744决策、逐步快照和全部end.run字段一致**，failures=[]。独立重新从raw选择验证身份、无HP/deck/relics注入、每条完整终局无first_diff/native_error。这是12次执行，跨模型重复种子；不是12个独立质量样本，也不是1020条全部Oracle认证。

完整门禁source `8ddf83aa9395b298a93a31f38675d299dbfb747e`，2026-10-06 14:35:30.227018–14:40:15.892518UTC，wall285.665512秒，实际退出0，check.log末行all checks passed。之后只改文档/进度记录，没有运行源码改变。

云pro-7912450e5767于2026-10-06 14:37:10.413528UTC只读API再次确认Success/shutdown。本项从未power_on、SSH部署、云训练或云评价，云使用时间0；采集PID1370535实际终止且/proc句柄缺失。保留外来CLAUDE.md删除、README/roadmap的另项段落及两份性能文档；stage只写本项自己的进度前缀，旧封存目录保持不变。

本项完成诊断后按完整门禁合入main，保留X1。下一独立问题应比较相同数据/成本下受约束的小幅更新能否保留整局质量，明确记录实际策略变化，而不因分类拟合不足放任过大的改动；样本选取分布与新状态覆盖也要单独比较。这个建议未实施，不能写成优化已经成功。公开记忆、完整动作信用和Linux成品资格等仍缺；最终高水平目标没有因诊断合同通过而缩小或完成。

证据于2026-10-06 14:42:42.419725UTC封存：`artifacts/nosl-q4af/manifest.json`列出171个文件、27,917,612字节，manifest SHA256 `986a1fa05031cc364d5b05a67ab26f47e6985746eea260a0d9d136e7d8fe871b`。封存时全部新文件大小/SHA及Q4-ac/ad/ae旧封存文件再核对通过；封存后目录只读，下一轮使用新目录。
