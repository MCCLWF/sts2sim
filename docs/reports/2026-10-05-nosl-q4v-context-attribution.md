# Q4-v：公开战斗/局外路由归因

完成固定诊断，学习继续门槛失败；无新训练、不替换X1。13策略×512独立新tune种子共6656次尝试，全部C++ Env真实死亡、0故障、0胜，不能据此宣称高水平或全部正版资格。隔离局外减轻了部分退步，但四个combat候选的样本均值仍低于X1。正常规则18条回放中17条完整死亡一致，另1条仅一致前缀；实际完整NoSL/Linux目标未完成。

## 前置与实现

main基线73b1bab；分支feat/nosl-q4v-context-attribution；固定计划见[计划](../plans/2026-10-05-nosl-q4v-context-attribution.md)。正式GPU源码562a61b48bab85961b3d3a77151367d20bceaeb7，正常回放工具修复552f93a。最新教师仍为原始CombatSolver0.50.0/9a4489d，本轮不搜索、不采新标签、不训练。

X1来自Q4-r/start.pt，SHA 6fb5f1955a11d55175d348659460ce459017baa018f7cf9242a671ba19266703。C1/P1/C3/P3固定Q4-u第64轮checkpoint；SHA与旧实验严格一致，列表见cloud/experiment/platform.json。所有非评分参数/缓冲SHA 18c07c509eb35a3d2eec01664a52baff42b2703bfea6f28f2f9a9f841acd87b1。

tools/nosl/context_attribution.py只按公开global列11 kGInCombat路由，包含战斗内Choice；full全程候选、combat仅战斗候选、outside仅局外候选，其他阶段X1。共享相同冻结编码器节省前向，value为X1；actor不读取seed/隐藏状态，不调用搜索/未来模拟。工具只作诊断，不改部署/PPO/model/schema/Env默认。

## 固定整局结果

NQ4VEVAL000000..000511，正常Silent A10/filter1、greedy，GPU bfloat16；各组512个完整真实死亡，0排除/故障/胜。保留全部逐seed动作及路由计数，总1,487,823次策略决策；同seed跨策略相关，6656不是独立胜率样本数。第二/三幕数指act>=1/2。

| 策略 | 平均层数 | 第二幕/512 | 第三幕/512 |
|---|---:|---:|---:|
| X1 | 18.714844 | 168 | 0 |
| C1-full | 18.230469 | 159 | 0 |
| C1-combat | 18.431641 | 152 | 0 |
| C1-outside | 18.345703 | 161 | 0 |
| P1-full | 17.925781 | 154 | 0 |
| P1-combat | 18.404297 | 156 | 1 |
| P1-outside | 18.132812 | 153 | 0 |
| C3-full | 18.273438 | 160 | 0 |
| C3-combat | 18.453125 | 155 | 0 |
| C3-outside | 18.148438 | 155 | 0 |
| P3-full | 17.523438 | 139 | 1 |
| P3-combat | 18.193359 | 150 | 0 |
| P3-outside | 18.164062 | 156 | 0 |

相对X1的配对层数差及未作多重比较校正的95%区间：

| 候选 | full−X1 | combat−X1 | outside−X1 |
|---|---|---|---|
| C1 | -0.484 [-1.030, +0.061] | -0.283 [-0.756, +0.190] | -0.369 [-0.875, +0.136] |
| P1 | -0.789 [-1.335, -0.243] | -0.311 [-0.807, +0.186] | -0.582 [-1.062, -0.102] |
| C3 | -0.441 [-0.982, +0.100] | -0.262 [-0.722, +0.198] | -0.566 [-1.056, -0.077] |
| P3 | -1.191 [-1.727, -0.656] | -0.521 [-1.014, -0.029] | -0.551 [-1.029, -0.072] |

P1/P3的combat−full分别+.478 [.001,.956]和+.670 [.209,1.131]；C1/C3分别+.201/+.180且区间跨零。这支持局外漂移是可见的干扰，不能证明同一状态的单个局外动作导致某项战损；路由改变整局状态分布。所有combat相对X1的均值仍负，四个登记继续门槛均false（两学习seed均>=+.5层、CI下界>0、无故障）。不将P3相对自身full的改善当作超越X1，不扩旧动作头路线、不选有偶然深局的候选。

full组局外改变2179–2424次，combat组全部局外实际改变0；outside组全部战斗改变0。每局history长度与两类决策计数一致。独立本地verify与原cloud summary逐项相同，完整配对/每类漂移原文见verified-results.json和各label.json。

## 正常规则与工具修复

首次通用oracle_replay未传模式，实际begin oracle_test_mode=true。原oracle/、oracle-deep/记录全部保留为legacy诊断，不能作正常规则资格。工具现默认显式off，保留on供旧诊断，并拒绝缺失或不符的真实begin模式；结果/summary记录模式。

全部18条在新oracle-normal/、oracle-normal-deep/显式off重放，逐条实际begin均false：

- 登记首2种子×8hybrid全部16条完整死亡一致，4695个决策，33.718秒。
- 补充P1-combat/NQ4VEVAL000473：进入第三幕、Env第39层死亡；546个决策及真实死亡完全一致。
- 补充P3-full/NQ4VEVAL000330：Env第44层死亡，转换工具拒绝“a second rest site option”；原版只执行到第二幕第27层，301/301决策一致前缀，后续未验证，不计完整规则资格。

完整合格17条共5241决策，加前缀共5542；只覆盖4个独立种子，不能推广为全部6656局或最终Boss资格。深路径补充读取结果后登记，不替代固定16样本/重算评价/改门槛，也不将tune种子转为训练。下一项优先独立处理第二休息选项的脚本表达边界，依据源码/normal oracle核验；同时后续学习须解决公开表示和跨根泛化，单纯隔离局外后扩训本轮不获支持。

## 验证命令与原始证据

仓库工作目录、所有本地重活nice19/CPU4；完整门禁CPU4,5/两构建job。

```text
.venv/bin/python tools/nosl/context_attribution.py --mode contract --start artifacts/nosl-q4r/start.pt --candidates artifacts/nosl-q4u/cloud/experiment2 --data artifacts/nosl-q4o/cloud/labels/public-tables.npz --out artifacts/nosl-q4v/contract-cpu
.venv/bin/python tools/nosl/context_attribution.py --mode pilot --start artifacts/nosl-q4r/start.pt --candidates artifacts/nosl-q4u/cloud/experiment2 --out artifacts/nosl-q4v/pilot
.venv/bin/python tools/nosl/context_attribution.py --mode verify --start artifacts/nosl-q4r/start.pt --candidates artifacts/nosl-q4u/cloud/experiment2 --out artifacts/nosl-q4v/local-recheck
.venv/bin/python tools/nosl/oracle_replay.py --trajectories <固定label.jsonl> --out <新目录> --jobs 1 --oracle-test-mode off
scripts/check.sh
```

CPU及CUDA合同各2759公开状态，2391战斗/368局外，其中243战斗Choice；四候选四路由logits/value与完整原模型逐项差0，padding/合法mask通过。8局pilot真实死亡。原生规则/网络未改，组件原生资格沿用Q4-u；本地库SHA 6296b12846d035ba598cb3beea10d2eaa649c241c468136cf8d51e67a099e231，远端dff862e8328998f3262d65a24b1355b434504f39cccd216a20c6e94921e9cb18。

check.log（562a61b）及工具修复后的check-final.log（552f93a）均all checks passed：60CTest、L2/L3/L4及固定终局/快照/Rest奖励/Pael门禁通过。这不是高水平强度验收；无重复扩大评价或学习。

## 云资源与证据冻结

pro-7912450e5767，4080 SUPER，Python3.12.3/Torch2.8.0+cu128。启动前detached本地保护pid1206726已核实存活，最长2400秒；远端正式timeout1500秒。首次部署bundle只含HEAD却请求分支引用，fetch失败，未开始评价；改取HEAD，保留初始脚本/manifest/deploy日志，不延长保护。

13组评价累计297.251秒，正式启动2026-10-05T23:31:53+00:00、结束2026-10-05T23:37:04.796146+00:00。32个10秒间隔样本：观测峰值RSS 4416.1MiB、显存1355MiB、利用率24%；不能代表瞬时峰值。API power_on 23:28:59.776588 UTC，power_off 23:38:23.098134 UTC，2026-10-05T23:38:55.953715+00:00确认Success/shutdown，保护确认后解除且进程退出；无仍运行任务。约9分56秒从启动请求至关机确认。

云56文件/7,413,640字节，归档1,006,261字节，SHA e06ba7b24d1d360df1199d2fc21636dce89f050c5af33e4cd13735cd0761878a；全部取回校验后立即关机，本地模式重放不占云GPU。全本地产物清单manifest.json：424文件/31,835,252字节，SHA de6a63127dbbfb5b6109d7cc1c5fc887901ed03ad136b5a55002cd49e872b5b3，逐文件校验通过。旧Q4-u及其他批产物未改；其他chat的CLAUDE删除、README/roadmap性能段和未提交文档保留。
