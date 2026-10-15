# Q4-aa：冻结首轮战斗更新的损失分解

**四个队列的完整损失更新均与未修改学习函数逐参数一致；价值项与熵项都没有达到登记的共同放大信号。仅策略损失也产生较大的首轮变化，因此下一项优先研究策略更新幅度和优势噪声。** 本项是冻结样本诊断，没有新整局强度评价，没有合格新模型；正式X1保持不变，高水平Silent A10整局/Linux NoSL仍未达到。

计划：[Q4-aa](../plans/2026-10-06-nosl-q4aa-gradient-attribution.md)。基线main e617a6a986bbf989df2439c787d0bcedb2140d59，分支feat/nosl-q4aa-gradient-attribution，实现/完整门禁源码6a1bf6d5812dcff7b6d0049a519bc1a80913f960，工具tools/nosl/diagnose_combat_update.py，产物artifacts/nosl-q4aa。Q4-z四个候选均低于X1，旧value对新遭遇回报误差较大，但此前没有critic造成退步的因果证据；历史Q4-f的不同回报与批次不能替代本次诊断。

## 冻结输入与实际核验

使用Q4-z学习seed1/3各自首批32条实际正常训练历史。每个seed的C（整局实际回报）与R（实际遭遇终结回报）除学习标签外，历史全部相同。逐动作重放原生A10 Env，核对inside/action kind/动作行/HP前后/动作总数与真实act、floor、hp、status终点；独立公式复核C/R全部原始标签，不重新采样动作。

| 学习seed | 冻结整局 | 原生决策 | 唯一战斗多合法选择行 | 公开缓存SHA256 |
|---|---:|---:|---:|---|
| 1 | 32 | 7537 | 5955 | 774046b35bcfabb1d9e163f0c851730490dbfb5f36933d28cc6426f9ac5ac68c |
| 3 | 32 | 6253 | 4899 | 51582fd90c39eb2bd77e045a5f92dc7e2053e16789e1473eff2f6015ec609ab8 |

共64条冻结整局、13790次原生决策、10854个唯一战斗观察。C/R及多个消融反复使用这些行，不记作新增独立轨迹。公开global/tokens/actions/counts保持int32；CPU float32以256行批次重新计算X1 logp/value，不称原CUDA采样浮点数逐位复现。没有教师搜索、隐藏未来输入或实机试动作。

正式X1文件SHA为6fb5f1955a11d55175d348659460ce459017baa018f7cf9242a671ba19266703；本地原生库SHA为6296b12846d035ba598cb3beea10d2eaa649c241c468136cf8d51e67a099e231。两者与登记值相同，现有学习函数、公共运行时和游戏规则未修改。Q4-z源manifest SHA e0c4989f5a9bd1a6af2ad3e9f158629f7a9678ea81864a05b5443d7c255b28f2及其372文件另逐一复核不变，previous-evidence-verification.json。

## 实际分解与预算

C1/R1/C3/R3各从X1和fresh Adam开始，分别运行P、PV、PH、PVH四臂。P为现有clipped PPO，V为.5×value loss（value loss自身为.5×MSE），H为−.05×entropy；同队列各臂使用相同seed置乱。优势固定为首批实际MC目标减旧value并全批标准化。全部lr .000133、clip .2、2epochs、minibatch256、grad norm .5；局外X1冻结。

每队列另从新X1运行一次未修改的episodic_strategy.update；PVH最终参数digest与该参照严格相同，4/4通过。初始同模型精确KL为0、argmax变化为0；更新后在该队列全部行测old||new精确KL、熵、动作变化、clip fraction、logp变化与value RMSE，保留16个诊断模型及全部指标。

实际共880次优化step：16消融臂704步、4参照176步。消融臂173664行访问，参照另43416行访问；不把重复访问解释成新样本。总wall **587.737596秒**，登记总上限1200秒、每队列240秒均未触及；没有重跑不利臂、扩预算或挑中途权重。本地CPU4/nice19、Torch/OMP/MKL/OpenBLAS各1线程、CPU float32。

```bash
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  timeout 1250 .venv/bin/python -B tools/nosl/diagnose_combat_update.py --out artifacts/nosl-q4aa
taskset -c 4 nice -n 19 env PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B artifacts/nosl-q4aa/verify_diagnostic.py
taskset -c 4,5 nice -n 19 env STS2_BUILD_JOBS=2 scripts/check.sh
```

诊断和保存结果核验均实际退出0。完整scripts/check.sh实际退出0，check.log末行all checks passed；构建/CTest/脚本要求的L2/L3/L4、跨幕、完整终点和快照门禁通过。其验证范围不等于高水平策略验收。本项没有新的候选整局Oracle回放或Linux实机部署评价。

## 全部首批行的结果

| 队列 | P 精确KL均值 | PV | PH | PVH | P 贪心动作变化 |
|---|---:|---:|---:|---:|---:|
| C1 | .110753 | .115481 | .111171 | .102882 | 10.6633% |
| R1 | .146444 | .141436 | .139975 | .110109 | 10.6633% |
| C3 | .112687 | .116216 | .103608 | .106265 | 11.6759% |
| R3 | .120321 | .130573 | .114012 | .112954 | 10.6552% |

这里是固定数据上一轮更新后的完整分布精确KL，不与训练期间逐minibatch的approx_kl混作同一指标。仅P即产生.111–.146均值KL与约10.66%–11.68%贪心动作变化；P的个别行KL最高20.243，不能用均值声称所有状态变化温和。

初始梯度在全部首批行按样本数加权，float32梯度以float64累计；共享参数范数如下，价值头与策略头分组证据、余弦另在gradients.json中完整保留。

| 队列 | P共享范数 | V共享范数 | H共享范数 | PVH共享范数 |
|---|---:|---:|---:|---:|
| C1 | .885381 | .035556 | .085633 | .892260 |
| R1 | .763432 | .234283 | .085633 | .792961 |
| C3 | .788787 | .015435 | .092163 | .791219 |
| R3 | .756604 | .236985 | .092163 | .798393 |

R价值项相对C更大，但共享范数仍低于P；仅凭范数不能排除耦合，也不能证明整局退步首因。Adam与梯度裁剪具有非线性。PVH相对P把R1/R3 value RMSE从.895956/.908844降至.691342/.675330，拟合改善没有一致放大策略KL。

登记判据要求：V的PV−P与PVH−PH、H的PH−P与PVH−PV，两种配对都至少增加.005且达到1.5倍，并在两个seed复现；只有C/R均通过才记共同信号。保存全部16个配对影响，不删除负向对照。实际V/H在C、R各自的replicated_signal以及common_signal **全部false**。这只能否定本批登记幅度的放大信号，不能证明价值或熵在其他状态、后续更新中没有影响。

## 保存输出审计、判断与封存

独立保存输出核验重新检查公开缓存类型/计数/合法动作/SHA、16模型的有限权重、公开schema/filter和冻结局外摘要、全部更新预算、4个未修改参照参数一致与登记判读公式，verified-results.json返回independent_verification=true。范围为保存输入/权重/预算/参照digest/规则核验，**不是独立第二次前向重算**。每个权重标注diagnostic first update only、combat with frozen outside X1、no strength qualification，不能直接作为正式整局策略。

本项支持下一项有界实验优先限制P更新跨度、检验优势噪声，之后另登记匹配的新正常整局训练与评价；尚未证明这是Q4-z整局退步的全部原因，尚未获得新的胜率改善。原X1在Q4-z固定1024局均值18.253906层、第二幕286/1024、第三幕1/1024、0通关；四个新候选均较弱，正式X1不替换。最终高水平Silent A10整局和Linux NoSL目标继续保持未完成。

本项没有启动云GPU。只读API于 **2026-10-06 05:27:14.616309UTC** 返回Success/shutdown；此前Q4-z已于05:07:28.814726UTC验证取回后关机。README/路线图只将本项新增前缀写入index，其他chat性能段落、两份未跟踪文档和CLAUDE.md原有删除保留，foreign-worktree-preservation.json及foreign-files-preservation.json。

全部诊断/核验/完整门禁进程结束后，于 **2026-10-06 05:46:07.173288UTC** 封存本项74文件、59859063字节，并逐一核对大小/SHA；`artifacts/nosl-q4aa/manifest.json` SHA为 **07d3ee4f166781d25780e83c0d20e407f17a4328787cc30392c692c0423510e9**。旧Q4-z封存目录未写入新输出。
