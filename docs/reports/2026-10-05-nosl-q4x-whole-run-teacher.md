# Q4-x：完整跑局离线教师先导未达到扩采门槛

最新原始 CombatSolver 0.50.0 已用于完整 Silent A10 跑局的离线教师。固定 8 个训练种子的纯 X1 对照全部正常规则合格；教师参照 7 条整局合格、1 条回放故障。77 个实际战斗根中 69 条路线通过原版执行、Env 和公开标签核验，得到 1335 个教师学习行；但第三幕覆盖和通关均为零，预设扩采门槛失败。本项没有训练新模型或更换 X1，高水平整局 NoSL 仍未达到。

计划：[Q4-x](../plans/2026-10-05-nosl-q4x-whole-run-teacher.md)。基线 main `c230250beb49a25dbf31f78e372dc04e658c1cb9`；分支 `feat/nosl-q4x-whole-run-teacher`。实现 `5ced99f`，第三幕索引修正 `2abcc51`，错误保留 `8fbd342`，稳定根 capture-only 修正 `968e0688adf1624835ae4829bc6e414080e7e75e`。原始结果、失败目录和核验清单在 `artifacts/nosl-q4x/`。

## 实现与来源

`tools/nosl/whole_run_teacher.py` 在同一个正常新局中交替执行教师战斗和冻结学生的局外决策。学生只读取四张公开 int32 表和公开动作历史；每场战斗首个普通 combat 决策作一次离线搜索，初始选牌仍由学生执行。无法获得完整存活胜利路线时明确回退学生，不重搜或换种子。接口故障立即停止该局，保留实际状态，不转换为死亡。

`NoSLTeacherHarness` 新增可选准确原版决策编号，避免仅用层数捕获错误的同层连续战斗。capture-only 在稳定根搜索后、脚本载体执行前停止。搜索必须证明原版正常模式、根及完整 continuation 未变化；完整胜利路线从种子重新执行，所有 L4 字段、快照及终点资源/RNG 一致后才能导出公开标签。导出器的可选 continuation 返回值允许继续采集器与独立训练 ABI 回放逐表、逐状态核对；原默认返回合同保留。

完整终局另作独立正常规则回放。战斗前缀匹配只记前缀资格，不能冒充完整跑局。错误根拒绝时先保存原始错误，避免读取不存在的 teacher.json 覆盖首因。原版规则、Env、公开 actor、模型及在线 NoSL 推理未改；教师搜索不进入在线部署路径。

上游锁定 `9a4489d840a3772aef451d5c6dc42dd79719f380` / 0.50.0，使用已拉取的独立原始源码与程序集，保留 `external/CombatSolver` 旧 Env 桥接分支。云端原始 CombatSolver DLL SHA256 `6fb76f3bd2465db90e69aed4162d5174358780196513d418ff4bd54cde286df4`，本轮最终教师宿主 DLL SHA256 `1649aeb3d675456847d7d77f46383a8b74ed9f9379c850c6fbba6d1d1aae4291`。

正式两臂均为 singleton CUDA float32 贪心 X1，公开 features3/actor_filter1。X1 SHA256 `6fb5f1955a11d55175d348659460ce459017baa018f7cf9242a671ba19266703`。所有战斗固定 beam30、nodes10000、软预算10000ms、DOP1，每个宿主最多90秒；每局900秒、64根和10000个策略决策，总实验3000秒，外层进程上限2700秒。没有 HP/牌组/遭遇/RNG 注入，没有评价种子进入本项。

## 定向资格与门禁

主要命令从仓库根执行，构建限制两 job，本地重活使用 nice19 和 CPU4；消费者构建使用 CPU4,5。

```bash
taskset -c 4,5 nice -n 19 /home/wst/.dotnet/dotnet build tools/NoSLTeacherHarness -c Release -m:1
taskset -c 4 nice -n 19 env PYTHONPATH=python .venv/bin/python -B \
  tools/nosl/whole_run_teacher.py run --seed NQ4XPILOT000000 \
  --out artifacts/nosl-q4x/pilot
taskset -c 4,5 nice -n 19 env STS2_BUILD_JOBS=2 scripts/check.sh
```

| 验收 | 实际结果 |
|---|---|
| 独立开发单局 NQ4XPILOT000000 | 42.465秒，7根/6合格路线、144教师行；第一幕17层死亡，正常整局304决策一致；不混入正式8种子 |
| 可选准确根编号未设置 | 冻结旧默认路线的原版 trace 字节完全相同 |
| 错误准确根编号 | 明确拒绝；修正后退出4，原始错误 `Teacher replay does not match the frozen actual student root` 保留 |
| capture-only 稳定根 | 2/2正常规则前缀决策/快照一致，完整 run 状态与搜索前相同；无 combat action 事件，载体未执行 |
| 最终消费者 Release 构建 | 通过；上述拒绝与 capture-only 使用实际最终接口验证 |
| 完整 scripts/check.sh | `all checks passed`；60 CTest、L2的200组合及要求的L3/L4、快照、遗物与最终胜利路径全部通过 |

完整门禁在 `2abcc51` 阶段执行；之后变更仅为离线教师宿主、采集参数/流程和计划，未改门禁覆盖的引擎、公共 Env、原版 oracle 或比较器。消费者后续构建及实际正常前缀、错误拒绝和正式整局核验补充验证这些改动，不重复无变化的规则门禁。日志为 `check.log`、`build-capture.log`、`capture-stop/capture-proof.json`、`default-selector/result.json` 和 `wrong-selector-fixed/result.json`。

## 云端启动故障与相同输入修正

第一段部署发现远端只有 .NET 9.0.18 运行时，没有 SDK。原生构建成功，但默认 SDK 路径不存在。随后迁移本地构建的两个宿主共6个 Release 文件并逐个核验 SHA，显式重定位依赖；不安装新 SDK，不改变原始教师。

首轮 `cloud/experiment/` 的8条对照全部未获得正常资格，8条教师均发生接口故障，不能作质量比较：已有本地化资源未通过 STS2_SRC 提供，且搜索载体可能落在未完成选牌边界。完整保留首轮结果。可用质量结果产生前，计划登记接口修正，在 `experiment2/` 重试同一8种子：显式使用已有46张本地化表的 Core 目录，并在原版稳定根 capture-only 停止。预算、种子、教师、模型和比较字段不变。

最终云端源码为 `968e068`；`capture-provenance.json` 核验6个迁移文件、原生源码未变和依赖路径。云原生库 SHA256 `dff862e8328998f3262d65a24b1355b434504f39cccd216a20c6e94921e9cb18`；本地原生库 `6296b12846d035ba598cb3beea10d2eaa649c241c468136cf8d51e67a099e231`，构建平台不同，各自记录。正式 invocation 另保存实际宿主、原始教师、脚本和路线 SHA。

## 正式结果

命令通过 `run_cloud2.sh` 执行，显式提供远端 DOTNET、STS2_GAME_DIR、STS2_SRC，OMP/MKL/OpenBLAS和Torch均限制1线程：

```bash
PYTHONPATH=python .venv/bin/python -B tools/nosl/whole_run_teacher.py experiment \
  --checkpoint artifacts/nosl-q4u/start.pt \
  --out artifacts/nosl-q4x/experiment2 --device cuda
```

表中层数为真实终点；故障8层是最后有效采集状态，不是终局。

| 训练种子 | 纯X1终点 | 离线教师终点 | 教师整局资格 |
|---|---|---|---|
| NQ4X000000 | 24层死亡 | 28层死亡 | 合格 |
| NQ4X000001 | 17层死亡 | 8层回放故障 | 不合格 |
| NQ4X000002 | 17层死亡 | 17层死亡 | 合格 |
| NQ4X000003 | 17层死亡 | 23层死亡 | 合格 |
| NQ4X000004 | 33层死亡 | 33层死亡 | 合格 |
| NQ4X000005 | 14层死亡 | 17层死亡 | 合格 |
| NQ4X000006 | 17层死亡 | 17层死亡 | 合格 |
| NQ4X000007 | 17层死亡 | 33层死亡 | 合格 |

15条完整正常规则轨迹共4680决策一致，其中对照2370、教师2310。77根的结果为69条合格胜利路线、7条未完成/仅死亡路线后学生回退、1条回放资格错误。第一幕59根/55合格，第二幕18根/14合格，第三幕0根。1335个教师行包含故障局在出错前的60个局部合格行；这不赋予该局整局资格，也未用于训练。

唯一故障是 NQ4X000001 root-004：原始路线17/17动作已经执行，实际/预测HP均47，但仍剩1个选牌标记。正常原版 trace 的最后动作是 Dagger Throw，随后终点核验拒绝，首因 `Teacher real terminal differs: actions=17/17, choices=1, hp=47/47`。保留完整日志和冻结路线，不清空剩余标记、不放宽比较、不重新搜索或重试该局。是否为致死出牌后的异步选择消费边界，需要独立检查；本项没有证明该路线合格。

7条完成教师整局的最后战斗均报告 `onlyDeathRoutes=true`，并在该战斗回退学生后死亡；其中3条停在第一幕17层、2条在第二幕33层、2条在第二幕中途。该标记只说明固定搜索设置没有找到存活路线，不证明牌组无解。部分完整配对走得更远，但不能将这些特权教师结果解释成学生学习收益。

| 预设扩采条件 | 结果 |
|---|---|
| 16条整局均正常合格 | 15/16，失败 |
| 至少75%搜索根获得合格路线 | 69/77 = 89.61%，通过 |
| 至少4条进入第三幕，或至少1条通关 | 0/0，失败 |
| 平均配对至少+5层、bootstrap95%下界大于0 | 因完整配对资格失败，不计算/不宣称通过 |

`summary.json` 的 `whole_run_gates_evaluable=false`、`gate_passed=false`，进程正常返回1表示预设门槛失败，非16局整体进程崩溃。没有删掉故障种子后重新计算正式区间，没有追加输入、预算、训练或候选模型。

## 资源、取回与停机

正式纯X1共41.629秒，教师参照986.404秒；原始搜索本体合计319.495秒、105121个expanded节点、552572次transition、94484个choice branch。其余耗时包含反复从种子回放、L4、公开导出/ABI和进程启动，不作为新版搜索速度比较；本项没有验证宣传中的2–3倍或峰值30倍。

平台：Python3.12.3、Torch2.8.0+cu128、NumPy2.3.2、CUDA12.8、.NET运行时9.0.18、GCC11.4.0。GPU可见名称为 NVIDIA GeForce RTX4080 SUPER，实例提供约32GB显存。GPU用于公开学生推理，原始C#搜索运行CPU。10秒间隔104个采样中，驱动及子进程峰值RSS1473.016MiB、GPU显存345MiB；采样利用率均0%，稀疏采样会漏掉短推理峰值。`platform.json` 的48线程来自单独遥测进程默认值，实际实验Torch显式为1线程。

实例 `pro-7912450e5767` 于 **2026-10-06 00:39:25.468 UTC** 发出启动指令，**01:25:46.039 UTC** API返回 Success/shutdown；指令到确认停机共46分20.570秒，包含部署、故障修正和取回，未取得实际账单金额。启动前建立独立停止保护，原截止01:49:23 UTC从未延长。取回校验后立即发出停机指令，确认停机才解除保护，保护进程已退出。

云结果包：2104文件、10098577原始字节，压缩4292749字节；归档SHA256 `7fe952a64e4c15b81fa2e53a50ca781e18e1d4525354b52f572f87e6ab55fcc8`。所有文件SHA/大小已在本地逐个核验。整轮本地产物冻结清单为2356文件、16338152字节；`manifest.json` SHA256 `5ad148c04fae07f3df82232f289c3f15fda4652ef4995f137ca1295aa30c519c`。旧Q4-v的424文件、Q4-w的147文件及两个清单SHA均未变。凭据未上传或进入提交/报告。

## 下一步边界

本项证明了完整离线教师接续与公开标签采集的可执行范围，未建立足够的后期成功课程。下一项应独立检验公开局外决策/构筑学习与终结战斗的成功信号，并处理冻结选择消费故障；不能仅继续增加已失败早期标签拟合的epoch，或把69条战斗胜利当高水平整局NoSL。8个训练种子的先导不是最终胜率评价，实机公开观察部署和高水平胜率仍待完成。
