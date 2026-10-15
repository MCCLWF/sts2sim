# Q4-ad：真实终局收益的结束回合学习

**数据和学习合同通过，两个固定候选未共同达到拟合门槛；不追加训练，不启动正式强度评价，不替换X1。** 这轮用同一真实状态的EndTurn/继续两条完整终局轨迹提供学习信号。C1训练balanced accuracy .802665、C3 .782188，C3正例召回.677419低于登记.7；留出分别.565341/.613636，未显示可靠泛化。高水平Silent A10整局及Linux实机NoSL目标仍未达到。

计划：[Q4-ad](../plans/2026-10-06-nosl-q4ad-endturn-credit.md)。起点main d7727f2ce1541f8f5b3ef4e7090351d7cd31fc84，分支feat/nosl-q4ad-endturn-credit；采集源码5a84639f7caa3e6622277ace037b4a30d2a1aca0，学习源码79afc81e3a9a62329abbfe14ece4e3bc9c66e1d0，独立审计源码f17e11e。产物artifacts/nosl-q4ad；旧Q4-z/aa/ab/ac只读。用户本轮新增“尽量少使用云 GPU”，在候选训练/正式评价之前登记本地CPU float32；原样本、学习与强度门槛不变，本项没有开机或GPU训练。

## 资格及打包修复

首次NQ4ADPILOT000000–000015固定16尝试在5.250405秒得到13完整paired roots、3未到root正常死亡；52次初始fork、3081次源不变、6162次同动作兄弟一致检查通过。末尾np.stack因真实公共token/action长度不一致报ValueError。原始目录与失败日志保留，修复为公共表按counts补齐，**从原13个冻结root快照恢复，不重采样、不换种子**；packing-recovery.json记录原18文件SHA与大小，roots记录只增加public摘要，原奖励和结果逐字保留。后续正式采集使用修复后的已提交源码。

```bash
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B artifacts/nosl-q4ad/repair_qualification.py
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/check_endturn_credit.py audit --start artifacts/nosl-q4r/start.pt \
  --heads artifacts/nosl-q4ac/qualification --data artifacts/nosl-q4ad/qualification-repaired \
  --out artifacts/nosl-q4ad/qualification-policy-audit.json
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/check_endturn_credit.py normal --data artifacts/nosl-q4ad/qualification-repaired \
  --out artifacts/nosl-q4ad/qualification-normal
```

三命令实际退出0。13个源公开状态/贪心及X1最佳非EndTurn选择、3343步原始奖励独立复算通过。**26/26完整TestMode=false正常整局、6380/6380原版决策、每步快照与全部end.run字段一致**；没有HP/牌组/遗物注入，全部真实死亡。独立新局前缀及第一分支动作的全部公开表/步数/status一致。第一个固定真实root的原生低步数两分支均STEP_LIMIT，实际pair_target输出complete=false/delta=null/label=null/weight=0，不伪造死亡或学习标签。

## 固定采集与真实标签

```bash
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/check_endturn_credit.py collect --start artifacts/nosl-q4r/start.pt \
  --heads artifacts/nosl-q4ac/qualification --out artifacts/nosl-q4ad/data
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/check_endturn_credit.py audit --start artifacts/nosl-q4r/start.pt \
  --heads artifacts/nosl-q4ac/qualification --data artifacts/nosl-q4ad/data \
  --out artifacts/nosl-q4ad/data-audit.json
```

512固定NQ4ADTRAIN尝试完成，450完整paired roots、62未到root正常死亡。全部900分支真实死亡，0故障/截断；251.465590秒，未超过600秒，不补种子。采集原会话后来已结束且句柄消失，没有可追溯的PTY退出码，不冒称记录退出0；完整最终summary/NPZ及原始流存在，独立audit实际退出0。完整900条尚未全部Oracle认证；正常路径资格范围仅上述26条。复制分支共享root，不算900个独立强度样本，也没有X1整局评价对照。

450公开root/source选择、123623个分支动作及逐步奖励独立复算通过；另有38372个唯一root前缀动作，含前缀的两分支记录共200367动作。奖励含第一次分支动作和真实终点；同root预先发生的Boss损血不重复支付。两return之差使用原1e-5 tie阈值，224对近似tie，226有方向；固定按整个seed留出。

| 数据 | 全部完整root | 有方向 | EndTurn更好 | 继续更好 |
|---|---:|---:|---:|---:|
| 训练 | 352 | 177 | 62 | 115 |
| 留出 | 98 | 49 | 16 | 33 |

登记数据数量/两类数量/零分支故障门槛全部通过。public-roots.npz SHA 0cc7ca9267b631dac95997ca1b85a1064e7ced893f0232ca8d6d926986a3c557。只对实际隐藏世界及固定E1/E3接续比较，不宣称公开最优Q；reward不是actor输入。

## 两个固定最终头及独立核验

```bash
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/check_endturn_credit.py train --start artifacts/nosl-q4r/start.pt \
  --heads artifacts/nosl-q4ac/qualification --data artifacts/nosl-q4ad/data --out artifacts/nosl-q4ad/learning
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/verify_endturn_credit.py --start artifacts/nosl-q4r/start.pt \
  --heads artifacts/nosl-q4ac/qualification --data artifacts/nosl-q4ad/data \
  --trained artifacts/nosl-q4ad/learning --out artifacts/nosl-q4ad/learning-audit.json
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/check_endturn.py --verify-only --start artifacts/nosl-q4r/start.pt \
  --data artifacts/nosl-q4o/cloud/labels --out artifacts/nosl-q4ad/public-contract \
  --candidates artifacts/nosl-q4ad/learning/C1/checkpoint.pt artifacts/nosl-q4ad/learning/C3/checkpoint.pt
```

三命令实际退出0；train退出0表示流程完成，不表示fit gate成功。公开146→64→1结构及整个X1原参数冻结，从原固定E1/E3初始化fresh Adam，lr .0003、128epochs/minibatch128/gradnorm1；有方向训练root的margin权重BCE，加.1×全部352训练root初始Bernoulli KL（含tie）。每seed256更新，只保存最终头/完整Adam和129个epoch记录，不按留出/中途选模型。缓存加两个头总1.533768秒；两个学习过程含保存分别.475168/.248695秒。

| 候选 | 初始训练balanced acc | 最终训练balanced acc | 正/负召回 | 留出balanced acc | 留出正/负召回 | fit gate |
|---|---:|---:|---:|---:|---:|---|
| C1 | .532118 | .802665 | .709677/.895652 | .565341 | .312500/.818182 | 通过 |
| C3 | .544530 | .782188 | .677419/.886957 | .613636 | .500000/.727273 | 失败 |

C1训练正例44/62、负例103/115；C3为42/62、102/115。留出正例5/16、8/16，负例27/33、24/33。两个初始留出balanced acc .580492/.612689，最终没有一致改善；49root有限样本不证明普遍退步或因果首因。初始锚点KL均0，最终.481273/.508716；这不是整局策略KL统计。

独立审计从原始分支奖励重算label/split/weight，公开cache与实际forward共900检查严格一致；冻结params/value、非EndTurn排序/mask通过。旧只读教师/锚点2759行公共合同另通过（2148适用、611不适用含368局外），全部原tensor和value/局外logit逐位不变、padding/控制/边界及错误schema/baseline/nonfinite/generic-loader严格拒绝。独立核对全部固定epoch/步数、初始和最终指标、完整Adam256step、finite/moments及载入roundtrip；不声称重放所有优化更新。

C1 checkpoint SHA 7ddb858972f156fcc6217a7dd8fc74095a2f62934ac44238e2cea46da8d35535、Adam 6bbe6a4234d018cd89afd6bacc7a330c8507de6feecae3519944c7f2c879a551；C3 checkpoint 9f936085ede143ffcc979aa74abccf0c528497bb0866000cbc252bb87f3f6eb5、Adam f93d06032e2c43f9c1a75a951e0f8aa98d4221e9480267beb12d196abd9907d0。冻结X1参数digest 6996897c9904d11036f9c89ec00866b05981753e7a2a05a97cf35772ee4f8080。

## 收尾与未验证范围

本项联合fit gate失败，按原登记停止后续post资格/5×1024正式评价，不实现或运行尚未需要的endturn_credit_experiment.py；没有新候选整局强度证据、不扩大此配置、不替换X1。新格式仍只支持显式Python加载，不能宣称Linux实机已经可用。当前信号能改善训练拟合，但公开表示/标签噪声/跨状态泛化仍需新的独立实验；下一个可检验问题是“结束回合头是否缺少它正在与之比较的具体继续动作信息”，这轮未证明该假设。

云pro-7912450e5767在2026-10-06 12:09:04.458264及13:23:27.187150UTC只读API确认Success/shutdown；本项无power_on、无云训练/评价，云使用时间0。旧工具只读复制到本项路径避免改写封存证据。工作树CLAUDE.md删除、另外聊天的README/roadmap段落及两份性能文档保留，不纳入本项提交。

完整门禁：`taskset -c 4,5 nice -n 19 env STS2_BUILD_JOBS=2 scripts/check.sh` 在f17e11e69654af670abd7ac0dd0b6ea50762ca29实际退出0，日志末行为all checks passed。2026-10-06 13:24:38.246258–13:29:31.911058UTC（纽约09:24–09:29）；门禁后仅改文档，运行源码保持。旧Q4-z/aa/ab/ac的372/74/699/263封存文件逐项SHA/大小复核通过。本项在封存证据后fast-forward合入main；README/roadmap仅暂存本项新增前缀，其他工作树字节保持。

最终证据2026-10-06 13:30:18.109608UTC封存：352文件、26178237字节，manifest SHA **724d51d67ed89761939fd2d90477e20fc1cb431bafb7393361b6fb3ef2878878**，逐文件SHA/大小复核通过，目录不再写入。失败原始日志、恢复前后数据、900原始完整分支、最后权重/Adam、正常资格和主线门禁均保留。
