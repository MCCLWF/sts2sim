# NoSL Q4-n：新版教师在真实学生战斗根上的资格与价值

基线 `ba77fdb`，实现提交 `d5b4932`。X1 未改权重；本轮完成正常规则的真实学生根搜索、
原版路线执行和 Env 对拍。**16 个冻结训练种子中 15 个完整通过，9 个至少多保留 3 HP，
达到预设小样本门槛。尚未导出学习表或训练新学生，不代表 NoSL 强度提高。**

## 前置状态与正常规则边界

- 原始 CombatSolver 0.50.0 锁定 `0d290fbee7e2779d2cebd8b8f652d82d00b6e8fc`，
  跟踪源码无修改；使用该源码构建的原始 C# DLL。旧 `external/CombatSolver` 桥接和
  C++ 搜索移植均未升级或当作新版教师。
- X1 checkpoint SHA256 为
  `6fb5f1955a11d55175d348659460ce459017baa018f7cf9242a671ba19266703`，
  features=3、actor_filter=1、冻结贪心推理。已有 dev 200 的 18.66 层、0 胜仍是基线，
  本轮不使用 dev/test 根作教师标签。
- 发现旧 Oracle 默认 run 和上游 OfflineSearchHarness bootstrap 都使用 TestMode=true。
  旧 oracle 只恢复了三个玩法读取，不能把历史门禁重新视为普通规则全资格。
  本轮 `run --oracle-test-mode off` / 新宿主始终显式 false，并在 begin/root/after 中记录。
  旧默认 on 的回归路径保留；本轮新增 off 证据独立列出。
- 新宿主链接原始上游搜索辅助代码，**不调用**上游 GameBootstrap 的测试模式初始化、
  空本地化或 Exists=true 补丁。真实游戏 v0.111.0 DLL、完整英文本地化、老档案全解锁和
  原有 RNG/房间/动作顺序保持原样；牌组、HP、药水、遗物均来自学生轨迹，不人为注入。

## 实现和发现

`ProductionVisualReads.cs` 按原始源码与正版 DLL 逐方法核对 94 组、98 个 V getter，
只关闭视觉分支。G/T/P 读取保持普通规则。旧审计表有三处方法归属写错，已按源码修正：
RunManager.InitializeShared、ChecksumTracker.LogStateDivergence、Vantom.PrepareMove。
纯视觉的附魔、眩晕、Serpent Form 工厂和 ThinkCmd 另外跳过；保留玩法效果及其顺序。
UI deferred notification 只处理已移出的玩家，保留有效玩家的 RecalculateCardValues 和订阅者。

首轮资格暴露了预览 NCard.Create 被较早编译的调用方内联、进而中断 RemoveFromDeck 的问题。
现在先安装原有视觉工厂，再编译调用方。不是跳过移牌、移除或战后 Hook。
Oracle 捕获后台 TaskHelper 异常并明确结束为 error，避免部分结算被当作有效轨迹。
一份 smoke 的构建失败后误跑了旧二进制，该产物已排除；后续构建/执行均 fail-fast。

教师仍使用原始 foreign-patch 审计，只给已经审计的 **Headless 类型**提供临时
gameplay-neutral 元数据，搜索后恢复；不把整程序集冒充原版、不关闭其他未知补丁检查。
原始 display builder 对正版 Mocks 中不存在的标题做局部过滤，真实内容缺标题继续拒绝，
不改真实本地化存在性和模型数据库。

搜索前后同时比对 run/combat dump 和原始 continuation/live stamps；再继续完整学生动作，
用未修改的 L4 比较每一步和终局，检查搜索污染。教师路线另起原版进程恢复同一个 root：
手牌用完整状态键及出现次序，目标用 combat id，药水用实际槽与 model id，选牌用选项/源
出现游标及完整状态键。药水 null 目标按原版 EnqueueManualUse 解析到合法玩家目标。
所有动作仍走 OracleRun 原来的合法性检查和真实队列，所有选择仍走原版效果。

路线在下一正常决策点停止，此时原版已生成战斗奖励；在战后/奖励之间停止会错比奖励 RNG，
已修正。既有 L4 对局部整局轨迹仍报告 truncated；额外合同要求全部决策相同、无 native error，
并逐字段比较结束时 run 资源/牌组/RNG。**不放宽 L4，不把战斗前缀叫完整整局。**

## 命令与原始产物

从仓库根执行（本地计算 nice19；采样/搜索限制单核和 DOP1）：

```bash
dotnet build tools/NoSLTeacherHarness -c Release -v quiet -nologo -m:1
PYTHONPATH=python OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 \
  nice -n 19 taskset -c 4 .venv/bin/python -B tools/nosl/student_roots.py collect \
  --checkpoint artifacts/nosl-q4j/x1/checkpoint.pt --out artifacts/nosl-q4n/student
nice -n 19 taskset -c 4 .venv/bin/python -B tools/nosl/student_roots.py batch \
  --collection artifacts/nosl-q4n/student --out artifacts/nosl-q4n/batch2
.venv/bin/python -B tools/nosl/student_roots.py audit --out artifacts/nosl-q4n/batch2
STS2_BUILD_JOBS=2 nice -n 19 scripts/check.sh
```

产物均在 `artifacts/nosl-q4n/`。collection.json 固定 16 个种子 `NQ4N000000`–`NQ4N000015`、
checkpoint/native SHA 与公开推理配置。采样 4,021 个策略决策、16 局正常死亡，约 3.01 秒。
没有按结果换种子；根规则为每种子第 8 层起首个战斗决策，实际根层数 8–14。

每个 invocation.json 保存输入 script SHA、宿主/solver DLL SHA、上游 commit、完整命令与
90 秒硬限时；每个 search/native 目录保存 stdout、完整 oracle trace、teacher.json、
状态/质量及 env-contract.json。单根 beam30、nodes10000、软预算 10 秒、DOP1。
prototype `teacher-probe3` 的搜索后整局 451/451 相同；`teacher-native3` 有 24 动作和
1 选择，144/144 前缀决策及终局资源/RNG 相同。

首轮 `batch/` 暴露适配问题，保留失败。其初始质量提取误把战斗内 card_select 当作结束点，
没有用于训练或扩采；audited-results/summary 从保存的真正战后决策重算，不重搜/挑根。
修复后的 `batch2/` 同一批 16 种子全部复验，最终结论来自 audited-summary.json，和该批
原始 summary 一致。

## 小样本结果

| 指标 | 原始结果 |
|---|---:|
| 固定种子/尝试 | 16 |
| 搜索不污染根、后续完整学生轨迹合格 | 15 |
| 真实教师路线 + Env 前缀/终局资源/RNG 合格 | 15 |
| 搜索后完整学生决策相同 | 4,627 |
| 教师路线前缀决策相同 | 1,825 |
| 教师真实动作/选择 | 302 / 19 |
| 动作组成 | 233 出牌、56 结束回合、13 用药 |
| 相对学生多保留 >=3 HP | 9/16 = 56.25% |
| 合格根终局 HP 增益均值/中位数 | +5.4 / +6 |
| 15 个合格搜索墙钟总和/中位数 | 29.32 秒 / 1.67 秒 |

逐种子 HP 增益（同一实际战斗，成功路线）：
`000000:+8, 000001:+1, 000002:0, 000003:+6, 000004:0, 000005:+7, 000006:0,
000007:+9, 000008:+1, 000009:+6, 000010:+16, 000011:+6, 000012:+11,
000014:+10, 000015:0`。
原计划要求 >=12 合格搜索且 >=25% 根改善 >=3 HP 或战败转战胜，本批通过。
这是少量训练根的教师机会和真实可执行性证据，不是训练后学生评价，不支持泛化/胜率结论。
HP 指标包含实际药水使用；没有验证长期资源价值，且这批合格学生本来都能赢所选战斗。

唯一未合格 `NQ4N000013`：较早 root 的搜索正常完成，但后续学生轨迹遇到
RollingBoulderPower。普通规则通过 NRollingBoulderVfx.HitCreature 信号触发真实伤害，
没有场景树时失败。此机制属于 G 类，不切成测试分支或忽略伤害；整条记录未计入资格。

完整 `scripts/check.sh` 全部通过（60 CTest、L2/L3/L4，日志 full-gate.log，末行
`all checks passed`）。标准门禁的历史 on-mode L2/L3/L4 回归，不代替以上显式 off
资格；新宿主独立构建和真实教师路径已直接运行。

## 云实例与下一步

本轮恢复/搜索是 CPU 合同小样本，未启动付费 GPU；API 于 2026-10-05T16:19:20Z
确认 `pro-7912450e5767` 为 shutdown，状态保存在 cloud-status.json。
本轮没有新增权重、训练 tensor 数据或模型晋升。NoSL 在线 actor 仍只做公开表直接推理，
不会部署教师搜索。X1 全局水平和公开观察实机交付仍未达成，目标继续 active。

下一步冻结单独的教师学习实验：从合格真实路线导出公开观察/action-row 标签，核对 filter/
选择语义；评估少量有收益的模仿更新及 KL 约束，再按事先判据决定扩采/PPO。不能据本批
教师 HP 收益直接启动无界训练或声称完成高水平 A10 NoSL。
