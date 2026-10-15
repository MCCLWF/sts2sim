# NoSL E01：把策略轨迹交给正版回放

分支 `feat/nosl-q2-learning-loop`。对应 [NoSL 计划](../plans/2026-10-03-silent-a10-nosl.md) E01，
以及 [Q1 报告](2026-10-03-nosl-q1-foundation.md)“未做的事”第一条。没有单独的任务计划：范围在 Q2 计划的“结果决定什么”里登记过，
实现提前到 Q2 训练等待期间完成。

## 为什么需要

原有 L4 只有一个方向：oracle 用自带的随机策略出轨迹，模拟器照着回放。oracle 策略不会走的路径永远比不到。
Q1 的战斗外死亡缺口就是这样漏掉的。学习出来的策略会系统性地走 oracle 策略不走的路。

## 做了什么

| 部分 | 文件 | 内容 |
|---|---|---|
| oracle 按给定动作执行 | `tools/Sts2Oracle/OracleRun.cs`（`run --script FILE`） | 每个决策点从脚本取下一个动作；种类不对、或正版此时不提供该动作，就以 error 结束并指出是第几个动作。随机策略分支及其 RNG 调用次序未改 |
| 战斗外的药水动作 | 同上 | 地图、奖励、事件、休息、商店、宝箱界面可以先用/丢药水，执行方式照 `NPotionPopup`（`DiscardPotionGameAction` 的第三个参数是“是否在战斗中”） |
| 脚本提前结束 | 同上 | 脚本用完时不再发出下一个决策，以 `max_decisions` 干净结束，供前缀比较 |
| Env 轨迹转脚本 | `tools/nosl/nosl_script.cpp` | 按采样器的语义重放（规范动作行号、唯一合法动作自动执行），逐个转成 oracle 动作；`ApplyAction` 的逆映射 |
| 批量回放 | `tools/nosl/oracle_replay.py` | 轨迹 → 脚本 → 正版轨迹 → 原有 `tests/l4/l4_run.py --trace` 比较（比较逻辑未改） |
| 文本转换 | `tests/l4/l4_run.py` 的 `action_line` | 认识战斗外药水动作 |
| 轨迹记录 | `python/nosl/evaluate.py --trajectories` | 每局记录种子与动作行号序列 |

每条轨迹的状态：`identical`（正版走完整条轨迹到死亡/胜利，每个决策一致）、`prefix`（遇到 oracle 驱动表达不了的动作，此前一致）、
`script_refused`（正版拒绝某个动作或要的是另一种决策：两边在这里不一致）、`mismatch`（L4 比较发现差异）、`tool_error`。

oracle 驱动仍然表达不了的 Env 动作：不休息就离开休息点；同一休息点选第二个选项。

## 结果

### 冒烟检查点的 8 条 dev 轨迹

策略是一个只训练了 43 万决策的检查点（`artifacts/nosl-q2/smoke`），`NOSLDEV0000`–`0007`，全部死在第一幕。

```text
python3 tools/nosl/oracle_replay.py --trajectories artifacts/nosl-q2/smoke/dev8.traj.jsonl --out artifacts/nosl-e01/smoke --jobs 2
{"trajectories": 8, "statuses": {"identical": 8}, "decisions_matched": 670, ...}
```

其中 3 条含战斗外丢药水。加入药水支持之前这 3 条只能比到丢药水为止（前缀）。

### 战斗外死亡

`NOSLDEV0051` 的随机轨迹去掉一步战斗外丢药水后交给正版：正版在 Room Full of Cheese 选 Search 后以 `death` 结束，
L4 `identical`，45/45。详见[该修复的报告](2026-10-03-out-of-combat-death.md)。

### Q2 两个最终策略的 400 条 dev 轨迹

策略是 Q2 登记实验的两个最终检查点（各 2,003 万决策），`NOSLDEV0000`–`0199`，评价时按策略分布采样。

```text
python3 tools/nosl/oracle_replay.py --trajectories artifacts/nosl-q2/s1/dev.traj.jsonl --out artifacts/nosl-e01/q2-s1 --jobs 3
{"trajectories": 200, "statuses": {"identical": 199, "prefix": 1}, "decisions_matched": 38781,
 "inexpressible_reasons": {"a second rest site option": 1}, "first_problems": []}

python3 tools/nosl/oracle_replay.py --trajectories artifacts/nosl-q2/s2/dev.traj.jsonl --out artifacts/nosl-e01/q2-s2 --jobs 6
{"trajectories": 200, "statuses": {"identical": 199, "mismatch": 1}, "decisions_matched": 56893, ...}
```

种子 2 那条 `mismatch` 不是规则分歧：`NOSLDEV0024` 是被采样器步数上限截断的一局（策略在商店里空转 3 万步），
脚本没有走到终局，L4 把它标成 `truncated`，而当时的驱动只把“含无法表达动作”的截断认作 `prefix`。
29,997 个脚本决策全部一致。驱动改为把“轨迹本身未到终局”的截断也认作 `prefix` 后单独重跑这一条：

```text
python3 tools/nosl/oracle_replay.py --trajectories artifacts/nosl-e01/q2-s2-0024.traj.jsonl --out artifacts/nosl-e01/q2-s2-0024 --jobs 1 --timeout 600
{"trajectories": 1, "statuses": {"prefix": 1}, "decisions_matched": 29997, ...}
```

合计：400 条轨迹，398 条走到死亡且逐决策一致，2 条为一致的前缀，0 条分歧；95,674 个决策在正版中比较一致。
种子 1 的轨迹里有 211 次战斗外丢药水、5 次战斗外用药水，都在一致的轨迹内。

深度：400 条里只有 1 条进入第二幕（`NOSLDEV0187`，死在第 23 层，302 个决策，identical）。其余全在第一幕。

## 过程中的两个自造问题

- 等药水动作完成的循环在入队之后才记录“原来的药水”，而动作在入队调用里就已执行完，于是一直等不到变化，
  报“动作没有执行”。改为入队前记录。
- `action_line` 用 `"potion" in ev` 识别战斗外药水动作，而战斗内的药水动作本来就有一个字符串型的 `potion` 字段，
  被误判。改为判断该字段是否为对象。

两处都是新工具自己的错误，不涉及模拟器或正版。

## 限制

- 只覆盖策略实际走到的状态：目前几乎全是第一幕。第二、三幕和双 Boss 路径仍然没有新策略轨迹的差分证据。
- 比较范围与 L4 相同：oracle 导出的状态、选择候选、RNG 流计数；不是逐字节的完整状态。
- 脚本模式下 oracle 的商店不再有 3000 决策强制离开的保护；长脚本的终止依赖脚本长度。
- 战斗外“用药水”只出现过 5 次（奖励界面 3、地图 2），都一致；覆盖很薄。
- 休息后丢/用药水被映射到随后的地图界面（oracle 驱动休息后自动离开）；这假设“离开休息点”与药水动作可交换。

## 之后的回放批次（2026-10-04 追加）

同一条流水线（`tools/nosl/oracle_replay.py`，L4 比较不变），每个新策略的 200 条 dev 轨迹评价完就回放一次。过滤器 v1 的轨迹由翻译器按同样的过滤器语义重放。

| 批次 | 轨迹 | identical | prefix | 分歧 | 比较一致的决策 |
|---|---:|---:|---:|---:|---:|
| Q2 种子 1、2（上文） | 400 | 398 | 2 | 0 | 95,674 |
| Q4-a F 种子 1、2 | 400 | 400 | 0 | 0 | 56,875 |
| Q4-b H 种子 1、2 | 400 | 400 | 0 | 0 | 75,458 |
| Q4-c I 种子 1、2 | 400 | 400 | 0 | 0 | 78,106 |
| Q5-a 执行器的干跑（`nosl_pilot`，两个策略） | 51 | 51 | 0 | 0 | 9,885 |
| 合计 | 1,651 | 1,649 | 2 | 0 | 315,998 |

仍然几乎全在第一幕：这 1,651 条里进入第二幕的一共 8 条（Q2 1、F 1、H 4、I 2）。
第二、三幕和双 Boss 路径仍然没有新策略轨迹的差分证据，上面“限制”一节不变。
