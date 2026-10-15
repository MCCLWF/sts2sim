# 策略包

`nosl-r12/` 是开发停下时最后一个经过留出种子确认的组合（[R12 报告](../docs/reports/2026-10-11-nosl-r12-visible-defects.md)）：

| 文件 | 内容 |
|---|---|
| `combat.bin` | 战斗策略网络（PPO，第 1690 次迭代） |
| `base.bin`、`strategy.bin` | 局外的基础策略和局外价值网络（margin 0.02） |
| `table.tsv` | 表规则（R4） |
| `readiness.tsv` | 门口模型（R5，五个关口的链式规则，κ = 2） |
| `combat_value.bin` | 战斗内的前瞻优势规则（R10，margin 0.05） |
| `policy.bundle` | 把上面这些组合起来的清单，末行是守卫 `nodiscard,nobuyfull,takerewards`（R12） |
| `bundle.json` | 导出记录：各文件来自哪个检查点、SHA-256 |

格式和每一行的含义见 [`nosl/actor.h`](../nosl/actor.h)。它全部只读公开观察，决策路径上没有模拟和搜索。

水平：进阶 10 留出种子上进第二幕约 65%、进第三幕约 3.6%，**0 通关**；进阶 0 通关约 0.5%。

用法：

```bash
build/nosl_actor_check --trajectories RUNS.jsonl --weights models/nosl-r12/policy.bundle   # 原生 actor 与记录的整局逐决策比较
bash tools/NoslPilot/package.sh models/nosl-r12 OUT_DIR                                      # 打成游戏内执行器的 Mod 包
```
