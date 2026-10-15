# 通用 Env：Vakuu 与先古访问历史执行报告

日期：2026-09-27。分支：`feat/generic-env-rf-api`。原版依据：v0.111.0 `Models/Events/Vakuu.cs`、`Saves/AncientStats.cs`、`Extensions/ListExtensions.cs`，以及直接运行游戏 DLL 的 `Sts2Oracle`。计划：[Vakuu 与先古访问历史](../plans/2026-09-27-env-vakuu-ancient-progress.md)。

## 前置与改动

Battleworn Dummy 和父事件恢复后，一条高生命值铁甲战士路线已到胜利；追加四条三幕路线时，两条停在 `Events.Vakuu.CalculateVars`，尚无状态分歧。

- `Vakuu` 按原版依序构造三组共十个遗物实例，以事件 RNG 分别不稳定洗牌并提供每组第一项；Sere Talon 的 9 点最大生命值提示保留。
- `Visits` 读取按先古事件和角色索引的 POD 档案访问次数，然后加一。输入放在 `NewRunOptions`，进入 `RunData`、生成状态字段、快照和导入校验。
- 专门测试检查非零、跨角色、跨先古历史，遗物构造顺序、三组选项以及七次事件 RNG 抽取。它检验游戏源码规定的不变量；实际路线另外由正版 oracle 差分验证。

## 验证

| 检查 | 结果 |
|---|---|
| `python3 tools/gen/gen_state_fields.py --check`；`python3 tools/gen/gen_dispatch.py --check`；`git diff --check` | 通过 |
| `ctest --test-dir build -R '^(vakuu_event|env_snapshot|env_m3|env_preview)$' --output-on-failure` | 4/4 通过 |
| `scripts/check.sh` | 全部通过：15/15 CTest；L2 200局零分歧；L3 80战、1611状态零分歧；默认 L4 80局、4089决策零分歧；聚焦遗物 428决策；严格二幕 3246决策；三幕胜利1138决策；Pael's Wing 219决策 |
| `DOTNET=~/.dotnet/dotnet python3 tests/l4/l4_run.py --seeds SEED0002,SEED0003 --policy-seeds 1,2 --no-neow --hp 10000 --stop victory --verify-snapshots --require-complete` | 四局中两局完整一致；另外两局越过 Vakuu 后在 `Relics.WhisperingEarring.ModifyMaxEnergy` 受控停止；已对齐 2513/3520 决策，零分歧 |

## 仍需完成

Whispering Earring 的能量修改与首回合自动出牌属于下一批真实可达规则，不能把 Vakuu 的成功视为四条路线完成。当前 C++ `NewRunOptions` 可接收非零访问历史，但 C ABI/Python reset 参数尚未暴露它；此轮 oracle 的内存存档是零历史基线。真实档案捕获与完全支持仍属 E1/E4 工作。CombatSolver 保持冻结。
