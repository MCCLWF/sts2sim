# 计划：oracle 随机数抽取账本与战斗记录扩展（M1-c）

- 执行者：Codex（gpt-6-sol，high）
- 分支：`feat/oracle-rng-ledger`
- 路线图：M1-c，同时补齐 M1-b 的记录内容。

## 背景

`tools/Sts2Oracle` 的 `combat` 命令（见 `OracleCombat.cs`）用法如下：
- 开一局新游戏，进入第一幕第 1 层最左边的怪物房。
- 用带种子的随机策略打完这场战斗。
- 每个决策点输出一行 JSON：`state` 行是完整状态，`action` 行是所选动作；最后输出 `end` 行。

C++ 模拟器将来要按同样的动作重放这场战斗，并逐个决策点比对状态（L3）。

分歧出现时，只比较 RNG 计数器很难定位原因。所以要加一本**随机数抽取账本**：记录每一次抽取来自哪条流、调用了什么方法、参数是什么、结果是什么、是哪个游戏模型发起的。这样 C++ 那边输出同样的账本后，逐条比对就能直接指出第一次多抽、少抽或参数不同的位置。

参考实现：sts2-ai 的 私有参照实现、`RngArgs.cs`、`RngCallerName.cs`。**只参考思路，不要复制代码。**

游戏源码：`$STS2_SRC/Random/Rng.cs`，只读。

## 任务

1. **账本**：给 `combat` 加一个选项 `--rng-ledger`。打开后，每次 RNG 抽取在对应位置输出一行：
   ```json
   {"event":"rng","stream":"Shuffle","method":"NextInt","args":[0,5],"result":3,"counter":10,"caller":"CardPileCmd.Shuffle"}
   ```
   - 在 `Rng` 的公开抽取方法上挂 Harmony 补丁：`NextBool`、`NextInt`（两个重载）、`NextUnsignedInt`、`NextUnsignedLong`、`NextFloat`、`NextDouble`、`NextGaussian*`。
   - 泛型方法（`NextItem<T>`、`WeightedNextItem<T>`、`Shuffle<T>`）没法直接补丁，要保证它们内部的底层抽取都被记录。如果某个泛型方法有自己的计数逻辑，就在报告里说明怎么处理的。
   - **嵌套调用只记最外层**。例如 `NextGaussianInt` 内部调用其他 `Next*`，只记一条。
   - `counter`：抽取之后该 `Rng` 的计数器值。
   - `stream` 的名字：
     - 局内的 12 条流：`RunRngType` 的名字。
     - 玩家的 3 条流：`player.` 加 `PlayerRngType` 的名字。
     - 怪物自己的 `Rng`：`monster.<combat_id>`。
     - 其他：`other`。
     - 用反向映射（`Rng` 实例 → 名字）实现：在进入战斗前建好，怪物生成后补上。
   - `caller`：调用栈上第一个不属于 `MegaCrit.Sts2.Core.Random` 命名空间的帧，格式为 `类型名.方法名`。async 状态机的帧要还原成原方法名（`<Foo>d__12.MoveNext` → `Foo`）。
2. **状态记录补全**：在 `state` 行里加入以下内容，字段名用 snake_case：
   - 敌人：意图，即 `NextMove` 的每个意图的类型，攻击类意图还要有显示伤害和段数。
   - 手牌：每张牌当前的能量费用。
   - 玩家：遗物列表（id 和计数器，如果有）、药水槽位（id 或 null）、充能球（id 列表）。
3. **批量**：`combat` 支持 `--seeds A,B,C`、`--characters IRONCLAD,SILENT`、`--policy-seeds 1,2`，对它们的笛卡尔积逐场运行。
   - 每场开头输出 `{"event":"begin","seed":...,"character":...,"policy_seed":...}`。
   - 场与场之间用 `RunManager.Instance.CleanUp()` 清理，参照 `Oracle.cs` 里 `newrun` 的批量写法。
4. **自检脚本** `tests/l3/check_ledger.py`：读 oracle 的输出，检查每条流在相邻两个 `state` 之间的抽取条数，和 `state` 里记录的计数器差值一致。
5. **清理**：删掉 `Oracle.cs` 里已经没用的 `godotprobe` 诊断命令。
6. 写执行报告 `docs/reports/2026-09-24-m1c-oracle-rng-ledger.md`。

## 验收

- `scripts/check.sh` 全部通过。L2 不能受影响：账本默认关闭，打开后 `newrun` 的输出也不能改变。
- 对 3 个种子 × IRONCLAD、SILENT × 2 个策略种子运行 `combat --rng-ledger`：
  - 全部正常结束；
  - `check_ledger.py` 通过；
  - 同样的参数跑两次，输出逐字节相同。
- 报告里附上运行命令和结果摘要。

## 不做什么

- 不改 C++ 代码。
- 不改 `tools/Sts2Headless/Headless.cs` 里的 TestMode 处理（另一个计划在审计它）。
- 不做覆盖率统计、合法动作比对，那些是后续工作。
