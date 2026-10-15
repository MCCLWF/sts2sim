# NoSL Q4-u：冻结编码器动作交互

计划：[固定四臂](../plans/2026-10-05-nosl-q4u-action-interaction.md)。基线main `abd948f`，首实现 `ed6d6f4`，正式修复源码 `d2770098ca5b62b5676ad9235943f8c01a3b2fcc`；原始产物 `artifacts/nosl-q4u/`。

**动作交互与评分头单独学习都未过继续门槛，不扩大该方案、不替换X1。** 五种策略的2560次评价全部真实死亡、0故障、0胜；两个P相对X1分别−0.891/−0.828层，配对区间均低于0。增加教师拟合没有改善整局策略；高水平Silent A10整局/Linux NoSL仍未达到。

本项针对Q4-o真实教师标签训练一致率约44%、整局无显著提升的证据，独立比较现有评分头与新增公开source/target交互，冻结整个编码器、投影、embedding和value。未重新搜索/采样教师，未重新解释Q4-s/Q4-t门槛，当前X1仍未替换，高水平Silent A10整局/Linux NoSL未达到。

## 输入、模型与合同

固定X1 SHA256 `6fb5f1955a11d55175d348659460ce459017baa018f7cf9242a671ba19266703`，features3/filter1、共享critic、power_context关闭。冻结Q4-o公开数据 `8d30a2c7e3ce51c7c0bb73551d062671a4892b5f5d3f241959864d0d74599f29`；1248教师行/57根（训练1216行/55根、留出32行/2根），1511 anchors（1378训练/133留出）。留出根过少只能作诊断，不按held选epoch。

action_interaction独立默认关闭，增加无bias的零初始化128×128投影，输入为公开编码source*target，输出加在原动作向量上。无来源/目标时乘积为零；现有评分MLP宽度不变。初始旧参数逐字保留、输出完全相同，非零路径确实改变logits且value不变。不模拟动作效果，不调用Env未来查询、RNG或隐藏状态；缓存仅为离线固定公开编码，部署仍直接读取公开表推理。

原生权重格式4记录features/power_context/action_interaction=1；旧1/2/3保持可读，未知标志明确拒绝。128公开观察上的零增长、非零输出、缓存/正常前向、无目标零值、token重排/动作指针重映射、额外padding屏蔽均通过；非法features/power/interaction三个fixture严格拒绝。

本地非零format4四条真实推理路径809决策，最大logit差0.000052、value差0、0超容差argmax差异；组合power+interaction两条534决策，最大0.000091、value差0；旧format3两条492决策0.000046，旧format2 X1两条483决策0.000072，旧format1小网络两条52决策0差；容差均0.002。这里只证明格式/当前观察前向，不代替新候选普通规则整局与实机资格。

## 缓存与首个失败

本地首缓存尝试因inputs每batch裁剪到不同动作数而拼接失败，没有训练；保留 `probe-cache.log`。补padding后2759行缓存每行与正常前向逐值相同（最大差0），两轮试学后缓存评分仍相同且全部非评分参数SHA保持 `18c07c509eb35a3d2eec01664a52baff42b2703bfea6f28f2f9a9f841acd87b1`。

首云端尝试 `experiment/` 在2.622秒缓存阶段失败，没有任何正式learner或评价开始；错误为CUDA上缓存三段tensor的两段slice不连续，Linear路径舍入与正常两段输入不同。修复score_actions使slice.contiguous，保持严格逐值判据；CUDA2759行探针最大差0（1.942秒），原失败文件不覆盖。修后在新目录 `experiment2/` 运行原种子、预算和最终epoch，不补种子或按结果重跑失败臂。

## 固定学习与整局评价

C1/P1/C3/P3依次执行，学习seed1/3，C现有评分头，P增加交互；每臂从X1/fresh Adam开始，lr3e-4、64轮、batch128、根CE等权+.5(教师KL+anchor KL)、梯度裁剪1。训练只更新policy MLP，P另更新interaction投影，其他参数和值输出保持原值。每臂固定最终epoch64，不根据held选择模型。

X1与四臂各512新tune种子NQ4UEVAL000000..000511，A10/filter1/greedy完整跑局，一次尝试保留真实胜/死和故障。门槛预先登记：两seed的P均比同seed C及X1至少+.5层，逐seed配对95%区间下界>0、无新增故障，才继续交互路线；两C均对X1满足相同条件才称评分头单独学习正信号。

| 臂 | 固定更新 | 训练秒 | 训练CE / 一致率 | held CE / 一致率 | anchor KL |
|---|---:|---:|---|---|---:|
| C1 | 640 | 9.872 | 1.070 / 56.74% | 1.968 / 34.38% | .1730 |
| P1 | 640 | 9.163 | .922 / 63.98% | 2.287 / 34.38% | .1602 |
| C3 | 640 | 8.741 | 1.070 / 56.66% | 1.990 / 31.25% | .1746 |
| P3 | 640 | 9.571 | .921 / 63.73% | 2.239 / 28.13% | .1611 |

初始教师一致率约40.46%、held37.5%。两P增加训练拟合，但held不增、整局反而下降；仅两held根不足以推断广泛泛化。所有非评分参数独立SHA一致，value输出保持原值；冻结编码器仍不能保证局外动作不漂移：例如24行商店anchor的最终动作一致率C1/C3约70.83%、P1约75%、P3约79.17%。没有把冻住critic或KL较小解释成冻住策略行为。

| 策略 | 平均层 | 进入第二幕 / 512 | 进入第三幕 | 胜利 / 故障 |
|---|---:|---:|---:|---|
| X1 | 18.6797 | 166 | 0 | 0 / 0 |
| C1 | 18.1797 | 142 | 0 | 0 / 0 |
| P1 | 17.7891 | 145 | 0 | 0 / 0 |
| C3 | 18.1113 | 149 | 0 | 0 / 0 |
| P3 | 17.8516 | 148 | 0 | 0 / 0 |

| 逐seed配对 | 层数差 | 95%区间 |
|---|---:|---|
| C1−X1 | −.5000 | [−1.0382, +.0382] |
| P1−X1 | −.8906 | [−1.4293, −.3520] |
| P1−C1 | −.3906 | [−.8804, +.0991] |
| C3−X1 | −.5684 | [−1.1132, −.0235] |
| P3−X1 | −.8281 | [−1.3305, −.3257] |
| P3−C3 | −.2598 | [−.7383, +.2188] |

每项都是同512seed差的标准误×1.96正态区间，剔除0对。所有继续判据失败；P−C的区间跨0，不声称交互层单独显著变坏，但本批没有净收益证据。没有挑中间权重、补评价seed、追加PPO或新候选dev/test；这些仍是原生Env强度结果，没有新增普通规则候选全轨迹或实机验收。

五个最终模型各4条原生/Python推理路径均通过：X1/C1/P1/C3/P3分别752/720/731/809/657决策，最大logit差.000092/.000022/.000031/.000031/.000022，value差均0，0超容差argmax差异。原生format2/4均实际走到非零评分模型，不以零初始化检查代替最终权重验证。

最终checkpoint SHA256：C1 `5302ae6724e9e6d8e2d0e3fb0903ada2ac52f8373e0c3a196e3e3462bd944ec3`；P1 `e339a20e9630c4eeba928899e379e22fa1684043652f9b024f385f77823d134b`；C3 `3c19c8fc2e30291df3a694bc2a41f210f8014e482c4ad4bfd9cc15beaea267d6`；P3 `b70b35fefa9564206585dcaf8e51dcb63e0e4ef678071040f4a2ab96a17a6853`。

## 执行与资源

```bash
PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 nice -n 19 taskset -c 4 .venv/bin/python tools/nosl/check_action_interaction.py --start artifacts/nosl-q4r/start.pt --data artifacts/nosl-q4o/cloud/labels --out artifacts/nosl-q4u/contract-layout
STS2_BUILD_JOBS=2 nice -n 19 taskset -c 4,5 scripts/check.sh
# 云端固定driver
PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 .venv/bin/python tools/nosl/action_head_experiment.py --mode run --start artifacts/nosl-q4u/start.pt --data artifacts/nosl-q4u/data --out artifacts/nosl-q4u/experiment2
```

完整门禁首次实现及contiguous修复后均全部通过（`check.log`、`check-layout.log`）；60 CTest、L2/L3/L4和固定跨幕/终局路径，末行all checks passed。额外定向检查发现grow到独立critic时会误传策略专用interaction参数；`a550182`按critic的完整键集合严格复制并验证非零policy/value输出相同。此路径不用于正式共享critic实验，保留实验源码d277009不重跑学习。该收尾修复的最终完整门禁`check-final.log`退出0、末行all checks passed；额外严格checker核验缓存SHA/身份、全部固定预算与可更新参数，输出与云端原summary逐项相同。最后只修改独立checker和文档，没有改变已过门禁的model/native/game代码，不重复相同游戏门禁。没有改游戏规则。普通模式教师数据来自旧Q4-o合同，默认历史门禁的TestMode限制仍在。

2026-10-05 22:50:55.932029 UTC实例pro-7912450e5767开机，独立2700秒保护先启动。自动续接后原foreground保护句柄与实际进程已消失，先补detached保护，期限仍按原开机时刻2700秒计算（1814秒余量），没有延长计费上限。源/依赖准备及回收都计入；SSH key/host-key严格核验，非交互probe使用绝对Python路径。

RTX4080SUPER、Python3.12.3/Torch2.8.0+cu128、CUDA12.8；远端原生库SHA `dff862e8328998f3262d65a24b1355b434504f39cccd216a20c6e94921e9cb18`。正式完整driver238.318秒，缓存1.919秒、四臂合计37.347秒，余量为评价、原生检查与宿主开销。25次10秒资源采样RSS峰值4907.5MiB、GPU显存1235MiB、利用率20%；短训练峰值可能未采到。

取回81个数据/日志/源码快照文件共57,071,907bytes，逐文件大小/SHA全部通过；压缩包50,977,928bytes、SHA256 `c2f3ffa5c7d6fdf56d356a94f3ee468e650f0cb4cb2d61eeed31fb4629f04321`。原始失败experiment与成功experiment2均在cloud副本中，完整退出码0/complete和四最终checkpoint保留。本地独立复制复核所有固定预算、配置、权重有限值、冻结参数、评价seed/终局/配对区间与cloud结果JSON完全一致，未覆写下载证据。

本地全部249个探针/失败/回收/独立副本/门禁文件共251,169,937bytes另由artifact-manifest逐项核验，清单自身不计入；清单SHA256 `19b669d979548520361027906968cd6a310acb578fc97697c12549addc64a202`。本地新推理库SHA `6296b12846d035ba598cb3beea10d2eaa649c241c468136cf8d51e67a099e231`，与远端编译产物区分记录；原X1/data SHA仍相同。

23:12:06.013839 UTC power_off返回Success，23:12:16.151822 UTC API确认Success/shutdown，随后解除detached保护并关闭SSH master；约21分20秒开机包含源码准备、首次缓存失败、修复前置验证、正式四分钟任务与取回。凭据只在本地用户文件，未上传。保留其他chat性能段落/文档及CLAUDE.md删除。

本批两种学习seed一致不支持扩大旧教师数据的评分头拟合。下一项应独立核查公开状态/教师标签的可辨识性、局外决策漂移与跨状态泛化，避免继续通过增加拟合或换评价种子寻找正结果；X1仍为冻结基线，高水平目标未完成。
