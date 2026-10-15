# Q4-p：公开能力归属输入的有界对照

## 目标与前置状态

高水平 Silent A10 整局 NoSL 仍未实现。X1（features 3、actor filter 1）为基线；Q4-o 两个教师蒸馏候选均未过预登记整局门槛，不继续原配置扩采。CombatSolver 最新原始主线 9a4489d 已安装且供离线使用。本实验不改变搜索、游戏规则、奖励或观察 schema。

当前 Power token 的 n1 是公开的能力持有者序号（玩家 0，敌人可见序号 +1），Enemy token 的 n6 是公开敌人序号。假设：显式把能力的 id/数量输入其持有者，降低 transformer 自学关联的成本，可提高战斗表现。它是输入结构假设，尚无强度证据。

## 实现

- 独立 `power_context` 模型开关，默认关闭；不启用 Q4-l 未通过的 deck-summary。
- 仅从实际公开 Power 行按持有者聚合：能力 embedding 分别乘数量的 log/clip 尺度，得到 2×width 向量。共享零初始化线性层写入玩家 global token 或对应 Enemy token；没有能力的持有者不加偏置。
- 忽略 padding，无隐私状态/实例 id/未来查询。旧 checkpoint 和原生权重格式保持兼容，新格式显式记录该开关。
- 检查真实 A10 观察中的数量/归属、独立聚合、重排及 padding 隔离；增长前后所有旧参数和初始输出完全相同；使用非零诊断参数做 Python/C++ 差分。

## 固定实验与判据（训练前登记）

两臂 C（关闭）/P（开启）均从 X1 SHA256 6fb5f1955a11d55175d348659460ce459017baa018f7cf9242a671ba19266703 开始。学习 seed 2、训练前缀 NQ4P、fresh Adam；每臂新增 5,007,360 决策（64×480×163），最多 1500 秒。其余配置沿用 X1/Q4-l：gamma 1、HP potential .2/70、boss damage .3、target KL .01、lr .000133、战斗熵 .01、局外熵 .05。6 worker，单臂顺序运行。没有中途选最佳 checkpoint、换种子或改变预算。

在固定新 tune 集 NQ4PEVAL000000..001999 上比较 X1、C、P 最终 greedy，全部尝试与故障保留。P 必须同时优于 C 和 X1 至少 .5 平均层，配对 95% 区间下界 >0，且没有新增故障，才允许继续此路线。单个学习 seed 通过只是下一轮独立复现实验的依据，不直接宣称稳定/高水平或替换实机策略。失败就停止本轮，不追加训练。

正式数据运行前实现/诊断通过；合入 main 前 `STS2_BUILD_JOBS=2 scripts/check.sh` 完整门禁通过。若候选有效，下一轮单独登记正常规则原版新路径资格与多 seed 复现。

## 资源和交付

使用用户授权的实例 pro-7912450e5767，启动前准备小增量包及独立关机保护（最多 65 分钟），云端只顺序运行有界任务。原始模型、配置、每局结果、日志和 SHA 清单取回；完毕调用 power_off 并核实 shutdown。凭据只留本地。保留其他 chat 的 README/路线图/CLAUDE.md 改动。

分支 feat/nosl-q4p-power-context。涉及 python/nosl/{model,ppo,export}.py、nosl/policy_net.cpp、tools/nosl/check_power_context.py、本计划、同名报告及 README/roadmap 状态。
