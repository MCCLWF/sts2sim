# NoSL Q4-t：冻结教师路线消费修复

计划：[真实教师消费与独立预算](../plans/2026-10-05-nosl-q4t-teacher-playback.md)。基线main `310b026`，实现/固定回放版本 `e35ded4f43cedf6db324f489e6e17801a0cf2dd5`；原始证据 `artifacts/nosl-q4t/`。本项是采集接口验证，没有重新搜索、采样或训练，不修改Q4-s原始结果或X1权重。

13条冻结预测胜利路线现均真实执行完毕、命中准确预测HP，3458个原版前缀决策及终点资源/RNG与Env严格一致；原成功八条的oracle日志逐字一致。修复的是四条自动选牌消费与一条替代路线预算；独立预算还暴露并修复了oracle自定义休息奖励的旧Proceed标记。本项不证明新学生强度或整局胜利；高水平Silent A10整局/Linux NoSL目标仍未完成。

## 原版首因与实现

原版 `CardSelectCmd.FromHand` 非手动确认且候选数<=MinSelect时直接返回全部候选，不调用ICardSelector。可选观察器只读取原版异步体MoveNext执行完成后的原Task/结果与调用前消费序号，不跳过原版方法或覆写结果、参数、filter、RNG；原版所有游戏分支照常执行。人工selector已消费时不重复消费；自动结果逐卡验证完整StateKey、CardId/升级、来源/选项occurrence、数量及顺序。无计划或身份不符明确拒绝。观察器仅在教师作用域安装，finally解除。

`OracleRun.ScriptDecisionLimit`默认null，普通脚本仍用原script.Count；冻结教师路线改用checked有界预算：原脚本非空动作数+计划动作数+动作/回合起始选择数+1。成功仍须完整计划、无遗留选择、玩家存活、敌人全死、准确HP，并另做Env每决策与终点资源/RNG核验。默认L4仍将max_decisions认作truncated；这里仅是严格核验的单场战斗前缀，不能记为完成整局。

首次实现直接包住FromHand公开入口，虽12条通过规则差分，但八条原成功日志的来源从FromHand变为FromHandForDiscard；L4忽略该元数据，逐字控制发现了它。观察器改为内部异步体，保留原公开入口和来源。首次失败数据 `replays/` 与 `replay-check.log` 保留，没有覆盖。

NQ4S000008原来303决策时仅执行40/65动作、HP24/1；独立预算后完整65动作、329决策、HP1，仍出现terminal act1/act0。第16层TinyMailbox的休息自定义奖励Proceed留下旧标记；Boss奖励处停止后，旧标记错误授权下一章投票。原版 `RewardsSet.Offer` 仅对CombatRoom屏设置terminal，`NRewardsScreen.OnProceedButtonPressed`仅在terminal Boss屏投票。现在仅CombatRoom奖励Proceed设置标记，普通奖励生成、原停止时刻、RNG及比较字段不变。修后该条终点act0/floor17，严格合同通过。

## 固定路线结果

原始输入全部来自Q4-s，32个训练种子NQ4S000000..000031；筛选条件固定为13条旧预测完整存活路线，包含5条失败和8条成功控制。原Q4-s416文件大小/SHA256全部核验，原manifest SHA256 `d35273260ec69b42d6210ad5c3e65ad00f310ce4039966df7ae654d9b9e1f2d9`。本轮没有新搜索、模型更新、dev/test选择或追加种子。

| 种子后六位 | 学生Boss HP | 新真实教师HP | 动作 | 自动选择 | 原状态 |
|---|---:|---:|---:|---:|---|
| 000000 | 0 | 25 | 40 | 0 | 合格 |
| 000001 | 9 | 15 | 36 | 1 | 失败 |
| 000006 | 14 | 11 | 68 | 0 | 合格 |
| 000008 | 0 | 1 | 65 | 0 | 失败 |
| 000013 | 8 | 24 | 37 | 0 | 合格 |
| 000015 | 20 | 36 | 38 | 0 | 合格 |
| 000016 | 12 | 9 | 50 | 0 | 合格 |
| 000017 | 0 | 3 | 60 | 1 | 失败 |
| 000019 | 17 | 19 | 49 | 0 | 合格 |
| 000021 | 20 | 46 | 56 | 0 | 合格 |
| 000026 | 0 | 4 | 54 | 1 | 失败 |
| 000028 | 0 | 16 | 68 | 3 | 失败 |
| 000029 | 0 | 5 | 51 | 0 | 合格 |

13条准确HP中9条至少多保留3HP，6条挽救原学生Boss死亡。这只是同一批旧训练轨迹的工程修复数据，不重算Q4-s门槛或宣布扩采通过；即使描述修复后的13路线/6挽救，仍低于原先16路线/8挽救判据。学生32局最终死亡、最新独立强度评价仍0胜，X1保持不变。

## 命令、控制与交付

```bash
nice -n 19 taskset -c 4,5 /home/wst/.dotnet/dotnet build tools/NoSLTeacherHarness -c Release -v quiet -nologo -m:1
nice -n 19 taskset -c 4,5 /home/wst/.dotnet/dotnet build tools/Sts2Oracle -c Release -v quiet -nologo -m:1
PYTHONPATH=python OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 nice -n 19 taskset -c 4 .venv/bin/python tools/nosl/check_teacher_playback.py --out artifacts/nosl-q4t/replays-body-observer
STS2_BUILD_JOBS=2 nice -n 19 taskset -c 4,5 scripts/check.sh
```

修复后宿主构建3.19秒、2警告/0错误，独立oracle2.58秒、7警告/0错误（未使用可选宿主hook及既有nullable警告）。最终回放宿主SHA256 `6a3ec3b894b8f5a8791dcb524c3847fdcd9fdcf07e9764ece1da908f0785aa62`，oracle `0af84e73486c68c063a2389650d973f163b9d1fe04e42d1ad866ff35cac7733f`；原始CombatSolver0.50.0仍固定 `9a4489d840a3772aef451d5c6dc42dd79719f380`，没有修改上游教师源码。

最终固定回放97.868秒，summary passed=true：13/13路线、原成功8/8逐字、普通学生32/32逐字、五种损坏自动token5/5准确拒绝；原版均TestMode=false。损坏用例分别为option occurrence、source occurrence、StateKey、缺失Choice和顺序；使用复制文件，原输入不动。首次固定回放97.758秒、passed=false（12/13路线、0/8成功逐字），原始失败完整保留。原生库/actor/学习器未修改。

完整`scripts/check.sh`退出0，60/60 CTest、L2 200局0差异、L3/L4、12项普通Trial合同、固定跨幕/终局胜利与Pael's Wing路径全部通过，末行all checks passed（`check.log`）。正常规则证据单独来自本项13路线/32学生，不能把其他历史默认TestMode门禁扩大解释成正常模式全覆盖。门禁后宿主/oracle SHA与固定回放一致；原生库仍为 `44e8a0bd2737c2304799240f42ffc6493f2eb428bef8377c1bcb65500b4615b6`，X1仍为 `6fb5f1955a11d55175d348659460ce459017baa018f7cf9242a671ba19266703`。

本项全部本地nice19单CPU顺序回放、双CPU构建，不启动云GPU。2026-10-05 22:25:13.363339 UTC结束API核验，实例pro-7912450e5767为Success/shutdown（`cloud-status-after.json`）。本项343个证据文件共35,173,385bytes，大小/SHA逐项核验通过；`artifact-manifest.json`自身不计入，其SHA256为 `6d3db4501d4550d60e6bf39e973724cc35104a2a6f101c3b996e494ac7cf5700`。保留另一chat性能文档/README/roadmap段落和用户CLAUDE.md删除。后续须独立判断局外构筑、公开信息与教师覆盖/学生学习瓶颈，不把本项接口修复当作高水平模型交付。
