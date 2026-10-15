# Q4-q：整局信用跨度诊断与有界对照

## 目标和现状

Silent A10 正常整局/Linux NoSL 仍未达到高水平，X1 为基线。Q4-p 已完成：单学习 seed 新输入有 +.2875 层信号但未过事前 .5 门槛，不继续该配置扩训。当前 PPO gamma=1、GAE lambda=.95、rollout=64；构筑/选路的延迟结果可能过度依赖 critic。此处是待实测假设，不是已知首因。

依据：[GAE 原论文](https://arxiv.org/abs/1506.02438)描述指数加权优势的偏差/方差取舍；不能由它推出本游戏必然受益。

## 诊断

- 固定 X1 SHA256 6fb5f1955a11d55175d348659460ce459017baa018f7cf9242a671ba19266703、actor filter 1，NQ4QDIAG000000..000127、正常 Silent A10，按分布采样，policy seed 2，真实终局/故障保留。
- 记录公开表对应 value、决策/动作、HP/floor/公开 Boss 状态、奖励、下一决策及真实终点，绝不使用未来信息选动作。用于离线测量的完整后继不能进入 actor。
- 抽出 PPO 的奖励和 GAE 数学供采样/诊断共用；用原实现逐步奖励、boss/ref 状态及迭代 GAE 差分证明等价，保持奖励配置和游戏规则不变。
- 比较 .95/64、.995/128、.995/256 和整局 Monte Carlo 相对于同一 critic 的优势；统计局外选择到下一场战斗结束/整局终点的跨度、符号一致率、critic 对真实后续回报的偏差。Monte Carlo 的单轨迹噪声须保留，不能把符号分歧直接称为错误动作。
- 若至少 96/128 正常终局、至少 25% 的局外实质选择距下一场战斗结果超过 20 个决策，且 .95/64 与 MC 的局外优势符号一致率低于 75%，才运行下述训练筛查。否则只报告诊断，另选依据，不用任意调参代替证据。
- 同时记录原始 int32 数值范围；PPO 当前 int16/clipping 不符合公开输入无损要求，改为 int32 无损 rollout 存储，两臂一致使用。验证训练 forward 使用的表逐项等于采样表，不将数值极值映射成另一个状态。

## 训练前固定筛查

若诊断通过，两臂 C/H 均从 X1 开始、features 3（不开 Q4-p）、fresh Adam、学习 seed 2、训练前缀 NQ4Q。两臂共同 rollout 128×240（6 worker×40）、30720 决策/迭代；C lambda .95，H lambda .995。每臂新增 5,007,360 决策/163 次更新、1800 秒上限，其余配置沿用 X1：gamma 1、HP 潜势 .2/70、boss damage .3、LR .000133、target KL .01、熵 .01/.05。共同增加 rollout 长度用于隔离 lambda；不是把两处差异合成未受控解释。

固定新 tune 集 NQ4QEVAL000000..001999，X1/C/H greedy，完整故障保留。H 对 C 和 X1 均须平均至少 +.5 层、逐 seed 配对 95% 区间下界 >0、无新增故障；否则不追加本配置训练、不替换 X1。单学习 seed 通过也只允许独立复现和新路径正常规则资格，不能声称高水平/稳定。

## 实现、验证和资源

分支 feat/nosl-q4q-credit-horizon；python/nosl/returns.py、ppo.py、tools/nosl/credit_horizon.py、计划/报告及 README/roadmap。不修改 Env、搜索或 actor 输入语义。抽取函数等价检查及真实轨迹诊断先通过，合入 main 前完整 scripts/check.sh。

本地诊断 nice19、单线程，云端重任务沿用授权实例 pro-7912450e5767，启动前准备 payload 和独立最多 75 分钟关机保护，两臂顺序执行；运行结束取回原始模型/日志/逐局结果校验，power_off 并确认 shutdown。凭据留本地，保留其他 chat 的未提交文件。
