# NoSL Q4-r：独立学习seed与采样形状复现

计划：[四臂预登记](../plans/2026-10-05-nosl-q4r-rollout-replication.md)。实验源码提交 `86b07f1cd59c1dd118d08881148745866c3250fa`，原始产物 `artifacts/nosl-q4r/`，工具 `tools/nosl/rollout_replication.py`。

**两个新学习seed均未通过128步采样形状的继续门槛，不扩训这组配置、不替换X1。** 四个候选相对X1的层数增量均小于+.5，全部配对95%区间跨零；五种策略各2000次固定评价，共10000次真实死亡、0故障、0胜。Q4-q单学习seed的共同PPO正向结果未在本批复现，高水平Silent A10整局/Linux NoSL仍未达到。

本批只增加固定实验driver/checker；继续使用Q4-q已通过的无损int32 PPO，不修改规则、奖励、公开schema、actor或原生推理。CombatSolver仍固定为原始0.50.0最新主线 `9a4489d840a3772aef451d5c6dc42dd79719f380`，已在Q4-o正常规则学生根中用作离线教师；本项不运行新增教师搜索，也不声称测得2–3倍或30倍加速。

## 前置状态与固定训练

X1 SHA256 `6fb5f1955a11d55175d348659460ce459017baa018f7cf9242a671ba19266703`，既有50,073,600决策、iteration1630、features3、actor filter1、共享critic、power_context关闭。preflight核验起始SHA、预算、模型合同及权重有限值；未用Q4-q的学习seed2或旧评价集作本批判据。

按预登记顺序C1、R1、C3、R3执行，各从X1和fresh Adam开始。训练prefix NQ4R，学习seed分别为1、3；C为64步×480槽（6worker×80），R为128步×240槽（6×40）。均每更新30,720决策、163次更新、新增5,007,360决策；最终55,080,960决策、iteration1793。每臂1500秒训练上限、外层1560秒终止，实际均完整完成预算。

共同gamma1、lambda.95、HP势.2/70、boss damage.3、LR.000133、targetKL.01、熵.01/.05。取回config逐字段核验，同学习seed的C/R仅out、rollout、runs_per_worker不同。长度和并行槽数同时改变，不能把结果称为单独长度的因果效应；两个新学习seed也不足以估计广泛训练稳定性。没有按seed1结果调整seed3、挑中途权重、换种子或重跑不利臂。

| 臂 | 新决策 / 更新 | 秒 | 决策/秒 | 采样终局数 | 故障 / 挂起 |
|---|---|---:|---:|---:|---|
| C1 | 5,007,360 / 163 | 829.2 | 6039 | 24557 | 0 / 0 |
| R1 | 5,007,360 / 163 | 1174.3 | 4264 | 24643 | 0 / 0 |
| C3 | 5,007,360 / 163 | 907.5 | 5518 | 25217 | 0 / 0 |
| R3 | 5,007,360 / 163 | 1057.9 | 4733 | 25289 | 0 / 0 |

总计20,029,440新决策、652次更新、99,706采样终局。上述吞吐包括实际轨迹和采样配置差异，不是纯GPU或CombatSolver性能比较。

## 固定完整评价与失败判据

五种策略仅评价登记的最终checkpoint，在新tune种子NQ4REVAL000000..001999执行greedy完整跑局。每种2000尝试/2000真实死亡、0故障；全部配对2000、排除0。未追加dev/test、按结果更换评价种子或改门槛。

| 策略 | 平均层 | 进入第二幕 | 进入第三幕 | 胜利 |
|---|---:|---:|---:|---:|
| X1 | 18.3775 | 621/2000 (31.05%) | 1/2000 | 0/2000 |
| C1 | 18.3300 | 559/2000 (27.95%) | 2/2000 | 0/2000 |
| R1 | 18.2595 | 603/2000 (30.15%) | 6/2000 | 0/2000 |
| C3 | 18.3695 | 585/2000 (29.25%) | 7/2000 | 0/2000 |
| R3 | 18.5080 | 624/2000 (31.20%) | 0/2000 | 0/2000 |

| 逐seed配对 | 层数增量 | 标准误 | 95%区间 |
|---|---:|---:|---|
| C1−X1 | −.0475 | .148220 | [−.338011,.243011] |
| R1−C1 | −.0705 | .147036 | [−.358690,.217690] |
| R1−X1 | −.1180 | .151301 | [−.414550,.178550] |
| C3−X1 | −.0080 | .154379 | [−.310583,.294583] |
| R3−C3 | +.1385 | .151890 | [−.159205,.436205] |
| R3−X1 | +.1305 | .149715 | [−.162940,.423940] |

区间来自2000个相同评价seed的层数差，采用配对标准误×1.96正态近似。预设继续门槛要求学习seed1和3的R均比各自C及X1至少+.5层、全部对应区间下界>0、无新增故障；幅度和区间均失败。R3有正点估计也不构成事后选臂依据。两种C控制也未重现Q4-q控制的+.7675层；训练前缀、学习seed及评价集均已改变，不能把差异单独归因于学习seed。此结果不能推出所有PPO/长rollout均无效，也没有隔离int32修复的策略强度收益。

对现有终局行离线分类，没有新增游戏采样：X1/C1/R1/C3/R3在第17层死亡分别987/1084/1008/1073/1020次。对应六个遭遇均为原版Overgrowth或Underdocks的第一幕Boss，依据 `Core/Models/Acts/Overgrowth.cs`、`Underdocks.cs` 的BossDiscoveryOrder与生成ID表核验。X1分别为Ceremonial Beast144、Waterfall Giant201、Vantom126、Lagavulin Matriarch188、The Kin220、Soul Fysh108次；完整分类在 `failure-context.json`。

约一半在第一幕Boss失败、第三幕进入极少，支持下一项检查实际Boss失败链、正常规则执行与教师改善空间；终局分类本身不能区分构筑、战斗或模拟器首因。下一项须独立计划和正常规则/公开输入资格，不继续扩大本次已失败的采样形状实验。

## 执行命令与验证

```bash
# 本地完整合入门禁
STS2_BUILD_JOBS=2 nice -n 19 taskset -c 4,5 scripts/check.sh > artifacts/nosl-q4r/check.log 2>&1

# 云端，PYTHONPATH=python，OMP/MKL/OPENBLAS_NUM_THREADS均为1
.venv/bin/python tools/nosl/rollout_replication.py --start artifacts/nosl-q4r/start.pt --out artifacts/nosl-q4r --mode run

# 取回后的独立本地副本核验，保留原始cloud目录逐文件SHA
cp -a --reflink=auto artifacts/nosl-q4r/cloud artifacts/nosl-q4r/local-recheck
PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 nice -n 19 taskset -c 4 .venv/bin/python tools/nosl/rollout_replication.py --start artifacts/nosl-q4r/start.pt --out artifacts/nosl-q4r/local-recheck --mode verify > artifacts/nosl-q4r/local-verify.json
```

完整门禁60/60 CTest、L2/L3/L4及固定整局路径通过，末行 `all checks passed`。Trial普通模式的12case/56transition及clone/restore通过，输出oracle_test_mode=false；既有其他默认oracle TestMode路径的限制仍在，不能据此升级为新增候选正常规则整局全覆盖。正式实验前源码已通过门禁；后续仅写报告/现状和忽略的回收脚本，没有改变训练或游戏代码。

driver记录各阶段命令、退出码和时间，拒绝覆盖已开始实验。云端与取回后本地独立checker对预算、全部固定种子、训练日志、config差异、模型有限值、配对结果及门槛的JSON语义完全相等，证据 `finish.log`、`local-verify.json` 与 `cloud/verified-results.json`。

五个最终模型各4条原生/Python前向路径均通过，value差0、容差.002、0超容差选动作差异：

| 策略 | 决策数 | 最大logit差 |
|---|---:|---:|
| X1 | 752 | .000092 |
| C1 | 918 | .000086 |
| R1 | 904 | .000089 |
| C3 | 1171 | .000088 |
| R3 | 837 | .000111 |

该检查验证当前推理格式；没有新增候选普通规则原版整局回放、完整公开encoder审计或Linux实机验收。Q4-q所述无限HP公开表示边界也未在本项解决。

最终checkpoint SHA256：C1 `4d02246f6c677bcf092db5b0b18265ebec2664b136a6d61dc97d4fdf21a02bd6`；R1 `4e784746017799ddd2931efe724a3171f8c3a24a126aee7a4e99d04a620a7671`；C3 `5faa536930c25fbdf11d9e1a65fc22ba1d7f791fc46ae707b581142e39eaf725`；R3 `0debe5efe78985adb432599b46809f6ebfcf26a35d3ab273ff867e9fec42f4e0`。

## 云资源、回收与停机

实例pro-7912450e5767启动前为shutdown，独立foreground 7200秒保护在开机前已确认运行。远端缓存 `/root/云 GPU 平台-tmp/mcc-nosl-q4m` 增量同步到86b07f1、跟踪源码干净；起始权重及source bundle SHA校验通过，保留依赖与云磁盘。首次SSH启动连接关闭后恢复严格key/host-key连接，未因此重启或重跑正式实验。

GPU RTX4080SUPER、Python3.12.3、Torch2.8.0+cu128、CUDA12.8。原生库SHA256 `b0c2cf68199ff841377db9694c17fc703dbb580b35e17e63099170d483d47585`。439次资源采样，进程树RSS峰值5830.30MiB、GPU显存峰值4921MiB、利用率峰值88%；资源记录进程在最终归档前确认结束，避免日志仍变动。

冻结臂在后续臂训练期间提前回收；四批归档如下，最终72个数据文件共55,144,330bytes全部逐文件大小/SHA256通过，另保留manifest与complete标记。原始本地云产物在 `artifacts/nosl-q4r/cloud`，独立复核副本在local-recheck；凭据未进入源码、payload或结果。

| 归档 | 本批文件数 | 归档bytes | SHA256 |
|---|---:|---:|---|
| early1 | 15 | 16693710 | ad208a1d0350c8a7a9895b8be9430a69819206d8c4181bd7a1c741c23aee0731 |
| early2 | 9 | 8300078 | df72babbefe18ae6ebd4c4ac0f7593cbd1d35c96a73e3e86ac55f64d55d2ac5f |
| early3 | 9 | 8332661 | ea0f630ecc0dc45533dcc4e285700805792710b7499c2f4ec8826b370795d592 |
| final | 39 | 8350132 | 86b3d34f57583b069d159362f81e665429556476c2160ecf57c7eb83090d0b57 |

2026-10-05T20:20:10.919643UTC发送power_on；21:37:43.660016UTC的power_off返回Success，21:37:53.180525UTC确认Success/shutdown，随后解除保护、SSH master退出。约77分42秒开机时间包含准备、四臂训练、全部评价和回收；停机证据 `cloud-status-after.json`、`stop-after-retrieval.log`、`shutdown-guard.log`。

代码、计划、报告与README/roadmap合入main；保留另一chat未提交的性能段落/文档和用户CLAUDE.md删除。实验完整执行与强度门槛失败分别记录，最终高水平NoSL目标未完成。
