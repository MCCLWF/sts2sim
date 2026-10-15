# NoSL Q4-m：局外同根分支回报

计划：[Q4-m](../plans/2026-10-05-nosl-q4m-branch-credit.md)。基线 `2220c56`，
行为源码 `a0bdd22`。冻结 X1（features 3、actor filter 1），起点 SHA256
`6fb5f1955a11d55175d348659460ce459017baa018f7cf9242a671ba19266703`。

## 问题与实现

Q4-l 未显示可靠提升，本轮检验局外探索/长期信用信号。
从正常 Silent A10 贪心轨迹的真实卡牌奖励、休息状态复制全部合法动作，
由同一冻结学生继续各分支到原版死亡/胜利终点。每种子最多一个根；
预先按 episode index 分成卡牌/休息、最早第 4/10 层四种 strata。
策略输入仍只有公开 global/tokens/actions/counts，seed、隐藏 RNG、
分支最优动作、未来结果均不进入部署 actor。

新增 `nosl_batch_fork` 深复制 Env、actor-filter 公开历史、当前动作表与计步。
新增 `python/nosl/branch_credit.py` 收集终点回报，固定 8 epochs 做动作排序学习，
同时约束原策略 KL 与战斗观察 KL。冻结 value head；共享编码器仍可能改变，
不称战斗策略严格冻结。每个 donor 的第一个战斗观察提供锚点，覆盖范围有限。
每 seed 用 SHA256 划 10% 独立验证根，只诊断拟合泛化，不选择 epoch。

分支训练回报为后续层数增量 + 50×真实胜利 + 15×后续 Boss 已观察伤害份额。
成功离开 Boss 战斗补足该 Boss 的剩余份额；死亡后的空终点观察不解释成敌人被杀，
因此死亡最后一步未公开的敌人伤害不计入。故障、步数上限或采集时间上限的整根
保留日志并排除可靠监督。单个隐藏世界的分支差异有方差，并非公开信息最优策略标签。

## 已验证的访问合同

- `cmake --build build --target sts2nosl_c -j2` 成功。
- `tools/nosl/check_batch_fork.py --trajectories artifacts/nosl-q4j/x1/dev-greedy.traj.jsonl`：
  `NOSLDEV0066` Choice（4 动作）、`NOSLDEV0114` Rest（2 动作）均通过。
  源和兄弟独立；相同动作后继一致；公开 filter 历史、动作表、计步一致；自复制拒绝。
- `probe-final`：2 根、6 个真实死亡分支、1,664 个策略决策，1.2 秒。
  仅用作工具合同检查，不作为 128 根预实验结论。
- `tools/nosl/oracle_replay.py --trajectories .../probe-fixed/trajectories.jsonl --jobs 1`：
  两条完整分支轨迹与正版 DLL 逐决策一致，共 843 个决策。
  修正终点空观察的 Boss 伤害计分后，`probe-final` 的轨迹与上述 oracle 输入逐字相同。
- 单 epoch、2 根 CPU 训练 smoke 成功，loss/梯度可传递；仅验证工具，不评价策略强度。
  保存参数全部有限，value head 与 X1 起点逐张量相同。
  第一次采集的 numpy index JSON 序列化失败与旧计分产物保留，均不进入正式云数据。
- `STS2_BUILD_JOBS=2 nice -n 19 scripts/check.sh` 全部通过，60 CTest 及完整 L2/L3/L4。
  原始日志 `artifacts/nosl-q4m/full-gate.log`。

## 云实验

使用已授权实例 `pro-7912450e5767`，冻结源码压缩包在云端 SHA256 核对。
凭据仅在本机读取，未上传。独立 60 分钟 power-off 保护于
`2026-10-05T14:23:44Z` 启动，正常结束仍需取回产物并确认 `shutdown`。

平台为 RTX 4080 SUPER（32,760 MiB）、Python 3.12.3、Torch 2.8.0+cu128、
CUDA 12.8、g++ 12.3。策略标签推理 float32，允许 TF32；没有改动 X1 网络或规则。
源码压缩包 SHA256 `33f844594dd74dda20af660b34fc3277a5091b331b1d9b045e9efb64bd705f94`，
云 driver 固定记录源码全 SHA。编译约 5 分钟；域名连接超时后使用已验证主机密钥的直连地址。

```bash
python -m nosl.branch_credit collect \
  --checkpoint artifacts/nosl-q4m/start.pt --out artifacts/nosl-q4m/collection/pilot \
  --roots 128 --attempts 256 --block 32 --seed-prefix NQ4M \
  --time-limit-s 900 --trace-roots 10
```

| 正式预实验 | 结果 |
|---|---:|
| 已尝试种子 | 143（15 个到根之前死亡） |
| 完整监督根 | 128/128，独立留出 11 个 |
| 分支终点 | 397 死亡、0 胜利、0 故障/截断 |
| 至少 +0.5 层的替代选项 | 22/128 = 17.1875% |
| 该比例 Wilson 95% 区间 | 11.63%–24.65% |
| 特权同根最优的平均层数增益 | +1.0703125 |
| 特权同根最优的平均训练回报增益 | +2.1861219 |
| 采集决策 / wall | 70,836 / 52.43 秒（1,351/s） |

事后分组：卡牌根 14/67 有层数改善，休息根 8/61。包括 Boss 部分伤害的训练回报
有 >0.5 改善的根为 26/67 和 18/61。它们均为训练期单世界特权比较，
**不是 NoSL 策略提升**。比例区间包含 20%，不能据此断言局外信用学习无效；
但预注册的点估计扩采门槛未达到，driver 自动停止该路线，没有事后修改门槛。

未做 4,096 根扩采、正式 8-epoch 学习、NQGRDY 2,000 对照或新学生 dev200/30 条差分。
CPU smoke 模型仅验证梯度路径，不能部署或替换 X1。
X1 保留为基线，已有独立 dev200 的 18.66 层、0 胜证据不变。

每 10 秒资源采样 24 次（包含编译末段与采集）：进程 RSS 求和峰值 1,783 MiB，GPU 显存采样峰值 457 MiB、
GPU 利用率采样峰值 9%。RSS 求和可能重复计算共享页，GPU 采样也可能漏掉瞬时峰值；
这是单进程局外标签流水线，未充分利用云 CPU/GPU，不是 PPO 或 CombatSolver 速度认证。

预实验的前 10 条完整选优分支在本机走正版 DLL：**10/10 identical，3,512 个决策全部一致**，
无 prefix、拒绝或工具错误。与原型两条合计 12 条、4,355 个逐决策核对，未放宽 L4 合同。

## 产物、关机与结论

- 24 个数据文件 + manifest，共 177,255 字节原始数据；下载后每项 bytes/SHA256 校验通过。
  归档 `artifacts/nosl-q4m/cloud-results.tar.gz`，解压目录 `artifacts/nosl-q4m/cloud/`。
  起点模型已在本机保留，不重复下载；`platform.json` 核对其 SHA256。
- 预实验 `cloud/collection/pilot/{config.json,roots.jsonl,attempts.jsonl,block-*.npz,summary.json}`；
  固定门槛判定 `cloud/pilot-decision.json`；原版差分 `cloud-oracle/summary.json`；
  分组与资源摘要 `diagnostics.json`；全门禁及失败修复前产物也保留。
- `power_off` 返回 Success。`2026-10-05T14:40:41Z` API 确认实例 **shutdown**，
  `shutdown-result.json`/`shutdown-confirmed` 保存证据，独立 guard 随后解除并退出。

本轮新增离线同根标签能力与规则访问证据，按预设门槛停止阴性预实验，
**没有产生更强的学生，高水平 A10 NoSL 目标仍未达到**。
局外分支中确有改善机会，但冻结学生的后续战斗限制它们能兑现多少。
下一项应检验当前学生实际战斗根的教师价值或公开观察缺口；
若回到局外监督，需要另立预算/收益判据，不能把这次事后分组当作扩采通过。
