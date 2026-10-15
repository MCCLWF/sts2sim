# Q4-ae：公开继续动作上下文与同容量对照

**正式配对评价失败：动作上下文C1/C3比同容量B1/B3低.6191/.9561层，比X1低8.6611/8.8926层，四个95%区间均为负；全部5120局真实死亡、0通关/0故障。** 训练拟合提升没有转化为整局收益，不扩大此配置、不替换X1。本项全部本地CPU、没有云开机；高水平Silent A10整局及Linux实机NoSL目标尚未完成。

计划：[Q4-ae](../plans/2026-10-06-nosl-q4ae-endturn-action-context.md)，起点main 836586d6549981a0cda66d98bc086cb206d36d41，分支feat/nosl-q4ae-endturn-action-context；学习源码3d2d6ded28c5b0dfa2424bd509569228a97daa4f，pilot源码b2b2e0918d296c7f547a54523a042c93b00410d5，完整门禁/审计源码1af7106。本项全部CPU4/nice19/float32、Torch/OMP/MKL/OpenBLAS各1线程，不开云实例；旧Q4-z/aa/ab/ac/ad只读。

## 固定公开输入与实际学习

复用封存Q4-ad450个真实root、352训练/98开发留出；177/49有方向，训练正/负62/115、留出16/33，224近似tie。public-roots.npz SHA 0cc7ca9267b631dac95997ca1b85a1064e7ced893f0232ca8d6d926986a3c557，manifest SHA 724d51d67ed89761939fd2d90477e20fc1cb431bafb7393361b6fb3ef2878878及全部352文件逐项复核。原reward/label/seed划分/margin权重不改，不新采数据。该留出已在旧实验看过，只作开发诊断，不是未见最终测试。

原公开结束头146→64→1，加274→64→1残差；B/C同为27138个学习参数、同seed初始tensor逐位相同。残差输入为原run128+9公开数字numeric18，再加X1当前最佳合法非EndTurn动作的公开action编码128；B将后者恒置零，C读真实公开编码。类型、source、target、number都由四公开表和冻结X1推理得到，没有未来模拟/搜索。残差最后层零初始化，初始输出严格复现旧E1/E3；整个X1原参数/价值冻结，仅两个小头学习。部署没有隐藏未来、seed、回报或source分组输入。

```bash
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/check_endturn_action.py train --start artifacts/nosl-q4r/start.pt \
  --heads artifacts/nosl-q4ac/qualification --data artifacts/nosl-q4ad --out artifacts/nosl-q4ae/learning
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/check_endturn_action.py contract --start artifacts/nosl-q4r/start.pt \
  --heads artifacts/nosl-q4ac/qualification --data artifacts/nosl-q4ad --trained artifacts/nosl-q4ae/learning \
  --out artifacts/nosl-q4ae/final-contract
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/verify_endturn_action.py learning --start artifacts/nosl-q4r/start.pt \
  --heads artifacts/nosl-q4ac/qualification --data artifacts/nosl-q4ad --trained artifacts/nosl-q4ae/learning \
  --out artifacts/nosl-q4ae/learning-audit.json
```

三命令实际退出0。缓存、初始完整合同和四模型固定学习合计21.693311秒，在登记180秒内；每模型128epochs/minibatch128、256优化step、fresh Adam .0003/gradnorm1。同seed B/C随机顺序完全相同；margin权重BCE均值+.1全部352训练root初始Bernoulli KL，包含tie。没有类重权/过采样/中途或留出选权重，仅最终checkpoint、完整Adam和129个epoch记录。

| 模型 | 训练balanced acc | 训练正/负召回 | 开发留出balanced acc | 留出正/负召回 | 训练fit gate |
|---|---:|---:|---:|---:|---|
| B1 | .824404 | .709677/.939130 | .642992 | .437500/.848485 | 通过 |
| C1 | .897616 | .838710/.956522 | .505682 | .375000/.636364 | 通过 |
| B3 | .832468 | .725806/.939130 | .613636 | .500000/.727273 | 通过 |
| C3 | .873422 | .790323/.956522 | .553030 | .500000/.606061 | 通过 |

每模型学习含保存分别.572713/.389953/.396335/.386890秒。两个C均通过原拟合门槛，允许按原计划进入新种子实际评价；开发留出较差原样记录，不据此挑模型/改变预算。训练的KL仅450固定root分布，不当作整局策略KL。

3209公共状态合同（450新root+2759旧教师/锚点）全部通过：初始B/C权重同、E logit/value逐位一致，原tensor/value、611不适用含368局外输出、非EndTurn argmax/mask不变。B随意扰动动作编码仍不变、公开padding及parked/onlyEndTurn/duplicateEndTurn边界通过。format/schema/baseline/nonfinite/generic-loader错误严格拒绝；新格式显式Python加载，尚不支持C++/Linux实机部署。

独立审计从900原始终局分支奖励重建target/方向/split/weight，对四模型各450 root（1800次）独立重建X1原非EndTurn argmax及其具体公开action编码，与缓存严格相同；固定最终指标、同seed初始tensor/参数量、完整Adam8组state/256step/finite/moments及roundtrip通过。B未使用的action输入权重保持初始值。未重放所有优化器更新；游戏/Env/model.py及旧奖励和头源码未改，实际学习文件与3d2d6de Git blob SHA一致。

## 正常新局资格

```bash
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/endturn_action_experiment.py pilot --start artifacts/nosl-q4r/start.pt \
  --heads artifacts/nosl-q4ac/qualification --learning artifacts/nosl-q4ae/learning \
  --contract artifacts/nosl-q4ae/final-contract/result.json --out artifacts/nosl-q4ae/pilot
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/endturn_action_experiment.py normal \
  --experiment artifacts/nosl-q4ae/pilot --out artifacts/nosl-q4ae/pilot-normal
```

两命令实际退出0。四模型各固定4新NQ4AEPILOT000000–000003，共16条实际正常死亡，0故障；均未进第二幕，平均层数B1/C1/B3/C3为14.25/8.75/11.25/10。四采样wall分别.579150/.326365/.405921/.420250秒。仅作系统资格，没有X1配对样本，不以4局均值评定强度。

**16/16完整正常TestMode=false原版回放、2316/2316决策、全部end.run字段和逐步快照一致**，无HP/deck/relics注入、failures=[]。重复种子跨模型是16次执行，不是16个独立强度样本。

## 正式评价、资源与收尾

完整门禁`taskset -c 4,5 nice -n 19 env STS2_BUILD_JOBS=2 scripts/check.sh`在1af710621ed4b07c012c035b9c303711614104c5实际退出0，末行all checks passed；2026-10-06 13:45:22.907251–13:50:19.727362UTC（纽约09:45–09:50）。随后只改文档，没有改变运行源码。

2026-10-06 13:51:04.159186UTC通过一次本地runner启动实际PID1362899，CPU affinity[4]/nice19、device=cpu/cloud_used=false，evaluation-job.json。原样最终X1/B1/C1/B3/C3各1024新NQ4AEEVAL，CPU float32/64槽；原1500秒上限、不补种子、不换中途头。实际driver终止、PTY退出1，14:03:19.206670UTC记录evaluation-exit.json；退出1是配对门槛阴性，不是运行故障。完整wall735.047477秒，全部5120真实死亡，0故障/截断，gate_passed=false。

```bash
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B artifacts/nosl-q4ae/run_local_evaluation.py
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/verify_endturn_action.py evaluation \
  --experiment artifacts/nosl-q4ae/experiment --out artifacts/nosl-q4ae/verified-results.json
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B tools/nosl/endturn_action_experiment.py normal \
  --experiment artifacts/nosl-q4ae/experiment --out artifacts/nosl-q4ae/final-normal
```

首命令实际退出1为上述阴性；第二命令独立核对全部raw结束/seed/计数/登记选择，并以不同250批大小的bootstrap复算四个区间，实际退出0；最后正常核验实际退出0，范围见下文。

| 策略 | 平均层数 | 第二幕 | 第三幕 | 通关 | 策略显式EndTurn | 策略决策 |
|---|---:|---:|---:|---:|---:|---:|
| X1 | 18.228516 | 301/1024 | 1/1024 | 0/1024 | 3 | 230431 |
| B1 | 10.186523 | 75/1024 | 0/1024 | 0/1024 | 8757 | 128102 |
| C1 | 9.567383 | 35/1024 | 0/1024 | 0/1024 | 9012 | 120674 |
| B3 | 10.291992 | 71/1024 | 0/1024 | 0/1024 | 8609 | 128992 |
| C3 | 9.335938 | 41/1024 | 0/1024 | 0/1024 | 8596 | 117043 |

总725242策略决策。显式EndTurn不计原生单合法自动推进，不能解释为X1整局只结束3次回合。不同旧实验的X1种子不同，不能用横向均值当同seed纵向提升。

| 配对 | 平均层数差 | 10000次bootstrap95%区间 |
|---|---:|---|
| C1−X1 | −8.661133 | [−9.092773,−8.227539] |
| C1−B1 | −.619141 | [−.986353,−.260742] |
| C3−X1 | −8.892578 | [−9.317383,−8.461914] |
| C3−B3 | −.956055 | [−1.319336,−.603491] |

两个C相对对应同容量B没有正收益，原方向继续门槛全部失败。每模型实际0/1024胜，Wilson双侧95%区间[0,.003737404]，不把观测0说成总体胜率精确0。不分离声称更大容量、单世界标签、权重/KL或其他组件是所有退步的唯一原因；本项只支持该固定动作上下文组合没有带来净收益。

各臂实际采样wall合计X1/B1/C1/B3/C3为240.222234/114.765476/110.868207/115.778787/106.214535秒。CPU用户/系统702.908029/31.846861秒；kernel child maxrss691.515625MiB，147个5秒样本RSS峰643.851563MiB。采样值不当连续绝对峰，CPU运行无GPU和云费用窗口。普通forward批延迟逐64局保存中位/p95，不能当Linux实机时延。

最终登记首2局+最深局（无胜场）共12次：各模型NQ4AEEVAL000000/000001，B1/C1/B3/C3最深分别000031/000182/000553/000073，均第二幕33层死亡。**12/12完整正常TestMode=false原版回放、3189/3189决策、逐步快照及全部end.run字段一致**，failures=[]；不是全部5120轨迹均Oracle认证。加本地pilot16/2316，共28次完整回放执行/5505决策，包含重复seed，不算28个独立强度样本。

云pro-7912450e5767于2026-10-06 13:35:27.318561及14:04:40.881105UTC只读API确认Success/shutdown；没有power_on、云训练或评价，本项云使用时间0。封存旧证据不写入，保留另一个chat的CLAUDE.md删除、README/roadmap段落和两份性能文档。本项在最终正常回放及封存后合入main，当前X1保留；高水平A10完整胜率、必要公开记忆和Linux实机合同仍未验收。

本项数据/完整Adam/实际公开推理/正常路径及主线门禁通过，整局效果明确失败。后续应重新检验标签泛化和训练约束覆盖实际访问状态的程度，避免仅让稀少root拟合更高；本项没有证明“扩大同样数据就能解决”，也没有证明全部退步的因果首因。不扩大此配置、不宣称高水平或实机就绪。

证据于2026-10-06 14:08:24.578336UTC封存：`artifacts/nosl-q4ae/manifest.json`列出360个文件、21,676,970字节，manifest SHA256 `90bd24c738b2e89b3d7a84832dd159a164bba77d9a9f06295396778c42afa9a0`。合入前已独立重读并核对全部文件大小和SHA；封存后目录只读，后续实验使用新目录。
